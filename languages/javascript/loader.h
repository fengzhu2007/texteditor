#ifndef JS_LOADER_H
#define JS_LOADER_H
#include "texteditor_global.h"
#include "../loader.h"

namespace Javascript {
class TEXTEDITOR_EXPORT Loader : public TextEditor::LanguageLoader
{
public:
    explicit Loader(QTextDocument* doc);
};
}
#endif // JS_LOADER_H
