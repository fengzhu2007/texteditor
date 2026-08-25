// GDScript language support for Godot Engine
#pragma once

#include "textindenter.h"
#include "texteditor_global.h"

namespace GDScript {

TEXTEDITOR_EXPORT TextEditor::TextIndenter *createIndenter(QTextDocument *doc);

} // namespace GDScript
