// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "fontsettings.h"

#include "utils/fileutils.h"
#include "utils/hostosinfo.h"
//#include "utils/stringutils.h"
#include "utils/theme/theme.h"
#include "core/icore.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QSettings>
#include <QTextCharFormat>
#include <QApplication>

#include <cmath>

using namespace Utils;

const char fontFamilyKey[] = "FontFamily";
const char fontSizeKey[] = "FontSize";
const char fontZoomKey[] = "FontZoom";
const char lineSpacingKey[] = "LineSpacing";
const char antialiasKey[] = "FontAntialias";
const char schemeFileNamesKey[] = "ColorSchemes";

const bool DEFAULT_ANTIALIAS = true;

namespace TextEditor {

// ------- FormatDescription
FormatDescription::FormatDescription(TextStyle id,
                                     const QString &displayName,
                                     const QString &tooltipText,
                                     const QColor &foreground,
                                     FormatDescription::ShowControls showControls)
    : m_id(id),
    m_displayName(displayName),
    m_tooltipText(tooltipText),
    m_showControls(showControls)
{
    m_format.setForeground(foreground);
    m_format.setBackground(defaultBackground(id));
}

FormatDescription::FormatDescription(TextStyle id,
                                     const QString &displayName,
                                     const QString &tooltipText,
                                     const Format &format,
                                     FormatDescription::ShowControls showControls)
    : m_id(id),
    m_format(format),
    m_displayName(displayName),
    m_tooltipText(tooltipText),
    m_showControls(showControls)
{
}

FormatDescription::FormatDescription(TextStyle id,
                                     const QString &displayName,
                                     const QString &tooltipText,
                                     const QColor &underlineColor,
                                     const QTextCharFormat::UnderlineStyle underlineStyle,
                                     FormatDescription::ShowControls showControls)
    : m_id(id),
    m_displayName(displayName),
    m_tooltipText(tooltipText),
    m_showControls(showControls)
{
    m_format.setForeground(defaultForeground(id));
    m_format.setBackground(defaultBackground(id));
    m_format.setUnderlineColor(underlineColor);
    m_format.setUnderlineStyle(underlineStyle);
}

FormatDescription::FormatDescription(TextStyle id,
                                     const QString &displayName,
                                     const QString &tooltipText,
                                     FormatDescription::ShowControls showControls)
    : m_id(id),
    m_displayName(displayName),
    m_tooltipText(tooltipText),
    m_showControls(showControls)
{
    m_format.setForeground(defaultForeground(id));
    m_format.setBackground(defaultBackground(id));
}

QColor FormatDescription::defaultForeground(TextStyle id)
{
    if (id == C_TEXT) {
        return Qt::black;
    } else if (id == C_LINE_NUMBER) {
        const QPalette palette = Utils::Theme::initialPalette();
        const QColor bg = palette.window().color();
        if (bg.value() < 128)
            return palette.windowText().color();
        else
            return palette.dark().color();
    } else if (id == C_CURRENT_LINE_NUMBER) {
        const QPalette palette = Utils::Theme::initialPalette();
        const QColor bg = palette.window().color();
        if (bg.value() < 128)
            return palette.windowText().color();
        else
            return QColor();
    } else if (id == C_PARENTHESES) {
        return QColor(Qt::red);
    } else if (id == C_AUTOCOMPLETE) {
        return QColor(Qt::darkBlue);
    } else if (id == C_SEARCH_RESULT_ALT1) {
        return QColor(0x00, 0x00, 0x33);
    } else if (id == C_SEARCH_RESULT_ALT2) {
        return QColor(0x33, 0x00, 0x00);
    }
    return QColor();
}

QColor FormatDescription::defaultBackground(TextStyle id)
{
    if (id == C_TEXT) {
        return Qt::white;
    } else if (id == C_LINE_NUMBER) {
        return Utils::Theme::initialPalette().window().color();
    } else if (id == C_SEARCH_RESULT) {
        return QColor(0xffef0b);
    } else if (id == C_SEARCH_RESULT_ALT1) {
        return QColor(0xb6, 0xcc, 0xff);
    } else if (id == C_SEARCH_RESULT_ALT2) {
        return QColor(0xff, 0xb6, 0xcc);
    } else if (id == C_PARENTHESES) {
        return QColor(0xb4, 0xee, 0xb4);
    } else if (id == C_PARENTHESES_MISMATCH) {
        return QColor(Qt::magenta);
    } else if (id == C_AUTOCOMPLETE) {
        return QColor(192, 192, 255);
    } else if (id == C_CURRENT_LINE || id == C_SEARCH_SCOPE) {
        const QPalette palette = Utils::Theme::initialPalette();
        const QColor &fg = palette.color(QPalette::Highlight);
        const QColor &bg = palette.color(QPalette::Base);

        qreal smallRatio;
        qreal largeRatio;
        if (id == C_CURRENT_LINE) {
            smallRatio = .3;
            largeRatio = .6;
        } else {
            smallRatio = .05;
            largeRatio = .4;
        }
        const qreal ratio = ((palette.color(QPalette::Text).value() < 128) !=
                             (palette.color(QPalette::HighlightedText).value() < 128)) ? smallRatio : largeRatio;

        const QColor &col = QColor::fromRgbF(fg.redF() * ratio + bg.redF() * (1 - ratio),
                                             fg.greenF() * ratio + bg.greenF() * (1 - ratio),
                                             fg.blueF() * ratio + bg.blueF() * (1 - ratio));
        return col;
    } else if (id == C_SELECTION) {
        return Utils::Theme::initialPalette().color(QPalette::Highlight);
    } else if (id == C_OCCURRENCES) {
        return QColor(180, 180, 180);
    } else if (id == C_OCCURRENCES_RENAME) {
        return QColor(255, 100, 100);
    } else if (id == C_DISABLED_CODE) {
        return QColor(239, 239, 239);
    }
    return QColor(); // invalid color
}

bool FormatDescription::showControl(FormatDescription::ShowControls showControl) const
{
    return m_showControls & showControl;
}



// -- FontSettings
FontSettings::FontSettings() :
    m_family(defaultFixedFontFamily()),
    m_fontSize(defaultFontSize()),
    m_fontZoom(100),
    m_lineSpacing(100),
    m_antialias(DEFAULT_ANTIALIAS)
{
}

void FontSettings::clear()
{
    m_family = defaultFixedFontFamily();
    m_fontSize = defaultFontSize();
    m_fontZoom = 100;
    m_lineSpacing = 100;
    m_antialias = DEFAULT_ANTIALIAS;
    m_scheme.clear();
    clearCaches();
}

static QString settingsGroup()
{
    return {};
}

void FontSettings::toSettings(QSettings *s) const
{
    s->beginGroup(settingsGroup());
    if (m_family != defaultFixedFontFamily() || s->contains(QLatin1String(fontFamilyKey)))
        s->setValue(QLatin1String(fontFamilyKey), m_family);

    if (m_fontSize != defaultFontSize() || s->contains(QLatin1String(fontSizeKey)))
        s->setValue(QLatin1String(fontSizeKey), m_fontSize);

    if (m_fontZoom!= 100 || s->contains(QLatin1String(fontZoomKey)))
        s->setValue(QLatin1String(fontZoomKey), m_fontZoom);

    if (m_lineSpacing != 100 || s->contains(QLatin1String(lineSpacingKey)))
        s->setValue(QLatin1String(lineSpacingKey), m_lineSpacing);

    if (m_antialias != DEFAULT_ANTIALIAS || s->contains(QLatin1String(antialiasKey)))
        s->setValue(QLatin1String(antialiasKey), m_antialias);

    auto schemeFileNames = s->value(QLatin1String(schemeFileNamesKey)).toMap();
    if (m_schemeFileName != defaultSchemeFileName() || schemeFileNames.contains(Utils::creatorTheme()->id())) {
        schemeFileNames.insert(Utils::creatorTheme()->id(), m_schemeFileName.toVariant());
        s->setValue(QLatin1String(schemeFileNamesKey), schemeFileNames);
    }

    s->endGroup();
}

bool FontSettings::fromSettings(const FormatDescriptions &descriptions, const QSettings *s)
{
    clear();

    QString group = settingsGroup();
    if (!s->childGroups().contains(group))
        return false;

    group += QLatin1Char('/');

    m_family = s->value(group + QLatin1String(fontFamilyKey), defaultFixedFontFamily()).toString();
    m_fontSize = s->value(group + QLatin1String(fontSizeKey), m_fontSize).toInt();
    m_fontZoom= s->value(group + QLatin1String(fontZoomKey), m_fontZoom).toInt();
    m_lineSpacing = s->value(group + QLatin1String(lineSpacingKey), m_lineSpacing).toInt();
    m_antialias = s->value(group + QLatin1String(antialiasKey), DEFAULT_ANTIALIAS).toBool();

    if (s->contains(group + QLatin1String(schemeFileNamesKey))) {
        // Load the selected color scheme for the current theme
        auto schemeFileNames = s->value(group + QLatin1String(schemeFileNamesKey)).toMap();
        if (schemeFileNames.contains(Utils::creatorTheme()->id())) {
            const FilePath scheme = FilePath::fromVariant(schemeFileNames.value(Utils::creatorTheme()->id()));
            loadColorScheme(scheme, descriptions);
        }
    }

    return true;
}


QJsonObject FontSettings::toJson(){
    return {
        {fontFamilyKey,m_family},
        {fontSizeKey,m_fontSize},
        {fontZoomKey,m_fontZoom},
        {lineSpacingKey,m_lineSpacing},
        {antialiasKey,m_antialias},
        {schemeFileNamesKey,m_schemeFileName.toString()},
    };
}

void FontSettings::fromJson(const QJsonObject& data){
    if(data.contains(fontFamilyKey)){
        m_family = data.find(fontFamilyKey)->toString();
    }
    if(data.contains(fontSizeKey)){
        m_fontSize = data.find(fontSizeKey)->toInt(10);
    }
    if(data.contains(fontZoomKey)){
        m_fontZoom = data.find(fontZoomKey)->toInt(100);
    }
    if(data.contains(lineSpacingKey)){
        m_lineSpacing = data.find(lineSpacingKey)->toInt(100);
    }
    if(data.contains(antialiasKey)){
        m_antialias = data.find(antialiasKey)->toBool(true);
    }
    if(data.contains(schemeFileNamesKey)){
        const FilePath scheme = FilePath::fromString(data.find(schemeFileNamesKey)->toString());
        clearCaches();
        loadColorScheme(scheme, initialFormats());
    }
}

bool FontSettings::equals(const FontSettings &f) const
{
    return m_family == f.m_family
            && m_schemeFileName == f.m_schemeFileName
            && m_fontSize == f.m_fontSize
            && m_lineSpacing == f.m_lineSpacing
            && m_fontZoom == f.m_fontZoom
            && m_antialias == f.m_antialias
            && m_scheme == f.m_scheme;
}

auto qHash(const TextStyle &textStyle)
{
    return ::qHash(quint8(textStyle));
}

static bool isOverlayCategory(TextStyle category)
{
    return category == C_OCCURRENCES
           || category == C_OCCURRENCES_RENAME
           || category == C_SEARCH_RESULT
           || category == C_SEARCH_RESULT_ALT1
           || category == C_SEARCH_RESULT_ALT2
           || category == C_PARENTHESES_MISMATCH;
}

/**
 * Returns the QTextCharFormat of the given format category.
 */
QTextCharFormat FontSettings::toTextCharFormat(TextStyle category) const
{
    auto textCharFormatIterator = m_formatCache.find(category);
    if (textCharFormatIterator != m_formatCache.end())
        return *textCharFormatIterator;

    const Format &f = m_scheme.formatFor(category);
    QTextCharFormat tf;

    if (category == C_TEXT) {
        tf.setFontFamily(m_family);
        tf.setFontPointSize(m_fontSize * m_fontZoom / 100.);
        tf.setFontStyleStrategy(m_antialias ? QFont::PreferAntialias : QFont::NoAntialias);
    }

    if (category == C_OCCURRENCES_UNUSED) {
        tf.setToolTip(QCoreApplication::translate("FontSettings_C_OCCURRENCES_UNUSED",
                                                  "Unused variable"));
    }

    if (f.foreground().isValid() && !isOverlayCategory(category))
        tf.setForeground(f.foreground());
    if (f.background().isValid()) {
        if (category == C_TEXT || f.background() != m_scheme.formatFor(C_TEXT).background())
            tf.setBackground(f.background());
    } else if (isOverlayCategory(category)) {
        // overlays without a background schouldn't get painted
        tf.setBackground(Qt::lightGray);
    }

    tf.setFontWeight(f.bold() ? QFont::Bold : QFont::Normal);
    tf.setFontItalic(f.italic());

    tf.setUnderlineColor(f.underlineColor());
    tf.setUnderlineStyle(f.underlineStyle());

    m_formatCache.insert(category, tf);
    return tf;
}

auto qHash(TextStyles textStyles)
{
    return ::qHash(reinterpret_cast<quint64&>(textStyles));
}

bool operator==(const TextStyles &first, const TextStyles &second)
{
    return first.mainStyle == second.mainStyle
        && first.mixinStyles == second.mixinStyles;
}

namespace {

double clamp(double value)
{
    return std::max(0.0, std::min(1.0, value));
}

QBrush mixBrush(const QBrush &original, double relativeSaturation, double relativeLightness)
{
    const QColor originalColor = original.color().toHsl();
    QColor mixedColor(QColor::Hsl);

    double mixedSaturation = clamp(originalColor.hslSaturationF() + relativeSaturation);

    double mixedLightness = clamp(originalColor.lightnessF() + relativeLightness);

    mixedColor.setHslF(originalColor.hslHueF(), mixedSaturation, mixedLightness);

    return mixedColor;
}
}

void FontSettings::addMixinStyle(QTextCharFormat &textCharFormat,
                                 const MixinTextStyles &mixinStyles) const
{
    for (TextStyle mixinStyle : mixinStyles) {
        const Format &format = m_scheme.formatFor(mixinStyle);

        if (format.foreground().isValid()) {
            textCharFormat.setForeground(format.foreground());
        } else {
            if (textCharFormat.hasProperty(QTextFormat::ForegroundBrush)) {
                textCharFormat.setForeground(mixBrush(textCharFormat.foreground(),
                                                      format.relativeForegroundSaturation(),
                                                      format.relativeForegroundLightness()));
            }
        }
        if (format.background().isValid()) {
            textCharFormat.setBackground(format.background());
        } else {
            if (textCharFormat.hasProperty(QTextFormat::BackgroundBrush)) {
                textCharFormat.setBackground(mixBrush(textCharFormat.background(),
                                                      format.relativeBackgroundSaturation(),
                                                      format.relativeBackgroundLightness()));
            }
        }
        if (!textCharFormat.fontItalic())
            textCharFormat.setFontItalic(format.italic());

        if (textCharFormat.fontWeight() == QFont::Normal)
            textCharFormat.setFontWeight(format.bold() ? QFont::Bold : QFont::Normal);

        if (textCharFormat.underlineStyle() == QTextCharFormat::NoUnderline) {
            textCharFormat.setUnderlineStyle(format.underlineStyle());
            textCharFormat.setUnderlineColor(format.underlineColor());
        }
    };
}

void FontSettings::clearCaches()
{
    m_formatCache.clear();
    m_textCharFormatCache.clear();
}

QTextCharFormat FontSettings::toTextCharFormat(TextStyles textStyles) const
{
    auto textCharFormatIterator = m_textCharFormatCache.find(textStyles);
    if (textCharFormatIterator != m_textCharFormatCache.end())
        return *textCharFormatIterator;

    QTextCharFormat textCharFormat = toTextCharFormat(textStyles.mainStyle);

    addMixinStyle(textCharFormat, textStyles.mixinStyles);

    m_textCharFormatCache.insert(textStyles, textCharFormat);

    return textCharFormat;
}

/**
 * Returns the list of QTextCharFormats that corresponds to the list of
 * requested format categories.
 */
QVector<QTextCharFormat> FontSettings::toTextCharFormats(const QVector<TextStyle> &categories) const
{
    QVector<QTextCharFormat> rc;
    const int size = categories.size();
    rc.reserve(size);
    for (int i = 0; i < size; i++)
         rc.append(toTextCharFormat(categories.at(i)));
    return rc;
}

/**
 * Returns the configured font family.
 */
QString FontSettings::family() const
{
    return m_family;
}

void FontSettings::setFamily(const QString &family)
{
    m_family = family;
    clearCaches();
}

/**
 * Returns the configured font size.
 */
int FontSettings::fontSize() const
{
    return m_fontSize;
}

void FontSettings::setFontSize(int size)
{
    m_fontSize = size;
    clearCaches();
}

/**
 * Returns the configured font zoom factor in percent.
 */
int FontSettings::fontZoom() const
{
    return m_fontZoom;
}

void FontSettings::setFontZoom(int zoom)
{
    m_fontZoom = zoom;
    clearCaches();
}

qreal FontSettings::lineSpacing() const
{
    QFont currentFont = font();
    currentFont.setPointSize(m_fontSize * m_fontZoom / 100);
    qreal spacing = QFontMetricsF(currentFont).lineSpacing();
    if (m_lineSpacing != 100)
        spacing *= qreal(m_lineSpacing) / 100;
    return spacing;
}

int FontSettings::relativeLineSpacing() const
{
    return m_lineSpacing;
}

void FontSettings::setRelativeLineSpacing(int relativeLineSpacing)
{
    m_lineSpacing = relativeLineSpacing;
}

QFont FontSettings::font() const
{
    QFont f(family(), fontSize());
    f.setStyleStrategy(m_antialias ? QFont::PreferAntialias : QFont::NoAntialias);
    return f;
}

/**
 * Returns the configured antialiasing behavior.
 */
bool FontSettings::antialias() const
{
    return m_antialias;
}

void FontSettings::setAntialias(bool antialias)
{
    m_antialias = antialias;
    clearCaches();
}

/**
 * Returns the format for the given font category.
 */
Format &FontSettings::formatFor(TextStyle category)

{
    return m_scheme.formatFor(category);
}

Format FontSettings::formatFor(TextStyle category) const
{
    return m_scheme.formatFor(category);
}

/**
 * Returns the file name of the currently selected color scheme.
 */
Utils::FilePath FontSettings::colorSchemeFileName() const
{
    return m_schemeFileName;
}

/**
 * Sets the file name of the color scheme. Does not load the scheme from the
 * given file. If you want to load a scheme, use loadColorScheme() instead.
 */
void FontSettings::setColorSchemeFileName(const Utils::FilePath &filePath)
{
    m_schemeFileName = filePath;
}

bool FontSettings::loadColorScheme(const Utils::FilePath &filePath,
                                   const FormatDescriptions &descriptions)
{
    clearCaches();

    bool loaded = true;
    m_schemeFileName = filePath;

    if (!m_scheme.load(m_schemeFileName)) {
        loaded = false;
        m_schemeFileName.clear();
        qWarning() << "Failed to load color scheme:" << filePath;
    }

    // Apply default formats to undefined categories
    for (const FormatDescription &desc : descriptions) {
        const TextStyle id = desc.id();
        if (!m_scheme.contains(id)) {
            if (id == C_NAMESPACE && m_scheme.contains(C_TYPE)) {
                m_scheme.setFormatFor(C_NAMESPACE, m_scheme.formatFor(C_TYPE));
                continue;
            }
            if (id == C_MACRO && m_scheme.contains(C_FUNCTION)) {
                m_scheme.setFormatFor(C_MACRO, m_scheme.formatFor(C_FUNCTION));
                continue;
            }
            Format format;
            const Format &descFormat = desc.format();
            // Default fallback for background and foreground is C_TEXT, which is set through
            // the editor's palette, i.e. we leave these as invalid colors in that case
            if (descFormat != format || !m_scheme.contains(C_TEXT)) {
                format.setForeground(descFormat.foreground());
                format.setBackground(descFormat.background());
            }
            format.setRelativeForegroundSaturation(descFormat.relativeForegroundSaturation());
            format.setRelativeForegroundLightness(descFormat.relativeForegroundLightness());
            format.setRelativeBackgroundSaturation(descFormat.relativeBackgroundSaturation());
            format.setRelativeBackgroundLightness(descFormat.relativeBackgroundLightness());
            format.setBold(descFormat.bold());
            format.setItalic(descFormat.italic());
            format.setUnderlineColor(descFormat.underlineColor());
            format.setUnderlineStyle(descFormat.underlineStyle());
            m_scheme.setFormatFor(id, format);
        }
    }

    return loaded;
}


bool FontSettings::loadColorScheme(const QString& name,const FormatDescriptions &descriptions){
    this->clearCaches();
    auto data = schemeMap();
    for(auto one:data){
        if(one.first==name){
            return loadColorScheme(Utils::FilePath::fromString(one.second),descriptions);
        }
    }
    return loadColorScheme(Utils::FilePath::fromString(data.first().second),descriptions);
}

bool FontSettings::loadColorScheme(const QString& name){

    return loadColorScheme(name,initialFormats());
}


bool FontSettings::saveColorScheme(const Utils::FilePath &fileName)
{
    const bool saved = m_scheme.save(fileName, Core::ICore::dialogParent());
    if (saved)
        m_schemeFileName = fileName;
    return saved;
}

/**
 * Returns the currently active color scheme.
 */
const ColorScheme &FontSettings::colorScheme() const
{
    return m_scheme;
}

void FontSettings::setColorScheme(const ColorScheme &scheme)
{
    m_scheme = scheme;
    clearCaches();
}

static QString defaultFontFamily()
{
    if (Utils::HostOsInfo::isMacHost())
        return QLatin1String("Monaco");

    const QString sourceCodePro("Source Code Pro");
    const QFontDatabase dataBase;
    if (dataBase.hasFamily(sourceCodePro))
        return sourceCodePro;

    if (Utils::HostOsInfo::isAnyUnixHost())
        return QLatin1String("Monospace");

    const QString courierNew("Courier New");
    if (dataBase.hasFamily(courierNew))
        return courierNew;

    return QLatin1String("Courier");
}

QString FontSettings::defaultFixedFontFamily()
{
    static QString rc;
    if (rc.isEmpty()) {
        QFont f = QFont(defaultFontFamily());
        f.setStyleHint(QFont::TypeWriter);
        rc = f.family();
    }
    return rc;
}

int FontSettings::defaultFontSize()
{
    if (Utils::HostOsInfo::isMacHost())
        return 12;
    if (Utils::HostOsInfo::isAnyUnixHost())
        return 9;
    return 10;
}

/**
 * Returns the default scheme file name, or the path to a shipped scheme when
 * one exists with the given \a fileName.
 */
FilePath FontSettings::defaultSchemeFileName(const QString &fileName)
{
    FilePath defaultScheme = Core::ICore::resourcePath("styles");

    if (!fileName.isEmpty() && (defaultScheme / fileName).exists()) {
        defaultScheme = defaultScheme / fileName;
    } else {
        const QString themeScheme = Utils::creatorTheme()->defaultTextEditorColorScheme();
        if (!themeScheme.isEmpty() && (defaultScheme / themeScheme).exists())
            defaultScheme = defaultScheme / themeScheme;
        else
            defaultScheme = defaultScheme / "default.xml";
    }

    return defaultScheme;
}

QList<QPair<QString,QString>> FontSettings::schemeMap(){

    return {
        {"Default",":/resource/styles/default_light.xml"},
        {"Dark",":/resource/styles/dark.xml"},
        {"Solarized Light",":/resource/styles/solarized-light.xml"},
        {"Solarized Dark",":/resource/styles/solarized-dark.xml"},
        {"Qt Creator Dark",":/resource/styles/creator-dark.xml"},
        {"Grayscale",":/resource/styles/grayscale.xml"},
        {"Inkpot",":/resource/styles/inkpot.xml"},
        {"IntelliJ IDEA",":/resource/styles/intellij.xml"},
        {"Mabakor",":/resource/styles/mabakor.xml"},
        {"Modnokai Night Shift v2",":/resource/styles/modnokai_night_shift_v2.xml"},





    };
}

FormatDescriptions FontSettings::initialFormats(){
    FormatDescriptions formatDescr;
    formatDescr.reserve(C_LAST_STYLE_SENTINEL);
    formatDescr.emplace_back(C_TEXT, QObject::tr("Text"),
                             QObject::tr("Generic text and punctuation tokens.\n"
                                "Applied to text that matched no other rule."),
                             Format{Qt::black, Qt::white});

    // Special categories
    const QPalette p = QApplication::palette();
    formatDescr.emplace_back(C_LINK, QObject::tr("Link"),
                             QObject::tr("Links that follow symbol under cursor."), Qt::blue);
    formatDescr.emplace_back(C_SELECTION, QObject::tr("Selection"), QObject::tr("Selected text."),
                             p.color(QPalette::HighlightedText));
    formatDescr.emplace_back(C_LINE_NUMBER, QObject::tr("Line Number"),
                             QObject::tr("Line numbers located on the left side of the editor."),
                             FormatDescription::ShowAllAbsoluteControlsExceptUnderline);
    formatDescr.emplace_back(C_SEARCH_RESULT, QObject::tr("Search Result"),
                             QObject::tr("Highlighted search results inside the editor."),
                             FormatDescription::ShowBackgroundControl);
    formatDescr.emplace_back(C_SEARCH_RESULT_ALT1, QObject::tr("Search Result (Alternative 1)"),
                             QObject::tr("Highlighted search results inside the editor.\n"
                                "Used to mark read accesses to C++ symbols."),
                             FormatDescription::ShowBackgroundControl);
    formatDescr.emplace_back(C_SEARCH_RESULT_ALT2, QObject::tr("Search Result (Alternative 2)"),
                             QObject::tr("Highlighted search results inside the editor.\n"
                                "Used to mark write accesses to C++ symbols."),
                             FormatDescription::ShowBackgroundControl);
    formatDescr.emplace_back(C_SEARCH_RESULT_CONTAINING_FUNCTION,
                             QObject::tr("Search Result Containing function"),
                             QObject::tr("Highlighted search results inside the editor.\n"
                                "Used to mark containing function of the symbol usage."),
                             FormatDescription::ShowForeAndBackgroundControl);
    formatDescr.emplace_back(C_SEARCH_SCOPE, QObject::tr("Search Scope"),
                             QObject::tr("Section where the pattern is searched in."),
                             FormatDescription::ShowBackgroundControl);
    formatDescr.emplace_back(C_PARENTHESES, QObject::tr("Parentheses"),
                             QObject::tr("Displayed when matching parentheses, square brackets "
                                "or curly brackets are found."));
    formatDescr.emplace_back(C_PARENTHESES_MISMATCH, QObject::tr("Mismatched Parentheses"),
                             QObject::tr("Displayed when mismatched parentheses, "
                                "square brackets, or curly brackets are found."));
    formatDescr.emplace_back(C_AUTOCOMPLETE, QObject::tr("Auto Complete"),
                             QObject::tr("Displayed when a character is automatically inserted "
                                "like brackets or quotes."));
    formatDescr.emplace_back(C_CURRENT_LINE, QObject::tr("Current Line"),
                             QObject::tr("Line where the cursor is placed in."),
                             FormatDescription::ShowBackgroundControl);

    FormatDescription currentLineNumber(C_CURRENT_LINE_NUMBER,
                                        QObject::tr("Current Line Number"),
                                        QObject::tr("Line number located on the left side of the "
                                           "editor where the cursor is placed in."),
                                        Qt::darkGray,
                                        FormatDescription::ShowAllAbsoluteControlsExceptUnderline);
    currentLineNumber.format().setBold(true);
    formatDescr.push_back(std::move(currentLineNumber));


    formatDescr.emplace_back(C_OCCURRENCES, QObject::tr("Occurrences"),
                             QObject::tr("Occurrences of the symbol under the cursor.\n"
                                "(Only the background will be applied.)"),
                             FormatDescription::ShowBackgroundControl);
    formatDescr.emplace_back(C_OCCURRENCES_UNUSED,
                             QObject::tr("Unused Occurrence"),
                             QObject::tr("Occurrences of unused variables."),
                             Qt::darkYellow,
                             QTextCharFormat::SingleUnderline);
    formatDescr.emplace_back(C_OCCURRENCES_RENAME, QObject::tr("Renaming Occurrence"),
                             QObject::tr("Occurrences of a symbol that will be renamed."),
                             FormatDescription::ShowBackgroundControl);

    // Standard categories
    formatDescr.emplace_back(C_NUMBER, QObject::tr("Number"), QObject::tr("Number literal."),
                             Qt::darkBlue);
    formatDescr.emplace_back(C_STRING, QObject::tr("String"),
                             QObject::tr("Character and string literals."), Qt::darkGreen);
    formatDescr.emplace_back(C_PRIMITIVE_TYPE, QObject::tr("Primitive Type"),
                             QObject::tr("Name of a primitive data type."), Qt::darkYellow);
    formatDescr.emplace_back(C_TYPE, QObject::tr("Type"), QObject::tr("Name of a type."),
                             Qt::darkMagenta);
    formatDescr.emplace_back(C_NAMESPACE, QObject::tr("Namespace"), QObject::tr("Name of a namespace."),
                             Qt::darkGreen);
    formatDescr.emplace_back(C_LOCAL, QObject::tr("Local"),
                             QObject::tr("Local variables."), QColor(9, 46, 100));
    formatDescr.emplace_back(C_PARAMETER, QObject::tr("Parameter"),
                             QObject::tr("Function or method parameters."), QColor(9, 46, 100));
    formatDescr.emplace_back(C_FIELD, QObject::tr("Field"),
                             QObject::tr("Class' data members."), Qt::darkRed);
    formatDescr.emplace_back(C_GLOBAL, QObject::tr("Global"),
                             QObject::tr("Global variables."), QColor(206, 92, 0));
    formatDescr.emplace_back(C_ENUMERATION, QObject::tr("Enumeration"),
                             QObject::tr("Applied to enumeration items."), Qt::darkMagenta);

    Format functionFormat;
    functionFormat.setForeground(QColor(0, 103, 124));
    formatDescr.emplace_back(C_FUNCTION, QObject::tr("Function"), QObject::tr("Name of a function."),
                             functionFormat);
    Format declarationFormat;
    declarationFormat.setBold(true);
    formatDescr.emplace_back(C_DECLARATION,
                             QObject::tr("Declaration"),
                             QObject::tr("Style adjustments to declarations."),
                             declarationFormat,
                             FormatDescription::ShowAllControls);
    formatDescr.emplace_back(C_FUNCTION_DEFINITION,
                             QObject::tr("Function Definition"),
                             QObject::tr("Name of function at its definition."),
                             FormatDescription::ShowAllControls);
    Format virtualFunctionFormat(functionFormat);
    virtualFunctionFormat.setItalic(true);
    formatDescr.emplace_back(C_VIRTUAL_METHOD, QObject::tr("Virtual Function"),
                             QObject::tr("Name of function declared as virtual."),
                             virtualFunctionFormat);

    formatDescr.emplace_back(C_BINDING, QObject::tr("QML Binding"),
                             QObject::tr("QML item property, that allows a "
                                "binding to another property."),
                             Qt::darkRed);



    Format jsLocalFormat;
    jsLocalFormat.setForeground(QColor(41, 133, 199)); // very light blue
    jsLocalFormat.setItalic(true);
    formatDescr.emplace_back(C_JS_SCOPE_VAR, QObject::tr("JavaScript Scope Var"),
                             QObject::tr("Variables defined inside the JavaScript file."),
                             jsLocalFormat);

    Format jsGlobalFormat;
    jsGlobalFormat.setForeground(QColor(0, 85, 175)); // light blue
    jsGlobalFormat.setItalic(true);
    formatDescr.emplace_back(C_JS_IMPORT_VAR, QObject::tr("JavaScript Import"),
                             QObject::tr("Name of a JavaScript import inside a QML file."),
                             jsGlobalFormat);
    formatDescr.emplace_back(C_JS_GLOBAL_VAR, QObject::tr("JavaScript Global Variable"),
                             QObject::tr("Variables defined outside the script."),
                             jsGlobalFormat);

    formatDescr.emplace_back(C_KEYWORD, QObject::tr("Keyword"),
                             QObject::tr("Reserved keywords of the programming language except "
                                "keywords denoting primitive types."), Qt::darkYellow);
    formatDescr.emplace_back(C_PUNCTUATION, QObject::tr("Punctuation"),
                             QObject::tr("Punctuation excluding operators."));
    formatDescr.emplace_back(C_OPERATOR, QObject::tr("Operator"),
                             QObject::tr("Non user-defined language operators.\n"
                                "To style user-defined operators, use Overloaded Operator."),
                             FormatDescription::ShowAllControls);
    formatDescr.emplace_back(C_OVERLOADED_OPERATOR,
                             QObject::tr("Overloaded Operators"),
                             QObject::tr("Calls and declarations of overloaded (user-defined) operators."),
                             functionFormat,
                             FormatDescription::ShowAllControls);
    formatDescr.emplace_back(C_PREPROCESSOR, QObject::tr("Preprocessor"),
                             QObject::tr("Preprocessor directives."), Qt::darkBlue);
    formatDescr.emplace_back(C_MACRO, QObject::tr("Macro"),
                             QObject::tr("Macros."), functionFormat);
    formatDescr.emplace_back(C_LABEL, QObject::tr("Label"), QObject::tr("Labels for goto statements."),
                             Qt::darkRed);
    formatDescr.emplace_back(C_COMMENT, QObject::tr("Comment"),
                             QObject::tr("All style of comments except Doxygen comments."),
                             Qt::darkGreen);
    formatDescr.emplace_back(C_DOXYGEN_COMMENT, QObject::tr("Doxygen Comment"),
                             QObject::tr("Doxygen comments."), Qt::darkBlue);
    formatDescr.emplace_back(C_DOXYGEN_TAG, QObject::tr("Doxygen Tag"), QObject::tr("Doxygen tags."),
                             Qt::blue);
    formatDescr.emplace_back(C_VISUAL_WHITESPACE, QObject::tr("Visual Whitespace"),
                             QObject::tr("Whitespace.\nWill not be applied to whitespace "
                                "in comments and strings."), Qt::lightGray);
    formatDescr.emplace_back(C_DISABLED_CODE, QObject::tr("Disabled Code"),
                             QObject::tr("Code disabled by preprocessor directives."));

    // Diff categories
    formatDescr.emplace_back(C_ADDED_LINE, QObject::tr("Added Line"),
                             QObject::tr("Applied to added lines in differences (in diff editor)."),
                             QColor(0, 170, 0));
    formatDescr.emplace_back(C_REMOVED_LINE, QObject::tr("Removed Line"),
                             QObject::tr("Applied to removed lines in differences (in diff editor)."),
                             Qt::red);
    formatDescr.emplace_back(C_DIFF_FILE, QObject::tr("Diff File"),
                             QObject::tr("Compared files (in diff editor)."), Qt::darkBlue);
    formatDescr.emplace_back(C_DIFF_LOCATION, QObject::tr("Diff Location"),
                             QObject::tr("Location in the files where the difference is "
                                "(in diff editor)."), Qt::blue);

    // New diff categories
    formatDescr.emplace_back(C_DIFF_FILE_LINE, QObject::tr("Diff File Line"),
                             QObject::tr("Applied to lines with file information "
                                "in differences (in side-by-side diff editor)."),
                             Format(QColor(), QColor(255, 255, 0)));
    formatDescr.emplace_back(C_DIFF_CONTEXT_LINE, QObject::tr("Diff Context Line"),
                             QObject::tr("Applied to lines describing hidden context "
                                "in differences (in side-by-side diff editor)."),
                             Format(QColor(), QColor(175, 215, 231)));
    formatDescr.emplace_back(C_DIFF_SOURCE_LINE, QObject::tr("Diff Source Line"),
                             QObject::tr("Applied to source lines with changes "
                                "in differences (in side-by-side diff editor)."),
                             Format(QColor(), QColor(255, 223, 223)));
    formatDescr.emplace_back(C_DIFF_SOURCE_CHAR, QObject::tr("Diff Source Character"),
                             QObject::tr("Applied to removed characters "
                                "in differences (in side-by-side diff editor)."),
                             Format(QColor(), QColor(255, 175, 175)));
    formatDescr.emplace_back(C_DIFF_DEST_LINE, QObject::tr("Diff Destination Line"),
                             QObject::tr("Applied to destination lines with changes "
                                "in differences (in side-by-side diff editor)."),
                             Format(QColor(), QColor(223, 255, 223)));
    formatDescr.emplace_back(C_DIFF_DEST_CHAR, QObject::tr("Diff Destination Character"),
                             QObject::tr("Applied to added characters "
                                "in differences (in side-by-side diff editor)."),
                             Format(QColor(), QColor(175, 255, 175)));

    formatDescr.emplace_back(C_LOG_CHANGE_LINE, QObject::tr("Log Change Line"),
                             QObject::tr("Applied to lines describing changes in VCS log."),
                             Format(QColor(192, 0, 0), QColor()));
    formatDescr.emplace_back(C_LOG_AUTHOR_NAME, QObject::tr("Log Author Name"),
                             QObject::tr("Applied to author names in VCS log."),
                             Format(QColor(0x007af4), QColor()));
    formatDescr.emplace_back(C_LOG_COMMIT_DATE, QObject::tr("Log Commit Date"),
                             QObject::tr("Applied to commit dates in VCS log."),
                             Format(QColor(0x006600), QColor()));
    formatDescr.emplace_back(C_LOG_COMMIT_HASH, QObject::tr("Log Commit Hash"),
                             QObject::tr("Applied to commit hashes in VCS log."),
                             Format(QColor(0xff0000), QColor()));
    formatDescr.emplace_back(C_LOG_DECORATION, QObject::tr("Log Decoration"),
                             QObject::tr("Applied to commit decorations in VCS log."),
                             Format(QColor(0xff00ff), QColor()));
    formatDescr.emplace_back(C_LOG_COMMIT_SUBJECT, QObject::tr("Log Commit Subject"),
                             QObject::tr("Applied to commit subjects in VCS log."),
                             Format{QColor{}, QColor{}});

    // Mixin categories
    formatDescr.emplace_back(C_ERROR,
                             QObject::tr("Error"),
                             QObject::tr("Underline color of error diagnostics."),
                             QColor(255,0, 0),
                             QTextCharFormat::SingleUnderline,
                             FormatDescription::ShowAllControls);
    formatDescr.emplace_back(C_ERROR_CONTEXT,
                             QObject::tr("Error Context"),
                             QObject::tr("Underline color of the contexts of error diagnostics."),
                             QColor(255,0, 0),
                             QTextCharFormat::DotLine,
                             FormatDescription::ShowAllControls);
    formatDescr.emplace_back(C_WARNING,
                             QObject::tr("Warning"),
                             QObject::tr("Underline color of warning diagnostics."),
                             QColor(255, 190, 0),
                             QTextCharFormat::SingleUnderline,
                             FormatDescription::ShowAllControls);
    formatDescr.emplace_back(C_WARNING_CONTEXT,
                             QObject::tr("Warning Context"),
                             QObject::tr("Underline color of the contexts of warning diagnostics."),
                             QColor(255, 190, 0),
                             QTextCharFormat::DotLine,
                             FormatDescription::ShowAllControls);
    Format outputArgumentFormat;
    outputArgumentFormat.setItalic(true);
    formatDescr.emplace_back(C_OUTPUT_ARGUMENT,
                             QObject::tr("Output Argument"),
                             QObject::tr("Writable arguments of a function call."),
                             outputArgumentFormat,
                             FormatDescription::ShowAllControls);
    formatDescr.emplace_back(C_STATIC_MEMBER,
                             QObject::tr("Static Member"),
                             QObject::tr("Names of static fields or member functions."),
                             FormatDescription::ShowAllControls);

    const auto cocoControls = FormatDescription::ShowControls(
        FormatDescription::ShowAllAbsoluteControls | FormatDescription::ShowRelativeControls);
    formatDescr.emplace_back(C_COCO_CODE_ADDED,
                             QObject::tr("Code Coverage Added Code"),
                             QObject::tr("New code that was not checked for tests."),
                             cocoControls);
    formatDescr.emplace_back(C_COCO_PARTIALLY_COVERED,
                             QObject::tr("Partially Covered Code"),
                             QObject::tr("Partial branch/condition coverage."),
                             Qt::darkYellow,
                             cocoControls);
    formatDescr.emplace_back(C_COCO_NOT_COVERED,
                             QObject::tr("Uncovered Code"),
                             QObject::tr("Not covered at all."),
                             Qt::red,
                             cocoControls);
    formatDescr.emplace_back(C_COCO_FULLY_COVERED,
                             QObject::tr("Fully Covered Code"),
                             QObject::tr("Fully covered code."),
                             Qt::green,
                             cocoControls);
    formatDescr.emplace_back(C_COCO_MANUALLY_VALIDATED,
                             QObject::tr("Manually Validated Code"),
                             QObject::tr("User added validation."),
                             Qt::blue,
                             cocoControls);
    formatDescr.emplace_back(C_COCO_DEAD_CODE,
                             QObject::tr("Code Coverage Dead Code"),
                             QObject::tr("Unreachable code."),
                             Qt::magenta,
                             cocoControls);
    formatDescr.emplace_back(C_COCO_EXECUTION_COUNT_TOO_LOW,
                             QObject::tr("Code Coverage Execution Count Too Low"),
                             QObject::tr("Minimum count not reached."),
                             Qt::red,
                             cocoControls);
    formatDescr.emplace_back(C_COCO_NOT_COVERED_INFO,
                             QObject::tr("Implicitly Not Covered Code"),
                             QObject::tr("PLACEHOLDER"),
                             Qt::red,
                             cocoControls);
    formatDescr.emplace_back(C_COCO_COVERED_INFO,
                             QObject::tr("Implicitly Covered Code"),
                             QObject::tr("PLACEHOLDER"),
                             Qt::green,
                             cocoControls);
    formatDescr.emplace_back(C_COCO_MANUALLY_VALIDATED_INFO,
                             QObject::tr("Implicit Manual Coverage Validation"),
                             QObject::tr("PLACEHOLDER"),
                             Qt::blue,
                             cocoControls);

    return formatDescr;
}

} // namespace TextEditor
