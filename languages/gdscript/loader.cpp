// GDScript language support for Godot Engine

#include "loader.h"
#include "gdscripthighlighter.h"
#include "gdscriptindenter.h"
#include "gdscriptautocompleter.h"

namespace GDScript {
Loader::Loader(QTextDocument* doc):TextEditor::LanguageLoader(doc) {
    m_hightlighter = new GDScript::Highlighter();
    m_indenter = GDScript::createIndenter(doc);
    m_autoCompleter = new GDScript::AutoCompleter();
}
}
