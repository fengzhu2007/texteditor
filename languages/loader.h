#ifndef LANGUAGELOADER_H
#define LANGUAGELOADER_H

#include "texteditor_global.h"
#include "syntax-highlighting/definition.h"
#include <QTextDocument>

namespace TextEditor{
class SyntaxHighlighter;
class Indenter;
class AutoCompleter;
class CodeFormatter;





class TEXTEDITOR_EXPORT LanguageLoader
{
public:
    explicit LanguageLoader(const KSyntaxHighlighting::Definition &definition,QTextDocument* doc);
    inline SyntaxHighlighter* hightlighter(){return m_hightlighter;}
    inline Indenter* indenter(){return m_indenter;}
    inline AutoCompleter* autoCompleter(){return m_autoCompleter;}
    inline CodeFormatter* codeFormatter(){return m_codeFormatter;}


private:
    SyntaxHighlighter* m_hightlighter;
    Indenter* m_indenter;
    AutoCompleter* m_autoCompleter;
    CodeFormatter* m_codeFormatter;


};
}
#endif // LANGUAGELOADER_H
