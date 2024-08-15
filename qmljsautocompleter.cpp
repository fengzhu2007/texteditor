// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmljsautocompleter.h"

#include "qmljsscanner.h"

#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QDebug>

using namespace QmlJSEditor;
using namespace QmlJS;

static int blockStartState(const QTextBlock &block)
{
    int state = block.previous().userState();
    qDebug()<<"blockStartState"<<block.previous().text()<<state;
    if (state == -1)
        return 0;
    else
        return state & 0xff;
}

static Token tokenUnderCursor(const QTextCursor &cursor)
{
    const QString blockText = cursor.block().text();
    const int blockState = blockStartState(cursor.block());

    Scanner tokenize;
    const QList<Token> tokens = tokenize(blockText, blockState);
    const int pos = cursor.positionInBlock();
    qDebug()<<"token size:"<<tokens.size()<<blockState;
    int tokenIndex = 0;
    for (; tokenIndex < tokens.size(); ++tokenIndex) {
        const Token &token = tokens.at(tokenIndex);
        qDebug()<<"token item:"<<token.kind<<token.begin()<<token.end();

        if (token.is(Token::Comment) || token.is(Token::String)) {
            if (pos > token.begin() && pos <= token.end())
                break;
        } else {
            if (pos >= token.begin() && pos < token.end())
                break;
        }
    }

    if (tokenIndex != tokens.size())
        return tokens.at(tokenIndex);

    return Token();
}

static bool shouldInsertMatchingText(QChar lookAhead)
{
    switch (lookAhead.unicode()) {
    case '{': case '}':
    case ']': case ')':
    case ';': case ',':
    case '"': case '\'':
        return true;

    default:
        if (lookAhead.isSpace())
            return true;

        return false;
    } // switch
}

static bool shouldInsertMatchingText(const QTextCursor &tc)
{
    QTextDocument *doc = tc.document();
    return shouldInsertMatchingText(doc->characterAt(tc.selectionEnd()));
}

static bool shouldInsertNewline(const QTextCursor &tc)
{
    QTextDocument *doc = tc.document();
    int pos = tc.selectionEnd();

    // count the number of empty lines.
    int newlines = 0;
    for (int e = doc->characterCount(); pos != e; ++pos) {
        const QChar ch = doc->characterAt(pos);

        if (! ch.isSpace())
            break;
        else if (ch == QChar::ParagraphSeparator)
            ++newlines;
    }

    if (newlines <= 1 && doc->characterAt(pos) != QLatin1Char('}'))
        return true;

    return false;
}

static bool isCompleteStringLiteral(QStringView text)
{
    if (text.length() < 2)
        return false;

    const QChar quote = text.at(0);

    if (text.at(text.length() - 1) == quote)
        return text.at(text.length() - 2) != QLatin1Char('\\'); // ### not exactly.

    return false;
}

AutoCompleter::AutoCompleter() = default;

AutoCompleter::~AutoCompleter() = default;

bool AutoCompleter::contextAllowsAutoBrackets(const QTextCursor &cursor,
                                              const QString &textToInsert) const
{
    QChar ch;

    if (! textToInsert.isEmpty())
        ch = textToInsert.at(0);

    switch (ch.unicode()) {
    case '(':
    case '[':
    case '{':

    case ')':
    case ']':
    case '}':

    case ';':
        break;

    default:
        if (ch.isNull())
            break;

        return false;
    } // end of switch

    const Token token = tokenUnderCursor(cursor);
    switch (token.kind) {
    case Token::Comment:
        return false;

    case Token::RightBrace:
        return false;

    case Token::String: {
        const QString blockText = cursor.block().text();
        const QStringView tokenText = QStringView(blockText).mid(token.offset, token.length);
        QChar quote = tokenText.at(0);
        // if a string literal doesn't start with a quote, it must be multiline
        if (quote != QLatin1Char('"') && quote != QLatin1Char('\'')) {
            const int startState = blockStartState(cursor.block());
            if ((startState & Scanner::MultiLineMask) == Scanner::MultiLineStringDQuote)
                quote = QLatin1Char('"');
            else if ((startState & Scanner::MultiLineMask) == Scanner::MultiLineStringSQuote)
                quote = QLatin1Char('\'');
        }

        // never insert ' into string literals, it adds spurious ' when writing contractions
        if (ch == QLatin1Char('\''))
            return false;

        if (ch != quote || isCompleteStringLiteral(tokenText))
            break;

        return false;
    }

    default:
        break;
    } // end of switch

    return true;
}

