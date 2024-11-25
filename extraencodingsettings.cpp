// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "extraencodingsettings.h"
#include "behaviorsettingswidget.h"

#include <utils/settingsutils.h>

#include <QLatin1String>
#include <QSettings>

// Keep this for compatibility reasons.
static const char kGroupPostfix[] = "EditorManager";
static const char kUtf8BomBehaviorKey[] = "Utf8BomBehavior";
static const char kDefaultCharset[] = "DefaultCharset";

using namespace TextEditor;

ExtraEncodingSettings::ExtraEncodingSettings() : m_utf8BomSetting(OnlyKeep),m_defaultCharset(QLatin1String("UTF-8")),m_lineEnding(System)
{}

ExtraEncodingSettings::~ExtraEncodingSettings() = default;

void ExtraEncodingSettings::toSettings(const QString &category, QSettings *s) const
{
    Q_UNUSED(category)

    Utils::toSettings(QLatin1String(kGroupPostfix), QString(), s, this);
}

void ExtraEncodingSettings::fromSettings(const QString &category, QSettings *s)
{
    Q_UNUSED(category)

    *this = ExtraEncodingSettings();
    Utils::fromSettings(QLatin1String(kGroupPostfix), QString(), s, this);
}


QJsonObject ExtraEncodingSettings::toJson(){

    return {
            {kUtf8BomBehaviorKey,m_utf8BomSetting},
            {kDefaultCharset,m_defaultCharset},
    };
}

void ExtraEncodingSettings::fromJson(const QJsonObject& data){
    if(data.contains(kUtf8BomBehaviorKey)){
        m_utf8BomSetting = static_cast<Utf8BomSetting>(data.find(kUtf8BomBehaviorKey)->toInt(OnlyKeep));
    }
    if(data.contains(kDefaultCharset)){
        m_defaultCharset = data.find(kDefaultCharset)->toString();
    }
}



QVariantMap ExtraEncodingSettings::toMap() const
{
    return {
        {kUtf8BomBehaviorKey, m_utf8BomSetting}
    };
}

void ExtraEncodingSettings::fromMap(const QVariantMap &map)
{
    m_utf8BomSetting = (Utf8BomSetting)map.value(kUtf8BomBehaviorKey, m_utf8BomSetting).toInt();
}

bool ExtraEncodingSettings::equals(const ExtraEncodingSettings &s) const
{
    return m_utf8BomSetting == s.m_utf8BomSetting;
}

QStringList ExtraEncodingSettings::lineTerminationModeNames()
{
    return {BehaviorSettingsWidget::tr("Unix (LF)"),
                BehaviorSettingsWidget::tr("Windows (CRLF)")};
}
