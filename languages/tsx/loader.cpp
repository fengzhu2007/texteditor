#include "loader.h"
#include "tsxhighlighter.h"
#include "tsxindenter.h"
#include "tsxautocompleter.h"
#include "tsxcodeformatter.h"

namespace Tsx {
Loader::Loader(QTextDocument* doc):TextEditor::LanguageLoader(doc) {
    m_hightlighter = new Tsx::Highlighter();
    m_indenter = Tsx::createIndenter(doc);
    m_autoCompleter = new Tsx::AutoCompleter();
    m_codeFormatter = new Tsx::CodeFormatter;
}
}
