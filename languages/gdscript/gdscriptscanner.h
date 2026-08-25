// GDScript language support for Godot Engine
#pragma once

#include "languages/token.h"
#include <QString>

namespace GDScript {
using namespace Code;

/**
 * @brief The Scanner class - lexical scanner for GDScript syntax highlighting
 *
 * GDScript is the scripting language for Godot game engine.
 * It has Python-like syntax with indentation-based blocks.
 */
class Scanner
{
public:
    Scanner(const Scanner &other) = delete;
    void operator=(const Scanner &other) = delete;

    enum State {
        State_Default,
        State_String,
        State_MultiLineString
    };

    Scanner(const QChar *text, const int length);

    void setState(int state);
    int state() const;
    Token read();
    QString value(const Token& tk) const;

private:
    Token onDefaultState();

    void checkEscapeSequence(QChar quoteChar);
    Token readStringLiteral(QChar quoteChar);
    Token readMultiLineStringLiteral(QChar quoteChar);
    Token readIdentifier();
    Token readNumber();
    Token readFloatNumber();
    Token readComment();
    Token readWhiteSpace();
    Token readOperator();
    Token readBrace(bool isOpening);
    Token readNodePath();
    Token readAnnotation();

    void clearState();
    void saveState(State state, QChar savedData);
    void parseState(State &state, QChar &savedData) const;

    void setAnchor() { m_markedPosition = m_position; }
    void move() { ++m_position; }
    int length() const { return m_position - m_markedPosition; }
    int anchor() const { return m_markedPosition; }
    bool isEnd() const { return m_position >= m_textLength; }

    QChar peek(int offset = 0) const
    {
        int pos = m_position + offset;
        if (pos >= m_textLength)
            return QLatin1Char('\0');
        return m_text[pos];
    }

    const QChar *m_text;
    const int m_textLength;
    int m_position = 0;
    int m_markedPosition = 0;

    int m_state;
};

} // namespace GDScript