bool AutoCompleter::contextAllowsAutoQuotes(const QTextCursor &cursor,
                                            const QString &textToInsert) const
{
    if (!isQuote(textToInsert))
        return false;

    const Token token = tokenUnderCursor(cursor);
    switch (token.kind) {
    case Token::Comment:
        return false;

    case Token::RightBrace:
        return false;

    case Token::String: {
        const QString blockText = cursor.block().text();
        const QStringView tokenText = QStringView(blockText).mid(token.offset, token.length);
        QChar quote = tokenText.at(0);
        // if a string literal doesn't start with a quote, it must be multiline
        if (quote != QLatin1Char('"') && quote != QLatin1Char('\'')) {
            const int startState = blockStartState(cursor.block());
            if ((startState & Scanner::MultiLineMask) == Scanner::MultiLineStringDQuote)
                quote = QLatin1Char('"');
            else if ((startState & Scanner::MultiLineMask) == Scanner::MultiLineStringSQuote)
                quote = QLatin1Char('\'');
        }

        // never insert ' into string literals, it adds spurious ' when writing contractions
        if (textToInsert.at(0) == QLatin1Char('\'') && quote != '\'')
            return false;

        if (textToInsert.at(0) != quote || isCompleteStringLiteral(tokenText))
            break;

        return false;
    }

    default:
        break;
    } // end of switch

    return true;
}

bool AutoCompleter::contextAllowsElectricCharacters(const QTextCursor &cursor) const
{
    Token token = tokenUnderCursor(cursor);
    switch (token.kind) {
    case Token::Comment:
    case Token::String:
        return false;
    default:
        return true;
    }
}

bool AutoCompleter::isInComment(const QTextCursor &cursor) const
{
    return tokenUnderCursor(cursor).is(Token::Comment);
}

QString AutoCompleter::insertMatchingBrace(const QTextCursor &cursor,
                                           const QString &text,
                                           QChar lookAhead,
                                           bool skipChars,
                                           int *skippedChars) const
{
    if (text.length() != 1)
        return QString();

    if (! shouldInsertMatchingText(cursor))
        return QString();

    const QChar ch = text.at(0);
    switch (ch.unicode()) {
    case '(':
        return QString(QLatin1Char(')'));

    case '[':
        return QString(QLatin1Char(']'));

    case '{':
        return QString(); // nothing to do.

    case ')':
    case ']':
    case '}':
    case ';':
        if (lookAhead == ch && skipChars)
            ++*skippedChars;
        break;

    default:
        break;
    } // end of switch

    return QString();
}

QString AutoCompleter::insertMatchingQuote(const QTextCursor &/*tc*/, const QString &text,
                                           QChar lookAhead, bool skipChars, int *skippedChars) const
{
    if (isQuote(text)) {
        if (lookAhead == text && skipChars)
            ++*skippedChars;
        else
            return text;
    }
    return QString();
}

QString AutoCompleter::insertParagraphSeparator(const QTextCursor &cursor) const
{
    if (shouldInsertNewline(cursor)) {
        QTextCursor selCursor = cursor;
        selCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        if (! selCursor.selectedText().trimmed().isEmpty())
            return QString();

        return QLatin1String("}\n");
    }

    return QLatin1String("}");
}
