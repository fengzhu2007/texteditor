#include "loader.h"
#include "csshighlighter.h"
#include "cssindenter.h"
#include "cssautocompleter.h"
#include "csscodeformatter.h"

namespace Css {
Loader::Loader(QTextDocument* doc):TextEditor::LanguageLoader(doc) {
    m_hightlighter = new Css::Highlighter();
    m_indenter = Css::createIndenter(doc);
    m_autoCompleter = new Css::AutoCompleter();
    m_codeFormatter = new Css::CodeFormatter;
}
}
