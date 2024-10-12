// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"

#include "cstylescanner.h"

#include "textdocumentlayout.h"
#include "syntaxhighlighter.h"

namespace CStyle {



class TEXTEDITOR_EXPORT CStyleHighlighter : public TextEditor::SyntaxHighlighter
{
    Q_OBJECT

public:
    CStyleHighlighter(QTextDocument *parent = nullptr);
    ~CStyleHighlighter() override;

    bool isQmlEnabled() const;
    void setQmlEnabled(bool duiEnabled);

protected:
    void highlightBlock(const QString &text) override;

    int onBlockStart();
    void onBlockEnd(int state);

    // The functions are notified whenever parentheses are encountered.
    // Custom behaviour can be added, for example storing info for indenting.
    void onOpeningParenthesis(QChar parenthesis, int pos, bool atStart);
    void onClosingParenthesis(QChar parenthesis, int pos, bool atEnd);

    bool maybeQmlKeyword(QStringView text) const;
    bool maybeQmlBuiltinType(QStringView text) const;

private:
    bool m_qmlEnabled;
    int m_braceDepth;
    int m_foldingIndent;
    bool m_inMultilineComment;

    CStyle::Scanner m_scanner;
    TextEditor::Parentheses m_currentBlockParentheses;
};

} // namespace QmlJSEditor
