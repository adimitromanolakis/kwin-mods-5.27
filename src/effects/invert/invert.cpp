/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "invert.h"

#include <QAction>
#include <QFile>
#include <kwinglutils.h>
#include <kwinglplatform.h>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QStandardPaths>
#include <QDir>
#include <QDBusConnection>
#include <QDBusMessage>

#include <KNotification>
#include <QUrl>
//#include <QQmlEngine>
//#include <QQmlComponent>
//#include <QQuickWindow>
//#include <QQuickItem>
//#include <QQmlApplicationEngine>

#include "ShaderBuilder.h"
#include "ShaderBuilder.cpp"

/*
 qdbus org.kde.KWin /org/kde/KWin/InvertEffect org.kde.KWin.InvertEffect.setParameter method 5
 qdbus org.kde.KWin /org/kde/KWin/InvertEffect org.kde.KWin.InvertEffect.setParameter saturation_mix 0.8
qdbus org.kde.KWin /org/kde/KWin/InvertEffect org.kde.KWin.InvertEffect.getConfiguration
 qdbus org.kde.KWin /org/kde/KWin/InvertEffect org.kde.KWin.InvertEffect.getCompileLog

 */

Q_LOGGING_CATEGORY(KWIN_INVERT, "kwin_effect_invert", QtWarningMsg)

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(invert);
}

namespace KWin
{

InvertEffect::InvertEffect()
    :   m_inited(false),
        m_valid(true),
        m_shader(nullptr),
        m_allWindows(false)
{
    settings = new QSettings("apdim9", "shaders");
    qWarning() << "XX Settings filename:" << settings->fileName();
    
    QFileInfo fi(settings->fileName() );
    settingsDirectory = fi.absolutePath();
    qWarning() << "XX Settings base dir:"  << fi.absolutePath() << " " << fi.dir(); // "C:/temp/foo"
    
    sendNotification("Screen inversion loading");

    //ShaderBuilder ss; ss.test();

    if(!settings->contains("shader_path")) {
        settings->setValue("shader_path", QDir::home().filePath("invert.frag") );
        settings->sync();
    }

    // Load the shader parameters from the configuration
    bool ok = true;
    for( auto k : ss.m_parameters    ) ok = ok && settings->contains(k.name);
    for( auto k : ss.m_parameters_int) ok = ok && settings->contains(k.name);

    for( auto k : ss.m_parameters_int) 
        qWarning() << " XX PARAM: " << k.name << " " << k.value << " " << settings->value(k.name,"-999");

    for( auto k : ss.m_parameters) 
        qWarning() << " XX PARAM: " << k.name << " " << k.value << " " << settings->value(k.name,"-999");
    
    
    if(! ok) { writeConfiguration(); }

    // Load with default values if not previously set
    for( auto & k : ss.m_parameters) {
        k.value = settings->value(k.name, k.value).toDouble();
    }

    for( auto & k : ss.m_parameters_int) {
        k.value = settings->value(k.name, k.value).toInt();
    }

    ss.printParams();


    QAction* a = new QAction(this);
    a->setObjectName(QStringLiteral("Invert"));
    a->setText(i18n("Toggle Invert Effect"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_I);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_I);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::META + Qt::Key_I, a);
    connect(a, &QAction::triggered, this, &InvertEffect::toggleScreenInversion);

    QAction* b = new QAction(this);
    b->setObjectName(QStringLiteral("InvertWindow"));
    b->setText(i18n("Toggle Invert Effect on Window"));
    KGlobalAccel::self()->setDefaultShortcut(b, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_U);
    KGlobalAccel::self()->setShortcut(b, QList<QKeySequence>() << Qt::CTRL + Qt::META + Qt::Key_U);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::META + Qt::Key_U, b);
    connect(b, &QAction::triggered, this, &InvertEffect::toggleWindow);

    connect(effects, &EffectsHandler::windowClosed, this, &InvertEffect::slotWindowClosed);

    registerDBus();

}


InvertEffect::~InvertEffect()
{
    delete m_shader;
    delete settings;
}

bool InvertEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}


