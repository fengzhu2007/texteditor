// GDScript language support for Godot Engine

#include "gdscriptscanner.h"

#include <QSet>

namespace GDScript {

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
        return Token(anchor(), 2, Token::Whitespace);
    }

    if (first == '\'' || first == '\"')
        return readStringLiteral(first);

    if (first.isLetter() || first == '_')
        return readIdentifier();

    if (first.isDigit())
        return readNumber();

    if (first == '#')
        return readComment();

    if (first == '(' || first == '[' || first == '{')
        return readBrace(true);
    if (first == ')' || first == ']' || first == '}')
        return readBrace(false);

    if (first == '$' || first == '%')
        return readNodePath();

    if (first == '@')
        return readAnnotation();

    if (first.isSpace())
        return readWhiteSpace();

    return readOperator();
}

void Scanner::checkEscapeSequence(QChar quoteChar)
{
    if (peek() == '\\') {
        move();
        QChar ch = peek();
        if (ch == '\n' || ch.isNull())
            saveState(State_String, quoteChar);
    }
}

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
    return Token(anchor(), length(), Token::String);
}

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
    return Token(anchor(), length(), Token::String);
}

Token Scanner::readIdentifier()
{
    // GDScript keywords
    static const QSet<QString> keywords = {
        // Control flow
        "if", "elif", "else", "for", "while", "match", "switch", "case", "break",
        "continue", "pass", "return", "when", "yield",
        // Declarations
        "var", "const", "func", "class", "class_name", "extends", "signal", "enum",
        "static", "namespace",
        // OOP
        "self", "super",
        // Other
        "as", "assert", "await", "breakpoint", "in", "is", "preload", "load",
        "trait", "and", "or", "not"
    };

    // GDScript built-in types
    static const QSet<QString> builtins = {
        // Basic types
        "bool", "int", "float", "String", "StringName", "NodePath",
        // Math types
        "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", "Vector4i",
        "Rect2", "Rect2i", "Transform2D",
        // 3D types
        "Transform3D", "Basis", "Quaternion", "AABB", "Plane",
        // Collections
        "Array", "Dictionary", "PackedByteArray", "PackedInt32Array",
        "PackedInt64Array", "PackedFloat32Array", "PackedFloat64Array",
        "PackedStringArray", "PackedVector2Array", "PackedVector3Array",
        "PackedColorArray",
        // Other types
        "Callable", "Signal", "RID", "Object", "Node", "Resource",
        "Color", "Rect3"
    };

    // Constants
    static const QSet<QString> constants = {
        "true", "false", "null", "PI", "TAU", "INF", "NAN"
    };

    QChar ch = peek();
    while (ch.isLetterOrNumber() || ch == '_') {
        move();
        ch = peek();
    }

    const QString v = QString(m_text + m_markedPosition, length());
    Token::Kind kind = Token::Identifier;

    if (v == "self" || v == "super")
        kind = Token::ClassField;
    else if (constants.contains(v))
        kind = Token::Type;
    else if (builtins.contains(v))
        kind = Token::Type;
    else if (keywords.contains(v))
        kind = Token::Keyword;

    return Token(anchor(), length(), kind);
}

inline static bool isHexDigit(QChar ch)
{
    return ch.isDigit()
            || (ch >= 'a' && ch <= 'f')
            || (ch >= 'A' && ch <= 'F');
}

inline static bool isOctalDigit(QChar ch)
{
    return ch >= '0' && ch <= '7';
}

inline static bool isBinaryDigit(QChar ch)
{
    return ch == '0' || ch == '1';
}

Token Scanner::readNumber()
{
    if (!isEnd()) {
        QChar ch = peek();
        if (ch.toLower() == 'b') {
            // Binary: 0b1010
            move();
            while (isBinaryDigit(peek()))
                move();
        } else if (ch.toLower() == 'o') {
            // Octal: 0o777
            move();
            while (isOctalDigit(peek()))
                move();
        } else if (ch.toLower() == 'x') {
            // Hex: 0xFF
            move();
            while (isHexDigit(peek()))
                move();
        } else {
            // Integer or float
            return readFloatNumber();
        }
        // GDScript supports underscore separators in numbers
        while (peek() == '_') {
            move();
            if (peek().isDigit()) {
                while (peek().isDigit() || peek() == '_')
                    move();
            }
        }
    }
    return Token(anchor(), length(), Token::Number);
}

Token Scanner::readFloatNumber()
{
    enum {
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
            else if (ch == '_') {
                move(); // skip underscore separator
                continue;
            }
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
            } else if (ch == '_') {
                move();
                continue;
            }
            else if (!ch.isDigit()) {
                break;
            }
        } else if (ch == '_') {
            move();
            continue;
        }
        else if (!ch.isDigit()) {
            break;
        }
        move();
    }

    return Token(anchor(), length(), Token::Number);
}

Token Scanner::readComment()
{
    QChar ch = peek();
    while (ch != '\n' && !ch.isNull()) {
        move();
        ch = peek();
    }
    return Token(anchor(), length(), Token::Comment);
}

Token Scanner::readWhiteSpace()
{
    while (peek().isSpace())
        move();
    return Token(anchor(), length(), Token::Whitespace);
}

Token Scanner::readOperator()
{
    static const QString EXCLUDED_CHARS = "\'\"_#([{}])$%@";
    QChar ch = peek();
    while (ch.isPunct() && !EXCLUDED_CHARS.contains(ch)) {
        move();
        ch = peek();
    }
    return Token(anchor(), length(), Token::Operator);
}

Token Scanner::readBrace(bool isOpening)
{
    Token::Kind kind = isOpening ? Token::LeftParenthesis : Token::RightParenthesis;
    return Token(anchor(), length(), kind);
}

Token Scanner::readNodePath()
{
    // GDScript node paths: $NodeName, %UniqueNode, $Path/To/Node
    QChar ch = peek();
    while (ch.isLetterOrNumber() || ch == '_' || ch == '/' || ch == '%') {
        move();
        ch = peek();
    }
    return Token(anchor(), length(), Token::String);
}

Token Scanner::readAnnotation()
{
    // GDScript annotations: @export, @onready, @tool, etc.
    QChar ch = peek();
    while (ch.isLetterOrNumber() || ch == '_') {
        move();
        ch = peek();
    }
    return Token(anchor(), length(), Token::Decorator);
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

} // namespace GDScript
