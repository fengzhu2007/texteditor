#ifndef TEXTEDITORENVIRONMENT_H
#define TEXTEDITORENVIRONMENT_H

#include "texteditor_global.h"
#include "languages/loader.h"
#include <functional>
#include <map>

namespace TextEditor {

using LoaderFunc = std::function<LanguageLoader*(QTextDocument*)>;

class TEXTEDITOR_EXPORT TextEditorEnvironment
{
public:
    static void init();
    static void destory();
    static TextEditorEnvironment* getIntance();

    template <typename T>
    void registerLangLoader(const QStringList& namelist){
        m_loaderList[namelist] = [](QTextDocument* doc){
            return new T(doc);
        };
    }

    LanguageLoader* loader(const QString& name,QTextDocument* doc);
private:
    TextEditorEnvironment();

private:
    static TextEditorEnvironment* instance;
    std::map<QStringList,LoaderFunc> m_loaderList;
};
}
#endif // TEXTEDITORENVIRONMENT_H
