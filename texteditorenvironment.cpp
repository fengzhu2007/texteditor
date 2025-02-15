#include "texteditorenvironment.h"
#include "texteditorconstants.h"
#include "utils/theme/theme.h"
#include "utils/theme/theme_p.h"
#include "core/coreconstants.h"
#include "snippets/snippetprovider.h"

#include "languages/css/loader.h"
#include "languages/html/loader.h"
#include "languages/javascript/loader.h"
#include "languages/jsx/loader.h"
#include "languages/python/loader.h"
#include "languages/tsx/loader.h"

namespace TextEditor {

TextEditorEnvironment* TextEditorEnvironment::instance = nullptr;

TextEditorEnvironment::TextEditorEnvironment() {
    this->registerLangLoader<Html::Loader>({"HTML","PHP/PHP","XML"});
    this->registerLangLoader<Css::Loader>({"CSS"});
    this->registerLangLoader<Javascript::Loader>({"JavaScript"});
    this->registerLangLoader<Jsx::Loader>({"JavaScript React (JSX)"});
    this->registerLangLoader<Python::Loader>({"Python"});
    this->registerLangLoader<Tsx::Loader>({"TypeScript","TypeScript React (TSX)"});
}

void TextEditorEnvironment::init(){
    if(instance==nullptr){
        instance = new TextEditorEnvironment();
        Utils::Theme *theme = new Utils::Theme(Core::Constants::DEFAULT_DARK_THEME);
        Utils::setCreatorTheme(theme);
        TextEditor::SnippetProvider::registerGroup(TextEditor::Constants::TEXT_SNIPPET_GROUP_ID,QObject::tr("Text", "SnippetProvider"));
    }
}

void TextEditorEnvironment::destory(){
    Utils::setCreatorTheme(nullptr);
    delete instance;
    instance = nullptr;
}

TextEditorEnvironment* TextEditorEnvironment::getIntance(){
    return instance;
}

LanguageLoader* TextEditorEnvironment::loader(const QString& name,QTextDocument* doc){
    if(instance!=nullptr){
        for(auto one:m_loaderList){
            if(one.first.contains(name)){
                return one.second(doc);
            }
        }
    }
    return nullptr;
}

}
