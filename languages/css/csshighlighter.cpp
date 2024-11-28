// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "csshighlighter.h"
#include "languages/token.h"
#include "languages/html/htmlscanner.h"

#include <QSet>

#include <utils/qtcassert.h>

using namespace TextEditor;
using namespace Code;

namespace Css {

Highlighter::Highlighter(QTextDocument *parent)
    : SyntaxHighlighter(parent),
      m_braceDepth(0),
      m_foldingIndent(0),
      m_inMultilineComment(false)
{
    m_currentBlockParentheses.reserve(20);
    setDefaultTextFormatCategories();
}

Highlighter::~Highlighter() = default;



void Highlighter::highlightBlock(const QString &text)
{
    int from = 0;
    int state = onBlockStart();
    const QList<Token> tokens = m_scanner(from,text, state);
    //Html::Scanner::dump(text,tokens);
    int index = 0;
    while (index < tokens.size()) {
        const Token &token = tokens.at(index);

        switch (token.kind) {
            case Token::Keyword:
            case Token::AtRules:
            case Token::Selector:
            case Token::AttrName:
                setFormat(token.offset, token.length, formatForCategory(C_KEYWORD));
                break;
            case Token::String:
        case Token::AttrValue:
                setFormat(token.offset, token.length, formatForCategory(C_STRING));
                break;

            case Token::Comment:
                setFormat(token.offset, token.length, formatForCategory(C_COMMENT));
                break;

            case Token::LeftParenthesis:
                onOpeningParenthesis(QLatin1Char('('), token.offset, index == 0);
                setFormat(token.offset, token.length, formatForCategory(C_OPERATOR));
                break;

            case Token::RightParenthesis:
                onClosingParenthesis(QLatin1Char(')'), token.offset, index == tokens.size()-1);
                setFormat(token.offset, token.length, formatForCategory(C_OPERATOR));
                break;

            case Token::LeftBrace:
                onOpeningParenthesis(QLatin1Char('{'), token.offset, index == 0);
                setFormat(token.offset, token.length, formatForCategory(C_OPERATOR));
                break;

            case Token::RightBrace:
                onClosingParenthesis(QLatin1Char('}'), token.offset, index == tokens.size()-1);
                setFormat(token.offset, token.length, formatForCategory(C_OPERATOR));
                break;

            case Token::LeftBracket:
                onOpeningParenthesis(QLatin1Char('['), token.offset, index == 0);
                setFormat(token.offset, token.length, formatForCategory(C_OPERATOR));
                break;

            case Token::RightBracket:
                onClosingParenthesis(QLatin1Char(']'), token.offset, index == tokens.size()-1);
                setFormat(token.offset, token.length, formatForCategory(C_OPERATOR));
                break;

            case Token::Delimiter:
                setFormat(token.offset, token.length, formatForCategory(C_OPERATOR));
                break;

            default:
                break;
        } // end swtich

        ++index;
    }

    int previousTokenEnd = 0;
    for (const auto &token : tokens) {
        setFormat(previousTokenEnd, token.begin() - previousTokenEnd, formatForCategory(C_VISUAL_WHITESPACE));

        switch (token.kind) {
        case Token::Comment:
        case Token::String:
        case Token::RegExp: {
            int i = token.begin(), e = token.end();
            while (i < e) {
                const QChar ch = text.at(i);
                if (ch.isSpace()) {
                    const int start = i;
                    do {
                        ++i;
                    } while (i < e && text.at(i).isSpace());
                    setFormat(start, i - start, formatForCategory(C_VISUAL_WHITESPACE));
                } else {
                    ++i;
                }
            }
        } break;

        default:
            break;
        } // end of switch

        previousTokenEnd = token.end();
    }

    setFormat(previousTokenEnd, text.length() - previousTokenEnd, formatForCategory(C_VISUAL_WHITESPACE));

    setCurrentBlockState(m_scanner.state());
    onBlockEnd(m_scanner.state());
}

int Highlighter::onBlockStart()
{
    m_currentBlockParentheses.clear();
    m_braceDepth = 0;
    m_foldingIndent = 0;
    m_inMultilineComment = false;
    if (TextBlockUserData *userData = TextDocumentLayout::textUserData(currentBlock())) {
        userData->setFoldingIndent(0);
        userData->setFoldingStartIncluded(false);
        userData->setFoldingEndIncluded(false);
    }

    int state = 0;
    int previousState = previousBlockState();
    if (previousState != -1) {
        state = previousState ;
        m_braceDepth =0;
        m_inMultilineComment = ((state & Scanner::MultiLineComment) == Scanner::MultiLineComment);
    }
    m_foldingIndent = m_braceDepth;

    return state;
}

void Highlighter::onBlockEnd(int state)
{
    setCurrentBlockState(state);
    TextDocumentLayout::setParentheses(currentBlock(), m_currentBlockParentheses);
    TextDocumentLayout::setFoldingIndent(currentBlock(), m_foldingIndent);
}

void Highlighter::onOpeningParenthesis(QChar parenthesis, int pos, bool atStart)
{
    if (parenthesis == QLatin1Char('{') || parenthesis == QLatin1Char('[') || parenthesis == QLatin1Char('+')) {
        ++m_braceDepth;
        // if a folding block opens at the beginning of a line, treat the entire line
        // as if it were inside the folding block
        if (atStart)
            TextDocumentLayout::userData(currentBlock())->setFoldingStartIncluded(true);
    }
    m_currentBlockParentheses.push_back(Parenthesis(Parenthesis::Opened, parenthesis, pos));
}

void Highlighter::onClosingParenthesis(QChar parenthesis, int pos, bool atEnd)
{
    if (parenthesis == QLatin1Char('}') || parenthesis == QLatin1Char(']') || parenthesis == QLatin1Char('-')) {
        --m_braceDepth;
        if (atEnd)
            TextDocumentLayout::userData(currentBlock())->setFoldingEndIncluded(true);
        else
            m_foldingIndent = qMin(m_braceDepth, m_foldingIndent); // folding indent is the minimum brace depth of a block
    }
    m_currentBlockParentheses.push_back(Parenthesis(Parenthesis::Closed, parenthesis, pos));
}

} // namespace QmlJSEditor
