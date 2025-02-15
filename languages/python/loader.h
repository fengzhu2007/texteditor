#ifndef PYTHON_LOADER_H
#define PYTHON_LOADER_H
#include "texteditor_global.h"
#include "../loader.h"

namespace Python {
class TEXTEDITOR_EXPORT Loader : public TextEditor::LanguageLoader
{
public:
    explicit Loader(QTextDocument* doc);
};
}
#endif // PYTHON_LOADER_H
