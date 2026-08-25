// GDScript language support for Godot Engine
#pragma once

#include "syntaxhighlighter.h"

namespace GDScript {

class Scanner;

class Highlighter : public TextEditor::SyntaxHighlighter
{
public:
    Highlighter();

private:
    void highlightBlock(const QString &text) override;
    int highlightLine(const QString &text, int initialState);

    int m_lastIndent = 0;
};

} // namespace GDScript
