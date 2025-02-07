#include "loader.h"
#include "languages/html/htmlhighlighter.h"
#include "languages/html/htmlindenter.h"
#include "languages/html/htmlautocompleter.h"
#include "languages/html/htmlcodeformatter.h"

#include "languages/css/csshighlighter.h"
#include "languages/css/cssindenter.h"
#include "languages/css/cssautocompleter.h"
#include "languages/css/csscodeformatter.h"

#include "languages/javascript/jshighlighter.h"
#include "languages/javascript/jsindenter.h"
#include "languages/javascript/jsautocompleter.h"
#include "languages/javascript/jscodeformatter.h"

#include "languages/jsx/jsxhighlighter.h"
#include "languages/jsx/jsxindenter.h"
#include "languages/jsx/jsxautocompleter.h"
#include "languages/jsx/jsxcodeformatter.h"

#include "languages/python/pythonhighlighter.h"
#include "languages/python/pythonindenter.h"
//#include "languages/python/pythonautocompleter.h"

#include <QString>
namespace TextEditor{

LanguageLoader::LanguageLoader(const KSyntaxHighlighting::Definition &definition,QTextDocument* doc)
    :m_hightlighter(nullptr),m_indenter(nullptr),m_autoCompleter(nullptr),m_codeFormatter(nullptr){

    const QString name = definition.name();
    if(name==QLatin1String("PHP/PHP") || name == QLatin1String("HTML") || name == QLatin1String("XML")){
        m_hightlighter = new Html::Highlighter();
        m_indenter = Html::createIndenter(doc);
        m_autoCompleter = new Html::AutoCompleter();
        m_codeFormatter = new Html::CodeFormatter;
    }else if(name==QLatin1String("CSS")){
        m_hightlighter = new Css::Highlighter();
        m_indenter = Css::createIndenter(doc);
        m_autoCompleter = new Css::AutoCompleter();
        m_codeFormatter = new Css::CodeFormatter;
    }else if(name==QLatin1String("JavaScript") || name==QLatin1String("TypeScript")){
        m_hightlighter = new Javascript::Highlighter();
        m_indenter = Javascript::createIndenter(doc);
        m_autoCompleter = new Javascript::AutoCompleter();
        m_codeFormatter = new Javascript::CodeFormatter;
    }else if(name==QLatin1String("JavaScript React (JSX)") || name==QLatin1String("TypeScript React (TSX)")){
        m_hightlighter = new Jsx::Highlighter();
        m_indenter = Jsx::createIndenter(doc);
        m_autoCompleter = new Jsx::AutoCompleter();
        m_codeFormatter = new Jsx::CodeFormatter;
    }else if(name== QLatin1String("Python")){
        m_hightlighter = new Python::Highlighter();
        m_indenter = Python::createIndenter(doc);
        //m_autoCompleter = new Python::AutoCompleter();
    }

}



}
