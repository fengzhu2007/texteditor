#include "loader.h"
#include "jshighlighter.h"
#include "jsindenter.h"
#include "jsautocompleter.h"
#include "jscodeformatter.h"

namespace Javascript {
Loader::Loader(QTextDocument* doc):TextEditor::LanguageLoader(doc) {
    m_hightlighter = new Javascript::Highlighter();
    m_indenter = Javascript::createIndenter(doc);
    m_autoCompleter = new Javascript::AutoCompleter();
    m_codeFormatter = new Javascript::CodeFormatter;
}
}
