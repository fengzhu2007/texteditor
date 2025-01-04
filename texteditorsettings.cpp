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
        //m_fontSettings.loadColorScheme(QLatin1String("Default"));
        m_fontSettings.loadColorScheme(QLatin1String("Dark"));
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

void TextEditorSettings::setFontSettings(const FontSettings& settings){
    d->m_fontSettings = settings;
}

void TextEditorSettings::setTypingSettings(const TypingSettings& settings){
    d->m_typeSettings = settings;
}

void TextEditorSettings::setStorageSettings(const StorageSettings& settings){
    d->m_storageSettings = settings;
}

void TextEditorSettings::setBehaviorSettings(const BehaviorSettings& settings){
    d->m_behaviorSettings = settings;
}

void TextEditorSettings::setMarginSettings(const MarginSettings& settings){
    d->m_marginSettings = settings;
}

void TextEditorSettings::setDisplaySettings(const DisplaySettings& settings){
    d->m_displaySettings = settings;
}

void TextEditorSettings::setCompletionSettings(const CompletionSettings& settings){
    d->m_completionSettings = settings;
}

void TextEditorSettings::setHighlighterSettings(const HighlighterSettings& settings){
    d->m_highlighterSettings = settings;
}

void TextEditorSettings::setExtraEncodingSettings(const ExtraEncodingSettings& settings){
    d->m_extraEncodingSettings = settings;
}

void TextEditorSettings::setCommentSettings(const CommentsSettings& settings){
    d->m_commentsSettings = settings;
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

QJsonObject TextEditorSettings::toJson(){

    return {
        {"font",d->m_fontSettings.toJson()},
        {"typing",d->m_typeSettings.toJson()},
        {"storage",d->m_storageSettings.toJson()},
        {"behavior",d->m_behaviorSettings.toJson()},
        {"margin",d->m_marginSettings.toJson()},
        {"display",d->m_displaySettings.toJson()},
        {"completion",d->m_completionSettings.toJson()},
        {"highlighter",d->m_highlighterSettings.toJson()},
        {"extraEncoding",d->m_extraEncodingSettings.toJson()},
        {"comments",d->m_commentsSettings.toJson()},
        {"tab",TabSettings().toJson()},
    };

}
void TextEditorSettings::fromJson(const QJsonObject& data){
    if(data.contains("font")){
        d->m_fontSettings.fromJson(data.find("font")->toObject());
    }
    if(data.contains("typing")){
        d->m_typeSettings.fromJson(data.find("typing")->toObject());
    }
    if(data.contains("storage")){
        d->m_storageSettings.fromJson(data.find("storage")->toObject());
    }
    if(data.contains("behavior")){
        d->m_behaviorSettings.fromJson(data.find("behavior")->toObject());
    }
    if(data.contains("margin")){
        d->m_marginSettings.fromJson(data.find("margin")->toObject());
    }
    if(data.contains("display")){
        d->m_displaySettings.fromJson(data.find("display")->toObject());
    }
    if(data.contains("completion")){
        d->m_completionSettings.fromJson(data.find("completion")->toObject());
    }
    if(data.contains("highlighter")){
        d->m_highlighterSettings.fromJson(data.find("highlighter")->toObject());
    }
    if(data.contains("extraEncoding")){
        d->m_extraEncodingSettings.fromJson(data.find("extraEncoding")->toObject());
    }
    if(data.contains("comments")){
        d->m_commentsSettings.fromJson(data.find("comments")->toObject());
    }
    if(data.contains("tab")){
        TabSettings::initGlobal(data.find("tab")->toObject());
    }
}

QString TextEditorSettings::name(){
    return QLatin1String("texteditor");
}

} // TextEditor
