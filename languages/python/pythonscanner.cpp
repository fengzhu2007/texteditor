// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "pythonscanner.h"

#include <QSet>

namespace Python {

Scanner::Scanner(const QChar *text, const int length)
    : m_text(text), m_textLength(length), m_state(0)
{
}

void Scanner::setState(int state)
{
    m_state = state;
}

int Scanner::state() const
{
    return m_state;
}

Token Scanner::read()
{
    setAnchor();
    if (isEnd())
        return Token(-1,-1,Token::TokenEnd);

    State state;
    QChar saved;
    parseState(state, saved);
    switch (state) {
    case State_String:
        return readStringLiteral(saved);
    case State_MultiLineString:
        return readMultiLineStringLiteral(saved);
    default:
        return onDefaultState();
    }
}

QString Scanner::value(const Token &tk) const
{
    return QString(m_text + tk.offset, tk.length);
}

Token Scanner::onDefaultState()
{
    QChar first = peek();
    move();

    if (first == '\\' && peek() == '\n') {
        move();
        return Token(anchor(), 2,Token::Whitespace);
    }

    if (first == '.' && peek().isDigit())
        return readFloatNumber();

    if (first == '\'' || first == '\"')
        return readStringLiteral(first);

    if (first.isLetter() || first == '_')
        return readIdentifier();

    if (first.isDigit())
        return readNumber();

    if (first == '#') {
        if (peek() == '#')
            return readDoxygenComment();
        return readComment();
    }

    if (first == '(' || first == '[' || first == '{')
        return readBrace(true);
    if (first == ')' || first == ']' || first == '}')
        return readBrace(false);

    if (first.isSpace())
        return readWhiteSpace();

    return readOperator();
}

/**
 * @brief Lexer::passEscapeCharacter
 * @return returns true if escape sequence doesn't end with newline
 */
void Scanner::checkEscapeSequence(QChar quoteChar)
{
    if (peek() == '\\') {
        move();
        QChar ch = peek();
        if (ch == '\n' || ch.isNull())
            saveState(State_String, quoteChar);
    }
}

/**
  reads single-line string literal, surrounded by ' or " quotes
  */
Token Scanner::readStringLiteral(QChar quoteChar)
{
    QChar ch = peek();
    if (ch == quoteChar && peek(1) == quoteChar) {
        saveState(State_MultiLineString, quoteChar);
        return readMultiLineStringLiteral(quoteChar);
    }

    while (ch != quoteChar && !ch.isNull()) {
        checkEscapeSequence(quoteChar);
        move();
        ch = peek();
    }
    if (ch == quoteChar)
        clearState();
    move();
    return Token(anchor(), length(),Token::String);
}

/**
  reads multi-line string literal, surrounded by ''' or """ sequences
  */
Token Scanner::readMultiLineStringLiteral(QChar quoteChar)
{
    for (;;) {
        QChar ch = peek();
        if (ch.isNull())
            break;
        if (ch == quoteChar && peek(1) == quoteChar && peek(2) == quoteChar) {
            clearState();
            move();
            move();
            move();
            break;
        }
        move();
    }
    return Token(anchor(), length(),Token::String);
}

/**
  reads identifier and classifies it
  */
Token Scanner::readIdentifier()
{
    static const QSet<QString> keywords = {
        "and", "as", "assert", "break", "class", "continue", "def", "del", "elif",
        "else", "except", "exec", "finally", "for", "from", "global", "if", "import",
        "in", "is", "lambda", "not", "or", "pass", "print", "raise", "return", "try",
        "while", "with", "yield"
    };

    // List of Python magic methods and attributes
    static const QSet<QString> magics = {
        // ctor & dtor
        "__init__", "__del__",
        // string conversion functions
        "__str__", "__repr__", "__unicode__",
        // attribute access functions
        "__setattr__", "__getattr__", "__delattr__",
        // binary operators
        "__add__", "__sub__", "__mul__", "__truediv__", "__floordiv__", "__mod__",
        "__pow__", "__and__", "__or__", "__xor__", "__eq__", "__ne__", "__gt__",
        "__lt__", "__ge__", "__le__", "__lshift__", "__rshift__", "__contains__",
        // unary operators
        "__pos__", "__neg__", "__inv__", "__abs__", "__len__",
        // item operators like []
        "__getitem__", "__setitem__", "__delitem__", "__getslice__", "__setslice__",
        "__delslice__",
        // other functions
        "__cmp__", "__hash__", "__nonzero__", "__call__", "__iter__", "__reversed__",
        "__divmod__", "__int__", "__long__", "__float__", "__complex__", "__hex__",
        "__oct__", "__index__", "__copy__", "__deepcopy__", "__sizeof__", "__trunc__",
        "__format__",
        // magic attributes
        "__name__", "__module__", "__dict__", "__bases__", "__doc__"
    };

    // List of python built-in functions and objects
    static const QSet<QString> builtins = {
        "range", "xrange", "int", "float", "long", "hex", "oct", "chr", "ord",
        "len", "abs", "None", "True", "False"
    };

    QChar ch = peek();
    while (ch.isLetterOrNumber() || ch == '_') {
        move();
        ch = peek();
    }

    const QString v = QString(m_text + m_markedPosition, length());
    Token::Kind kind = Token::Identifier;
    if (v == "self")
        kind = Token::ClassField;
    else if (builtins.contains(v))
        kind = Token::Type;
    else if (magics.contains(v))
        kind = Token::MagicAttr;
    else if (keywords.contains(v))
        kind = Token::Keyword;

    return Token(anchor(), length(),kind);
}

inline static bool isHexDigit(QChar ch)
{
    return ch.isDigit()
            || (ch >= 'a' && ch <= 'f')
            || (ch >= 'A' && ch <= 'F');
}

inline static bool isOctalDigit(QChar ch)
{
    return ch.isDigit() && ch != '8' && ch != '9';
}

inline static bool isBinaryDigit(QChar ch)
{
    return ch == '0' || ch == '1';
}

inline static bool isValidIntegerSuffix(QChar ch)
{
    return ch == 'l' || ch == 'L';
}

Token Scanner::readNumber()
{
    if (!isEnd()) {
        QChar ch = peek();
        if (ch.toLower() == 'b') {
            move();
            while (isBinaryDigit(peek()))
                move();
        } else if (ch.toLower() == 'o') {
            move();
            while (isOctalDigit(peek()))
                move();
        } else if (ch.toLower() == 'x') {
            move();
            while (isHexDigit(peek()))
                move();
        } else { // either integer or float number
            return readFloatNumber();
        }
        if (isValidIntegerSuffix(peek()))
            move();
    }
    return Token(anchor(), length(),Token::Number);
}

Token Scanner::readFloatNumber()
{
    enum
    {
        State_INTEGER,
        State_FRACTION,
        State_EXPONENT
    } state;
    state = (peek(-1) == '.') ? State_FRACTION : State_INTEGER;

    for (;;) {
        QChar ch = peek();
        if (ch.isNull())
            break;

        if (state == State_INTEGER) {
            if (ch == '.')
                state = State_FRACTION;
            else if (!ch.isDigit())
                break;
        } else if (state == State_FRACTION) {
            if (ch == 'e' || ch == 'E') {
                QChar next = peek(1);
                QChar next2 = peek(2);
                bool isExp = next.isDigit()
                        || ((next == '-' || next == '+') && next2.isDigit());
                if (isExp) {
                    move();
                    state = State_EXPONENT;
                } else {
                    break;
                }
            } else if (!ch.isDigit()) {
                break;
            }
        } else if (!ch.isDigit()) {
            break;
        }
        move();
    }

    QChar ch = peek();
    if ((state == State_INTEGER && (ch == 'l' || ch == 'L'))
            || (ch == 'j' || ch =='J'))
        move();

    return Token(anchor(), length(),Token::Number);
}

/**
  reads single-line python comment, started with "#"
  */
Token Scanner::readComment()
{
    QChar ch = peek();
    while (ch != '\n' && !ch.isNull()) {
        move();
        ch = peek();
    }
    return Token(anchor(), length(),Token::Comment);
}

/**
  reads single-line python doxygen comment, started with "##"
  */
Token Scanner::readDoxygenComment()
{
    QChar ch = peek();
    while (ch != '\n' && !ch.isNull()) {
        move();
        ch = peek();
    }
    return Token(anchor(), length(),Token::Doxygen);
}

/**
  reads whitespace
  */
Token Scanner::readWhiteSpace()
{
    while (peek().isSpace())
        move();
    return Token(anchor(), length(),Token::Whitespace);
}

/**
  reads punctuation symbols, excluding some special
  */
Token Scanner::readOperator()
{
    static const QString EXCLUDED_CHARS = "\'\"_#([{}])";
    QChar ch = peek();
    while (ch.isPunct() && !EXCLUDED_CHARS.contains(ch)) {
        move();
        ch = peek();
    }
    return Token(anchor(), length(),Token::Operator);
}

Token Scanner::readBrace(bool isOpening)
{
    Token::Kind kind = isOpening ? Token::LeftParenthesis : Token::RightParenthesis;
    return Token(anchor(), length(),kind);
}

void Scanner::clearState()
{
    m_state = 0;
}

void Scanner::saveState(State state, QChar savedData)
{
    m_state = (state << 16) | static_cast<int>(savedData.unicode());
}

void Scanner::parseState(State &state, QChar &savedData) const
{
    state = static_cast<State>(m_state >> 16);
    savedData = static_cast<ushort>(m_state);
}

} // Python