bool InvertEffect::checkShader(QStringList& lines, QString shaderName) 
{
    QString fragmentMod = lines.join("\n");

    //QFile file("/tmp/shader2.txt");
    QFile file( QDir(settingsDirectory).filePath("shader_mod.txt") );

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(fragmentMod.toUtf8());
        file.close();
    }

    QByteArray fragmentModArray = fragmentMod.toUtf8();
    const char *source = fragmentModArray.constData();
    //  const char* source = fragmentMod.toLatin1().constData();


    // printf("Source=%s\n",source);
    qCCritical(KWIN_INVERT) << "Shader length:" << strlen(source) <<  "   lines length :" << lines.length(); 

    const char *vertexShaderStrings[1];
    vertexShaderStrings[0] = (char*)source;

    // Attempt to compile the shader
    GLuint testShader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(testShader, 1, vertexShaderStrings, nullptr);
    glCompileShader(testShader);

    GLint compileStatus;
    glGetShaderiv(testShader, GL_COMPILE_STATUS, &compileStatus);

    GLint logLength;
    glGetShaderiv(testShader, GL_INFO_LOG_LENGTH, &logLength);
    QByteArray log(logLength, 0);
    glGetShaderInfoLog(testShader, logLength, nullptr, log.data());
    
    shaderCompileLog = log;

    if (compileStatus != GL_TRUE) {

        qCCritical(KWIN_INVERT) << "Shader compilation failed:\n" << log;
        
        sendNotification("Shader compilation failed.\n" + QString(log) );

        glDeleteShader(testShader);
        return false;
    }

    qCCritical(KWIN_INVERT) << "Shader compiled with no errors:" << log;

    glDeleteShader(testShader);
    return true;
}


bool InvertEffect::loadData()
{
    ensureResources();
    
    // Construct the full path to the shader file
    QString filePath = settings->value("shader_path").toString();//, (QString)(QDir::homePath() + "/invert.frag") ).toString();

    const qint64 coreVersionNumber = GLPlatform::instance()->isGLES() ? kVersionNumber(3, 0) : kVersionNumber(1, 40);

    if (GLPlatform::instance()->glslVersion() >= coreVersionNumber) {
        filePath = filePath.replace(QStringLiteral(".frag"), QStringLiteral("_core.frag"));
    }

    qCCritical(KWIN_INVERT) <<  "loading shader from path: " << filePath;

    QByteArray fragmentSource = readFile(filePath);

    sendNotification("Loading shader "+ filePath);

    //QByteArray v = ShaderManager::instance()->generateVertexSource(ShaderTrait::MapTexture);
    //qCCritical(KWIN_INVERT) << "ZZZ Vertex shader: " << v.data();

    QStringList fragLines = ((QString)fragmentSource).split('\n');

    // Update parameters on shader from the configuration
    ss.printParams();
    for( auto k : ss.m_parameters_int) ss.setParameter(fragLines, k.name, "int", k.value);
    for( auto k : ss.m_parameters    ) ss.setParameter(fragLines, k.name, "float", k.value);

    // ss.disableShader(fragLines, "FILM_NOISE");

    QString fragmentMod = fragLines.join("\n");

    // Write fragmentMod to /tmp/shader.txt
    QFile file("/tmp/shader.txt");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(fragmentMod.toUtf8());
        file.close();
    }

    if(! checkShader(fragLines, "invert") ) {
        qCCritical(KWIN_INVERT) << "The shader failed to load!";
        m_valid = false;
        m_inited = false;
        return false;
    }

    // m_shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QString(), filePath);
    
    m_shader = ShaderManager::instance()->generateCustomShader(
        ShaderTrait::MapTexture,
        ShaderManager::instance()->generateVertexSource(ShaderTrait::MapTexture),
        QByteArray(fragmentMod.toStdString().c_str())
    );

    if (!m_shader->isValid()) {
        qCCritical(KWIN_INVERT) << "the shader failed to load!";
        m_valid = false;
        m_inited = false;
        return false;
    }

    
    //QByteArray aa = ShaderManager::instance()->generateFragmentSource(ShaderTrait::MapTexture|ShaderTrait::Modulate);
    //qCCritical(KWIN_INVERT) << "ZZZZ frag shader: " << aa.data();

    m_inited = true;
    m_valid = true;

    return true;
}

bool InvertEffect::reLoadData()
{
    if(m_inited) {
        delete m_shader;
    }

    return loadData();
}


