#ifndef LANGUAGELOADER_H
#define LANGUAGELOADER_H

#include "texteditor_global.h"
#include "syntax-highlighting/definition.h"
#include <QTextDocument>

namespace TextEditor{
class SyntaxHighlighter;
class Indenter;
class AutoCompleter;
class TEXTEDITOR_EXPORT LanguageLoader
{
public:
    explicit LanguageLoader(const KSyntaxHighlighting::Definition &definition,QTextDocument* doc);
    SyntaxHighlighter* hightlighter();
    Indenter* indenter();
    AutoCompleter* autoCompleter();

private:
    SyntaxHighlighter* m_hightlighter;
    Indenter* m_indenter;
    AutoCompleter* m_autoCompleter;
};
}
#endif // LANGUAGELOADER_H
