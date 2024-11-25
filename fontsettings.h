// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"

#include "colorscheme.h"
#include "textstyles.h"

#include "utils/filepath.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QTextCharFormat>
#include <QVector>
#include <QJsonObject>

QT_BEGIN_NAMESPACE
class QSettings;
class QFont;
QT_END_NAMESPACE

namespace TextEditor {

class TEXTEDITOR_EXPORT FormatDescription
{
public:
    enum ShowControls {
        ShowForegroundControl = 0x1,
        ShowBackgroundControl = 0x2,
        ShowForeAndBackgroundControl = ShowForegroundControl | ShowBackgroundControl,
        ShowFontControls = 0x4,
        ShowUnderlineControl = 0x8,
        ShowRelativeForegroundControl = 0x10,
        ShowRelativeBackgroundControl = 0x20,
        ShowRelativeControls = ShowRelativeForegroundControl | ShowRelativeBackgroundControl,
        ShowFontUnderlineAndRelativeControls = ShowFontControls
                                               | ShowUnderlineControl
                                               | ShowRelativeControls,
        ShowAllAbsoluteControls = ShowForegroundControl
                                  | ShowBackgroundControl
                                  | ShowFontControls
                                  | ShowUnderlineControl,
        ShowAllAbsoluteControlsExceptUnderline = ShowAllAbsoluteControls & ~ShowUnderlineControl,
        ShowAllControls = ShowAllAbsoluteControls | ShowRelativeControls
    };
    FormatDescription() = default;

    FormatDescription(TextStyle id,
                      const QString &displayName,
                      const QString &tooltipText,
                      ShowControls showControls = ShowAllAbsoluteControls);

    FormatDescription(TextStyle id,
                      const QString &displayName,
                      const QString &tooltipText,
                      const QColor &foreground,
                      ShowControls showControls = ShowAllAbsoluteControls);
    FormatDescription(TextStyle id,
                      const QString &displayName,
                      const QString &tooltipText,
                      const Format &format,
                      ShowControls showControls = ShowAllAbsoluteControls);
    FormatDescription(TextStyle id,
                      const QString &displayName,
                      const QString &tooltipText,
                      const QColor &underlineColor,
                      const QTextCharFormat::UnderlineStyle underlineStyle,
                      ShowControls showControls = ShowAllAbsoluteControls);

    TextStyle id() const { return m_id; }

    QString displayName() const
    { return m_displayName; }

    static QColor defaultForeground(TextStyle id);
    static QColor defaultBackground(TextStyle id);

    const Format &format() const { return m_format; }
    Format &format() { return m_format; }

    QString tooltipText() const
    { return  m_tooltipText; }

    bool showControl(ShowControls showControl) const;

private:
    TextStyle m_id;            // Name of the category
    Format m_format;            // Default format
    QString m_displayName;      // Displayed name of the category
    QString m_tooltipText;      // Description text for category
    ShowControls m_showControls = ShowAllAbsoluteControls;
};

using FormatDescriptions = std::vector<FormatDescription>;

/**
 * Font settings (default font and enumerated list of formats).
 */
class TEXTEDITOR_EXPORT FontSettings
{
public:
    using FormatDescriptions = std::vector<FormatDescription>;

    FontSettings();
    void clear();
    inline bool isEmpty() const { return m_scheme.isEmpty(); }

    void toSettings(QSettings *s) const;

    bool fromSettings(const FormatDescriptions &descriptions,
                      const QSettings *s);
    QJsonObject toJson();
    void fromJson(const QJsonObject& data);

    QVector<QTextCharFormat> toTextCharFormats(const QVector<TextStyle> &categories) const;
    QTextCharFormat toTextCharFormat(TextStyle category) const;
    QTextCharFormat toTextCharFormat(TextStyles textStyles) const;

    QString family() const;
    void setFamily(const QString &family);

    int fontSize() const;
    void setFontSize(int size);

    int fontZoom() const;
    void setFontZoom(int zoom);

    qreal lineSpacing() const;
    int relativeLineSpacing() const;
    void setRelativeLineSpacing(int relativeLineSpacing);

    QFont font() const;

    bool antialias() const;
    void setAntialias(bool antialias);

    Format &formatFor(TextStyle category);
    Format formatFor(TextStyle category) const;

    Utils::FilePath colorSchemeFileName() const;
    void setColorSchemeFileName(const Utils::FilePath &filePath);
    bool loadColorScheme(const Utils::FilePath &filePath, const FormatDescriptions &descriptions);
    bool loadColorScheme(const QString& name,const FormatDescriptions &descriptions);
    bool loadColorScheme(const QString& name);
    bool saveColorScheme(const Utils::FilePath &filePath);

    const ColorScheme &colorScheme() const;
    void setColorScheme(const ColorScheme &scheme);

    bool equals(const FontSettings &f) const;

    static QString defaultFixedFontFamily();
    static int defaultFontSize();

    static Utils::FilePath defaultSchemeFileName(const QString &fileName = {});

    static QList<QPair<QString,QString>> schemeMap();

    static FormatDescriptions initialFormats();

    friend bool operator==(const FontSettings &f1, const FontSettings &f2) { return f1.equals(f2); }
    friend bool operator!=(const FontSettings &f1, const FontSettings &f2) { return !f1.equals(f2); }

private:
    void addMixinStyle(QTextCharFormat &textCharFormat, const MixinTextStyles &mixinStyles) const;
    void clearCaches();

private:
    QString m_family;
    Utils::FilePath m_schemeFileName;
    int m_fontSize;
    int m_fontZoom;
    int m_lineSpacing;
    bool m_antialias;
    ColorScheme m_scheme;
    mutable QHash<TextStyle, QTextCharFormat> m_formatCache;
    mutable QHash<TextStyles, QTextCharFormat> m_textCharFormatCache;
};

} // namespace TextEditor