void InvertEffect::drawWindow(EffectWindow* w, int mask, const QRegion &region, WindowPaintData& data)
{
    // Load if we haven't already
    if (m_valid && !m_inited)
        m_valid = loadData();

    bool useShader = m_valid && (m_allWindows != m_windows.contains(w));
    if (useShader) {
        ShaderManager *shaderManager = ShaderManager::instance();
        shaderManager->pushShader(m_shader);

        data.shader = m_shader;
    }

    effects->drawWindow(w, mask, region, data);

    if (useShader) {
        ShaderManager::instance()->popShader();
    }
}

void InvertEffect::paintEffectFrame(KWin::EffectFrame* frame, const QRegion &region, double opacity, double frameOpacity)
{
    if (m_valid && m_allWindows) {
        frame->setShader(m_shader);
        ShaderBinder binder(m_shader);
        effects->paintEffectFrame(frame, region, opacity, frameOpacity);
    } else {
        effects->paintEffectFrame(frame, region, opacity, frameOpacity);
    }
}

void InvertEffect::slotWindowClosed(EffectWindow* w)
{
    m_windows.removeOne(w);
}

void InvertEffect::toggleScreenInversion()
{
    m_allWindows = !m_allWindows;
    if(m_allWindows) reLoadData();

    effects->addRepaintFull();
}

void InvertEffect::toggleWindow()
{
    if (!effects->activeWindow()) {
        return;
    }
    if (!m_windows.contains(effects->activeWindow()))
        m_windows.append(effects->activeWindow());
    else
        m_windows.removeOne(effects->activeWindow());
    effects->activeWindow()->addRepaintFull();
}

bool InvertEffect::isActive() const
{
    return m_valid && (m_allWindows || !m_windows.isEmpty());
}

bool InvertEffect::provides(Feature f)
{
    return f == ScreenInversion;
}

void InvertEffect::setShaderPath(QString value)
{
    settings->setValue("shader_path", value);
    settings->sync();
    return;
}

void InvertEffect::setParameter(QString param, double value)
{
    int update = 0;


    for( auto & k : ss.m_parameters_int) {
        if(k.name == param) {
           qCWarning(KWIN_INVERT) << "XXXX PARAM Setting " << param << " to:" << value;
           k.value = (int)value;
           update = 1;
        }
    }

    for( auto & k : ss.m_parameters) {
        if(k.name == param) {
           qCWarning(KWIN_INVERT) << "XXXX PARAM  Setting " << param << " to:" << value;
           k.value = value;
           update = 1;
        }
    }

    if(update) {
        m_allWindows = false;
        toggleScreenInversion();
    }
}



QString InvertEffect::getConfiguration()
{
    QString a;

    for( auto & k : ss.m_parameters_int) {
        a += QString("%1=%2\n").arg(k.name).arg(k.value);
    }        

    for( auto & k : ss.m_parameters) {
        a += QString("%1=%2\n").arg(k.name).arg(k.value);
    }        

    return a;
}


void
InvertEffect::writeConfiguration()
{
    for( auto k : ss.m_parameters) {
        settings->setValue(k.name, k.value);
    }

    for( auto k : ss.m_parameters_int) {
        settings->setValue(k.name, k.value);
    }

    settings->sync();
}



void InvertEffect::registerDBus()
{
    qCWarning(KWIN_INVERT) << "Register DBUS org.kde.KWin.InvertEffect!";
        
    QString serviceName = QStringLiteral("org.kde.KWin.InvertEffect");

    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/org/kde/KWin/InvertEffect"),
        serviceName,
        this,
        QDBusConnection::ExportScriptableContents);

    QDBusConnection::sessionBus().registerService(serviceName);
}


QString InvertEffect::getCompileLog()
{ 
    return shaderCompileLog;
}

QByteArray InvertEffect::readFile(const QString &filePath)
{
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            return file.readAll();
        }
        qCCritical(KWIN_INVERT) << "Failed to read file " << filePath;
        return QByteArray();
}

void InvertEffect::sendNotification(const QString &message)
{
    KNotification::event(QStringLiteral("graphicsreset"), "", message, "kblocks");
}

} // namespace

#include "invert.moc"
