#include "loader.h"
#include "jsxhighlighter.h"
#include "jsxindenter.h"
#include "jsxautocompleter.h"
#include "jsxcodeformatter.h"

namespace Jsx {
Loader::Loader(QTextDocument* doc):TextEditor::LanguageLoader(doc) {
    m_hightlighter = new Jsx::Highlighter();
    m_indenter = Jsx::createIndenter(doc);
    m_autoCompleter = new Jsx::AutoCompleter();
    m_codeFormatter = new Jsx::CodeFormatter;
}
}
