#include "ShaderBuilder.h"
#include <QAction>
#include <QFile>
#include <kwinglutils.h>
#include <kwinglplatform.h>
#include <KLocalizedString>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QRegularExpression>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(TEST, "ShaderBuilder", QtWarningMsg)


void 
ShaderBuilder::setParameter(QStringList& lines, QString paramName, QString type, double value)
{
    QRegularExpression param0("const " + type + " " + paramName + " ");
    QString replace0 = QString("const " + type + " " + paramName + " = ") + QString::number(value) + QStringLiteral("; // modified parameter");

    
    for ( int i = 0; i < lines.size(); i++ ) {
        // Match the regex against the current line
        QRegularExpressionMatch match = param0.match(lines[i]);

        if (match.hasMatch()) {
            QString captured = match.captured(0);
            qWarning() << "Match found:" << captured;
            lines[i] = replace0;
        }
    }
}


void 
ShaderBuilder::disableShader(QStringList& lines, QString shaderName)
{
    int i1 = lines.indexOf(QRegularExpression("// " + shaderName + ":BEGIN") );
    int i2 = lines.indexOf(QRegularExpression("// " + shaderName + ":END") );
    
    qWarning() << "YYYY i1=" << i1 << " i2=" << i2;

    QStringList newLines;
    if( i1 == -1 || i2 == -1) {
        return;
    }

    for(int i = 0; i < lines.size(); i++) 
        if(i < i1 || i > i2)
            newLines.append(lines[i]);

    lines = newLines;
}





QString ShaderBuilder::test()
{
        QString filePath = QDir::homePath() + "/invert_core.frag";

        QFile file(filePath);
        QByteArray data;

        if (file.open(QIODevice::ReadOnly)) {
            data = file.readAll();
        } else {
            qCCritical(TEST) << "Failed to read shader " << filePath;
        }

        QStringList shaderLines = ((QString)data).split('\n');

        setParameter(shaderLines, "static_saturation", "float", 0.001);
        setParameter(shaderLines, "method", "float", 1);

        disableShader(shaderLines, "TECHNICOLOR2");

        QString shaderString = shaderLines.join("\n");

        //shaderString = shaderString.replace(QRegularExpression("^const.*test_param =.*$"), "test_param = 55555;");

        qWarning() << "YYY shaderString=" << shaderString.toStdString().c_str();

        return shaderString;
}
