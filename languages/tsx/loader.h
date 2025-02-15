#ifndef TSX_LOADER_H
#define TSX_LOADER_H
#include "texteditor_global.h"
#include "../loader.h"

namespace Tsx {
class TEXTEDITOR_EXPORT Loader : public TextEditor::LanguageLoader
{
public:
    explicit Loader(QTextDocument* doc);
};
}
#endif // TSX_LOADER_H
