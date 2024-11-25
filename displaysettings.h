// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"

#include "QMetaType"
#include <QJsonObject>

QT_BEGIN_NAMESPACE
class QSettings;
class QLabel;
QT_END_NAMESPACE

namespace TextEditor {

enum class AnnotationAlignment
{
    NextToContent,
    NextToMargin,
    RightSide,
    BetweenLines
};

class TEXTEDITOR_EXPORT DisplaySettings
{
public:
    DisplaySettings() = default;

    void toSettings(QSettings *s) const;
    void fromSettings(QSettings *s);
    QJsonObject toJson();
    void fromJson(const QJsonObject& data);

    friend bool operator==(const DisplaySettings &t1, const DisplaySettings &t2) { return t1.equals(t2); }
    friend bool operator!=(const DisplaySettings &t1, const DisplaySettings &t2) { return !t1.equals(t2); }

    bool m_displayLineNumbers = true;
    bool m_textWrapping = true;
    bool m_visualizeWhitespace = false;
    bool m_visualizeIndent = true;
    bool m_displayFoldingMarkers = true;
    bool m_highlightCurrentLine = false;
    bool m_highlightBlocks = false;
    bool m_animateMatchingParentheses = true;
    bool m_highlightMatchingParentheses = true;
    bool m_markTextChanges = true ;
    bool m_autoFoldFirstComment = true;
    bool m_centerCursorOnScroll = false;
    bool m_openLinksInNextSplit = false;
    bool m_forceOpenLinksInNextSplit = false;
    bool m_displayFileEncoding = false;
    bool m_displayFileLineEnding = true;
    bool m_scrollBarHighlights = true;
    bool m_animateNavigationWithinFile = false;
    int m_animateWithinFileTimeMax = 333; // read only setting
    bool m_displayAnnotations = true;
    AnnotationAlignment m_annotationAlignment = AnnotationAlignment::RightSide;
    int m_minimalAnnotationContent = 15;

    bool equals(const DisplaySettings &ds) const;

    static QLabel *createAnnotationSettingsLink();
};

} // namespace TextEditor

Q_DECLARE_METATYPE(TextEditor::AnnotationAlignment)
