#pragma once

#include "texteditor_global.h"
#include "textindenter.h"

namespace Html {


TEXTEDITOR_EXPORT TextEditor::TextIndenter *createIndenter(QTextDocument *doc);

TEXTEDITOR_EXPORT void indent(QTextDocument *doc, int startLine, int endLine,const TextEditor::TabSettings &tabSettings);

}
