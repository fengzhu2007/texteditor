#ifndef CSS_LOADER_H
#define CSS_LOADER_H
#include "texteditor_global.h"
#include "../loader.h"

namespace Css {
class TEXTEDITOR_EXPORT Loader : public TextEditor::LanguageLoader
{
public:
    explicit Loader(QTextDocument* doc);
};
}
#endif // CSS_LOADER_H
