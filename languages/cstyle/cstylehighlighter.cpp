// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cstylehighlighter.h"

#include <QSet>

#include <utils/qtcassert.h>

using namespace TextEditor;

namespace CStyle {

CStyleHighlighter::CStyleHighlighter(QTextDocument *parent)
    : SyntaxHighlighter(parent),
      m_qmlEnabled(true),
      m_braceDepth(0),
      m_foldingIndent(0),
      m_inMultilineComment(false)
{
    m_currentBlockParentheses.reserve(20);
    setDefaultTextFormatCategories();
}

CStyleHighlighter::~CStyleHighlighter() = default;

bool CStyleHighlighter::isQmlEnabled() const
{
    return m_qmlEnabled;
}

void CStyleHighlighter::setQmlEnabled(bool qmlEnabled)
{
    m_qmlEnabled = qmlEnabled;
}

void CStyleHighlighter::highlightBlock(const QString &text)
{

    const QList<Token> tokens = m_scanner(text, onBlockStart());

    int index = 0;
    while (index < tokens.size()) {
        const Token &token = tokens.at(index);

        switch (token.kind) {
            case Token::Number:
                setFormat(token.offset, token.length, formatForCategory(C_NUMBER));
                break;
            case Token::Keyword:
                setFormat(token.offset, token.length, formatForCategory(C_KEYWORD));
                break;

            case Token::String:
                setFormat(token.offset, token.length, formatForCategory(C_STRING));
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

bool CStyleHighlighter::maybeQmlKeyword(QStringView text) const
{
    if (text.isEmpty())
        return false;

    const QChar ch = text.at(0);
    if (ch == QLatin1Char('p') && text == QLatin1String("property"))
        return true;
    else if (ch == QLatin1Char('a') && text == QLatin1String("alias"))
        return true;
    else if (ch == QLatin1Char('c') && text == QLatin1String("component"))
        return true;
    else if (ch == QLatin1Char('s') && text == QLatin1String("signal"))
        return true;
    else if (ch == QLatin1Char('r') && (text == QLatin1String("readonly") || text == QLatin1String("required")))
        return true;
    else if (ch == QLatin1Char('i') && text == QLatin1String("import"))
        return true;
    else if (ch == QLatin1Char('o') && text == QLatin1String("on"))
        return true;
    else if (ch == QLatin1Char('e') && text == QLatin1String("enum"))
        return true;
    else
        return false;
}

bool CStyleHighlighter::maybeQmlBuiltinType(QStringView text) const
{
    if (text.isEmpty())
        return false;

    const QChar ch = text.at(0);

    if (ch == QLatin1Char('a') && text == QLatin1String("action"))
            return true;
    else if (ch == QLatin1Char('b') && text == QLatin1String("bool"))
        return true;
    else if (ch == QLatin1Char('c') && text == QLatin1String("color"))
        return true;
    else if (ch == QLatin1Char('d') && text == QLatin1String("date"))
        return true;
    else if (ch == QLatin1Char('d') && text == QLatin1String("double"))
        return true;
    else if (ch == QLatin1Char('e') && text == QLatin1String("enumeration"))
        return true;
    else if (ch == QLatin1Char('f') && text == QLatin1String("font"))
        return true;
    else if (ch == QLatin1Char('i') && text == QLatin1String("int"))
        return true;
    else if (ch == QLatin1Char('l') && text == QLatin1String("list"))
        return true;
    else if (ch == QLatin1Char('m') && text == QLatin1String("matrix4x4"))
        return true;
    else if (ch == QLatin1Char('p') && text == QLatin1String("point"))
        return true;
    else if (ch == QLatin1Char('q') && text == QLatin1String("quaternion"))
        return true;
    else if (ch == QLatin1Char('r') && text == QLatin1String("real"))
        return true;
    else if (ch == QLatin1Char('r') && text == QLatin1String("rect"))
        return true;
    else if (ch == QLatin1Char('s') && text == QLatin1String("size"))
        return true;
    else if (ch == QLatin1Char('s') && text == QLatin1String("string"))
        return true;
    else if (ch == QLatin1Char('t') && text == QLatin1String("time"))
        return true;
    else if (ch == QLatin1Char('u') && text == QLatin1String("url"))
        return true;
    else if (ch == QLatin1Char('v') && text == QLatin1String("variant"))
        return true;
    else if (ch == QLatin1Char('v') && text == QLatin1String("var"))
        return true;
    else if (ch == QLatin1Char('v') && text == QLatin1String("vector2d"))
        return true;
    else if (ch == QLatin1Char('v') && text == QLatin1String("vector3d"))
        return true;
    else if (ch == QLatin1Char('v') && text == QLatin1String("vector4d"))
        return true;
    else
        return false;
}

int CStyleHighlighter::onBlockStart()
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
        state = previousState;
        m_braceDepth = 0;
        m_inMultilineComment = ((state & Scanner::MultiLineMask) == Scanner::MultiLineComment);
    }
    m_foldingIndent = m_braceDepth;

    return state;
}

void CStyleHighlighter::onBlockEnd(int state)
{
    setCurrentBlockState(state);
    TextDocumentLayout::setParentheses(currentBlock(), m_currentBlockParentheses);
    TextDocumentLayout::setFoldingIndent(currentBlock(), m_foldingIndent);
}

void CStyleHighlighter::onOpeningParenthesis(QChar parenthesis, int pos, bool atStart)
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

void CStyleHighlighter::onClosingParenthesis(QChar parenthesis, int pos, bool atEnd)
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
