// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsxhighlighter.h"

#include <QSet>
#include <QDebug>
#include <utils/qtcassert.h>

using namespace TextEditor;
using namespace Code;

namespace Jsx {

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
    auto previous = currentBlock().previous();
    TextBlockUserData *userData = nullptr;
    QStack<int> stacks;
    if(previous.isValid()){
        userData = TextDocumentLayout::userData(previous);
        stacks = userData->stateStack();
        //qDebug()<<"previous:"<<stacks;
    }

    const QList<Token> tokens = m_scanner(from,text, state,stacks);
    int index = 0;
    while (index < tokens.size()) {
        const Token &token = tokens.at(index);

        switch (token.kind) {
            case Token::Keyword:
            case Token::TagStart:
            case Token::TagEnd:
            case Token::TagLeftBracket:
            case Token::TagRightBracket:
                setFormat(token.offset, token.length, formatForCategory(C_KEYWORD));
                break;

            case Token::String:
                setFormat(token.offset, token.length, formatForCategory(C_STRING));
                break;
            case Token::AttrValue:
                setFormat(token.offset, token.length, formatForCategory(C_STRING));
                break;
            case Token::AttrName:
                setFormat(token.offset, token.length, formatForCategory(C_FIELD));
                 break;
            case Token::Number:
                setFormat(token.offset, token.length, formatForCategory(C_NUMBER));
                break;
            case Token::Comment:
                if (m_inMultilineComment
                    && QStringView(text).mid(token.end() - 2, 2) == QLatin1String("*/")) {
                    onClosingParenthesis(QLatin1Char('-'), token.end() - 1, index == tokens.size()-1);
                    m_inMultilineComment = false;
                } else if (!m_inMultilineComment
                           && (m_scanner.state() & Scanner::MultiLineMask) == Scanner::MultiLineComment
                           && index == tokens.size() - 1) {
                    onOpeningParenthesis(QLatin1Char('+'), token.offset, index == 0);
                    m_inMultilineComment = true;
                }
                setFormat(token.offset, token.length, formatForCategory(C_COMMENT));
                break;

            case Token::RegExp:
                setFormat(token.offset, token.length, formatForCategory(C_STRING));
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
            case Token::Equal:
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
    {
         auto userData = TextDocumentLayout::userData(currentBlock());
        userData->setStateStack(m_scanner.statesStack());
         //qDebug()<<"save:"<<m_scanner.statesStack();
    }
    onBlockEnd(m_scanner.state());


}

static inline int expressionDepth(int state)
{
    if((state & Scanner::TemplateExpressionOpenBracesMask4) == Scanner::TemplateExpressionOpenBracesMask4){
        return 4;
    }else if((state & Scanner::TemplateExpressionOpenBracesMask3) == Scanner::TemplateExpressionOpenBracesMask3){
        return 3;
    }else if((state & Scanner::TemplateExpressionOpenBracesMask2) == Scanner::TemplateExpressionOpenBracesMask2){
        return 2;
    }else if((state & Scanner::TemplateExpressionOpenBracesMask1) == Scanner::TemplateExpressionOpenBracesMask1){
        return 1;
    }else{
        return 0;
    }
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

        state = previousState /*& 0xffffffff*/;
        m_braceDepth = expressionDepth(previousState);
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
