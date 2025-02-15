#ifndef JSX_LOADER_H
#define JSX_LOADER_H
#include "texteditor_global.h"
#include "../loader.h"

namespace Jsx {
class TEXTEDITOR_EXPORT Loader : public TextEditor::LanguageLoader
{
public:
    explicit Loader(QTextDocument* doc);
};
}
#endif // JSX_LOADER_H
