/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_INVERT_H
#define KWIN_INVERT_H

#include <kwineffects.h>
#include <QDBusContext>
#include <QDBusUnixFileDescriptor>
#include <QSettings>
#include "ShaderBuilder.h"
#include <KNotification>


namespace KWin
{

class GLShader;

/**
 * Inverts desktop's colors
 */
class InvertEffect
    : public Effect, public QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.InvertEffect")

public:
    InvertEffect();
    ~InvertEffect() override;

    void drawWindow(EffectWindow* w, int mask, const QRegion &region, WindowPaintData& data) override;
    void paintEffectFrame(KWin::EffectFrame* frame, const QRegion &region, double opacity, double frameOpacity) override;
    bool isActive() const override;
    bool provides(Feature) override;

    int requestedEffectChainPosition() const override;

    static bool supported();
    bool reLoadData();

public Q_SLOTS:
    Q_SCRIPTABLE void toggleScreenInversion();
    void toggleWindow();
    void slotWindowClosed(KWin::EffectWindow *w);

    Q_SCRIPTABLE void setParameter(QString param, double value);
    Q_SCRIPTABLE void setShaderPath(QString value);
    Q_SCRIPTABLE QString getConfiguration();
    Q_SCRIPTABLE void writeConfiguration();
    Q_SCRIPTABLE QString getCompileLog();

protected:
    bool loadData();
    QByteArray readFile(const QString &filePath);
    void registerDBus();
    ShaderBuilder ss;

private:
    bool m_inited;
    bool m_valid;
    GLShader* m_shader;
    bool m_allWindows;
    QList<EffectWindow*> m_windows;

    QSettings *settings;
    QString settingsDirectory;
    QString shaderCompileLog;

    void sendNotification(const QString &message);
    bool checkShader(QStringList& lines, QString shaderName);

};

inline int InvertEffect::requestedEffectChainPosition() const
{
    return 99;
}

} // namespace

#endif
