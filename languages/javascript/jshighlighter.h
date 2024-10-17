// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"

#include "jsscanner.h"

#include "textdocumentlayout.h"
#include "syntaxhighlighter.h"

namespace Javascript {



class TEXTEDITOR_EXPORT Highlighter : public TextEditor::SyntaxHighlighter
{
    Q_OBJECT

public:
    Highlighter(QTextDocument *parent = nullptr);
    ~Highlighter() override;



protected:
    void highlightBlock(const QString &text) override;

    int onBlockStart();
    void onBlockEnd(int state);

    // The functions are notified whenever parentheses are encountered.
    // Custom behaviour can be added, for example storing info for indenting.
    void onOpeningParenthesis(QChar parenthesis, int pos, bool atStart);
    void onClosingParenthesis(QChar parenthesis, int pos, bool atEnd);



private:
    int m_braceDepth;
    int m_foldingIndent;
    bool m_inMultilineComment;

    Javascript::Scanner m_scanner;
    TextEditor::Parentheses m_currentBlockParentheses;
};

} // namespace QmlJSEditor
