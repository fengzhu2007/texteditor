#include "loader.h"
#include "htmlhighlighter.h"
#include "htmlindenter.h"
#include "htmlautocompleter.h"
#include "htmlcodeformatter.h"

namespace Html {
Loader::Loader(QTextDocument* doc):TextEditor::LanguageLoader(doc) {
    m_hightlighter = new Html::Highlighter();
    m_indenter = Html::createIndenter(doc);
    m_autoCompleter = new Html::AutoCompleter();
    m_codeFormatter = new Html::CodeFormatter;
}
}
