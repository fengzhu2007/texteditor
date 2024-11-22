// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "texteditorsettings.h"

#include "behaviorsettings.h"
#include "completionsettings.h"
#include "displaysettings.h"
#include "extraencodingsettings.h"
#include "fontsettings.h"
#include "icodestylepreferences.h"
#include "icodestylepreferencesfactory.h"
#include "marginsettings.h"
#include "storagesettings.h"
#include "tabsettings.h"
#include "texteditor.h"
#include "typingsettings.h"
#include "highlightersettings.h"
#include "commentssettings.h"


#include "core/icore.h"
#include "utils/qtcassert.h"

#include <QApplication>
#include <QDebug>

using namespace TextEditor::Constants;
using namespace TextEditor::Internal;

namespace TextEditor {
namespace Internal {

class TextEditorSettingsPrivate
{
    Q_DECLARE_TR_FUNCTIONS(TextEditor::TextEditorSettings)

public:
    FontSettings m_fontSettings;
    TypingSettings m_typeSettings;
    StorageSettings m_storageSettings;
    BehaviorSettings m_behaviorSettings;
    MarginSettings m_marginSettings;
    DisplaySettings m_displaySettings;
    CompletionSettings m_completionSettings;
    HighlighterSettings m_highlighterSettings;
    ExtraEncodingSettings m_extraEncodingSettings;
    CommentsSettings m_commentsSettings;

    QMap<Utils::Id, ICodeStylePreferencesFactory *> m_languageToFactory;

    QMap<Utils::Id, ICodeStylePreferences *> m_languageToCodeStyle;
    QMap<Utils::Id, CodeStylePool *> m_languageToCodeStylePool;
    QMap<QString, Utils::Id> m_mimeTypeToLanguage;

    void initFontSetting(){
        //creator-dark.xml
        //m_fontSettings.loadColorScheme(":/resource/styles/default_light.xml",initialFormats());
        m_fontSettings.loadColorScheme(QLatin1String("Default"));
    }

};



} // namespace Internal


static TextEditorSettingsPrivate *d = nullptr;
static TextEditorSettings *m_instance = nullptr;

TextEditorSettings::TextEditorSettings()
{
    QTC_ASSERT(!m_instance, return);
    m_instance = this;
    d = new Internal::TextEditorSettingsPrivate;
    d->initFontSetting();
}

TextEditorSettings::~TextEditorSettings()
{
    delete d;

    m_instance = nullptr;
}

TextEditorSettings *TextEditorSettings::instance()
{
    return m_instance;
}

const FontSettings &TextEditorSettings::fontSettings()
{
    return d->m_fontSettings;
}

const TypingSettings &TextEditorSettings::typingSettings()
{
    return d->m_typeSettings;
}

const StorageSettings &TextEditorSettings::storageSettings()
{
    return d->m_storageSettings;
}

const BehaviorSettings &TextEditorSettings::behaviorSettings()
{
    return d->m_behaviorSettings;
}

const MarginSettings &TextEditorSettings::marginSettings()
{
    return d->m_marginSettings;
}

const DisplaySettings &TextEditorSettings::displaySettings()
{
    return d->m_displaySettings;
}

const CompletionSettings &TextEditorSettings::completionSettings()
{
    return d->m_completionSettings;
}

const HighlighterSettings &TextEditorSettings::highlighterSettings()
{
    return d->m_highlighterSettings;
}

const ExtraEncodingSettings &TextEditorSettings::extraEncodingSettings()
{
    return d->m_extraEncodingSettings;
}

const CommentsSettings &TextEditorSettings::commentsSettings()
{
    return d->m_commentsSettings;
}

void TextEditorSettings::registerCodeStyleFactory(ICodeStylePreferencesFactory *factory)
{
    d->m_languageToFactory.insert(factory->languageId(), factory);
}

void TextEditorSettings::unregisterCodeStyleFactory(Utils::Id languageId)
{
    d->m_languageToFactory.remove(languageId);
}

const QMap<Utils::Id, ICodeStylePreferencesFactory *> &TextEditorSettings::codeStyleFactories()
{
    return d->m_languageToFactory;
}

ICodeStylePreferencesFactory *TextEditorSettings::codeStyleFactory(Utils::Id languageId)
{
    return d->m_languageToFactory.value(languageId);
}

/*ICodeStylePreferences *TextEditorSettings::codeStyle()
{
     return nullptr;
}

ICodeStylePreferences *TextEditorSettings::codeStyle(Utils::Id languageId)
{
    return d->m_languageToCodeStyle.value(languageId, codeStyle());
}*/

QMap<Utils::Id, ICodeStylePreferences *> TextEditorSettings::codeStyles()
{
    return d->m_languageToCodeStyle;
}

void TextEditorSettings::registerCodeStyle(Utils::Id languageId, ICodeStylePreferences *prefs)
{
    d->m_languageToCodeStyle.insert(languageId, prefs);
}

void TextEditorSettings::unregisterCodeStyle(Utils::Id languageId)
{
    d->m_languageToCodeStyle.remove(languageId);
}

/*CodeStylePool *TextEditorSettings::codeStylePool()
{
     return nullptr;
}*/

CodeStylePool *TextEditorSettings::codeStylePool(Utils::Id languageId)
{
    return d->m_languageToCodeStylePool.value(languageId);
}

void TextEditorSettings::registerCodeStylePool(Utils::Id languageId, CodeStylePool *pool)
{
    d->m_languageToCodeStylePool.insert(languageId, pool);
}

void TextEditorSettings::unregisterCodeStylePool(Utils::Id languageId)
{
    d->m_languageToCodeStylePool.remove(languageId);
}

void TextEditorSettings::registerMimeTypeForLanguageId(const char *mimeType, Utils::Id languageId)
{
    d->m_mimeTypeToLanguage.insert(QString::fromLatin1(mimeType), languageId);
}

Utils::Id TextEditorSettings::languageId(const QString &mimeType)
{
    return d->m_mimeTypeToLanguage.value(mimeType);
}

static void setFontZoom(int zoom)
{
    d->m_fontSettings.setFontZoom(zoom);
    d->m_fontSettings.toSettings(Core::ICore::settings());
    emit m_instance->fontSettingsChanged(d->m_fontSettings);
}

int TextEditorSettings::increaseFontZoom(int step)
{
    const int previousZoom = d->m_fontSettings.fontZoom();
    const int newZoom = qMin(qMax(20, previousZoom + step),400);//min 20 max 400
    if (newZoom != previousZoom)
        setFontZoom(newZoom);
    return newZoom;
}

void TextEditorSettings::resetFontZoom()
{
    setFontZoom(100);
}

int TextEditorSettings::setZoom(int zoom){
    zoom = qMin(qMax(20, zoom),400);//min 20 max 400
    setFontZoom(zoom);
    return zoom;
}

} // TextEditor
