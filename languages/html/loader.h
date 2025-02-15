#ifndef HTML_LOADER_H
#define HTML_LOADER_H
#include "texteditor_global.h"
#include "../loader.h"

namespace Html {
class TEXTEDITOR_EXPORT Loader : public TextEditor::LanguageLoader
{
public:
    explicit Loader(QTextDocument* doc);
};
}
#endif // HTML_LOADER_H
