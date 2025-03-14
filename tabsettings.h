// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"

#include <QTextBlock>
#include <QJsonObject>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace TextEditor {

// Tab settings: Data type the GeneralSettingsPage acts on
// with some convenience functions for formatting.
class TEXTEDITOR_EXPORT TabSettings
{
public:

    enum TabPolicy {
        SpacesOnlyTabPolicy = 0,
        TabsOnlyTabPolicy = 1,
        MixedTabPolicy = 2
    };

    // This enum must match the indexes of continuationAlignBehavior widget
    enum ContinuationAlignBehavior {
        NoContinuationAlign = 0,
        ContinuationAlignWithSpaces = 1,
        ContinuationAlignWithIndent = 2
    };

    TabSettings();
    TabSettings(TabPolicy tabPolicy, int tabSize,int indentSize, ContinuationAlignBehavior continuationAlignBehavior);

    static void initGlobal(const QJsonObject& data);
    static void initGlobal(const TabSettings& setting);

    void toSettings(const QString &category, QSettings *s) const;
    void fromSettings(const QString &category, QSettings *s);
    QJsonObject toJson();
    void fromJson(const QJsonObject& data);

    QVariantMap toMap() const;
    void fromMap(const QVariantMap &map);

    int lineIndentPosition(const QString &text) const;
    int columnAt(const QString &text, int position) const;
    int columnAtCursorPosition(const QTextCursor &cursor) const;
    int positionAtColumn(const QString &text, int column, int *offset = nullptr, bool allowOverstep = false) const;
    int columnCountForText(const QString &text, int startColumn = 0) const;
    int indentedColumn(int column, bool doIndent = true) const;
    QString indentationString(int startColumn, int targetColumn, int padding, const QTextBlock &currentBlock = QTextBlock()) const;
    QString indentationString(const QString &text) const;
    int indentationColumn(const QString &text) const;
    static int maximumPadding(const QString &text);

    void indentLine(const QTextBlock &block, int newIndent, int padding = 0) const;
    void reindentLine(QTextBlock block, int delta) const;

    bool isIndentationClean(const QTextBlock &block, const int indent) const;
    bool guessSpacesForTabs(const QTextBlock &block) const;

    friend bool operator==(const TabSettings &t1, const TabSettings &t2) { return t1.equals(t2); }
    friend bool operator!=(const TabSettings &t1, const TabSettings &t2) { return !t1.equals(t2); }

    static int firstNonSpace(const QString &text);
    static inline bool onlySpace(const QString &text) { return firstNonSpace(text) == text.length(); }
    static int spacesLeftFromPosition(const QString &text, int position);
    static bool cursorIsAtBeginningOfLine(const QTextCursor &cursor);
    static int trailingWhitespaces(const QString &text);
    static void removeTrailingWhitespace(QTextCursor cursor, QTextBlock &block);


    TabPolicy m_tabPolicy = TabsOnlyTabPolicy;
    int m_tabSize = 4;
    int m_indentSize = 4;
    ContinuationAlignBehavior m_continuationAlignBehavior = ContinuationAlignWithSpaces;

    bool equals(const TabSettings &ts) const;


    static TabPolicy g_tabPolicy;
    static int g_tabSize;
    static int g_indentSize;
    static ContinuationAlignBehavior g_continuationAlignBehavior;

};

} // namespace TextEditor

Q_DECLARE_METATYPE(TextEditor::TabSettings)
Q_DECLARE_METATYPE(TextEditor::TabSettings::TabPolicy)
Q_DECLARE_METATYPE(TextEditor::TabSettings::ContinuationAlignBehavior)
