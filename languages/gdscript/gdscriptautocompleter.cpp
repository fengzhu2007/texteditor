// GDScript language support for Godot Engine

#include "gdscriptautocompleter.h"
#include "gdscriptscanner.h"

#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QDebug>

using namespace GDScript;

AutoCompleter::AutoCompleter()
{
    setAutoInsertBracketsEnabled(true);
    setAutoInsertQuotesEnabled(true);
    setSurroundWithBracketsEnabled(true);
    setSurroundWithQuotesEnabled(true);
    setOverwriteClosingCharsEnabled(true);
}

AutoCompleter::~AutoCompleter()
{
}

static int blockStartState(const QTextBlock &block)
{
    int state = block.previous().userState();
    if (state == -1)
        return 0;
    else
        return state;
}

static bool isInStringOrComment(const QTextCursor &cursor)
{
    const QString blockText = cursor.block().text();
    int blockState = blockStartState(cursor.block());

    Scanner scanner(blockText.constData(), blockText.size());
    scanner.setState(blockState);

    const int pos = cursor.positionInBlock();
    Code::Token tk(-1, -1, Code::Token::TokenEnd);
    while (!(tk = scanner.read()).isEndOfBlock()) {
        if (pos >= tk.begin() && pos < tk.end()) {
            return tk.kind == Code::Token::String || tk.kind == Code::Token::Comment;
        }
    }
    return false;
}

bool AutoCompleter::contextAllowsAutoBrackets(const QTextCursor &cursor,
                                              const QString &textToInsert) const
{
    if (isInStringOrComment(cursor))
        return false;

    Q_UNUSED(textToInsert);
    return true;
}

bool AutoCompleter::contextAllowsAutoQuotes(const QTextCursor &cursor,
                                            const QString &textToInsert) const
{
    if (isInStringOrComment(cursor))
        return false;

    Q_UNUSED(textToInsert);
    return true;
}

bool AutoCompleter::contextAllowsElectricCharacters(const QTextCursor &cursor) const
{
    if (isInStringOrComment(cursor))
        return false;

    return true;
}

bool AutoCompleter::isInComment(const QTextCursor &cursor) const
{
    return isInStringOrComment(cursor);
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
    }
}

static bool shouldInsertMatchingText(const QTextCursor &tc)
{
    QTextDocument *doc = tc.document();
    return shouldInsertMatchingText(doc->characterAt(tc.selectionEnd()));
}

QString AutoCompleter::insertMatchingBrace(const QTextCursor &tc,
                                           const QString &text,
                                           QChar lookAhead,
                                           bool skipChars,
                                           int *skippedChars,
                                           int *adjustPos) const
{
    Q_UNUSED(skipChars);
    Q_UNUSED(adjustPos);
    *skippedChars = 0;

    if (isInStringOrComment(tc))
        return QString();

    QString result;
    switch (text.at(0).unicode()) {
    case '{':
        result = QLatin1Char('}');
        break;
    case '[':
        result = QLatin1Char(']');
        break;
    case '(':
        result = QLatin1Char(')');
        break;
    default:
        break;
    }

    if (shouldInsertMatchingText(lookAhead))
        return result;

    return QString();
}

QString AutoCompleter::insertMatchingQuote(const QTextCursor &tc,
                                           const QString &text,
                                           QChar lookAhead,
                                           bool skipChars,
                                           int *skippedChars) const
{
    Q_UNUSED(skipChars);
    *skippedChars = 0;

    if (isInStringOrComment(tc))
        return QString();

    const QChar quote = text.at(0);
    if (quote != QLatin1Char('"') && quote != QLatin1Char('\''))
        return QString();

    if (shouldInsertMatchingText(lookAhead))
        return QString(quote);

    return QString();
}

static bool isElectricCharacter(const QChar &ch)
{
    return ch == QLatin1Char(':');
}

QString AutoCompleter::insertParagraphSeparator(const QTextCursor &tc) const
{
    QString text;

    const QString blockText = tc.block().text();
    if (blockText.isEmpty())
        return text;

    // Check if the line ends with an electric character
    int index = blockText.length() - 1;
    while (index > 0 && blockText[index].isSpace())
        --index;

    if (isElectricCharacter(blockText[index])) {
        // The line ends with ':', add extra indentation
        text = QString();
    }

    return text;
}
