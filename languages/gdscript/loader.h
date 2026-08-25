// GDScript language support for Godot Engine
#ifndef GDSCRIPT_LOADER_H
#define GDSCRIPT_LOADER_H
#include "texteditor_global.h"
#include "../loader.h"

namespace GDScript {
class TEXTEDITOR_EXPORT Loader : public TextEditor::LanguageLoader
{
public:
    explicit Loader(QTextDocument* doc);
};
}
#endif // GDSCRIPT_LOADER_H
