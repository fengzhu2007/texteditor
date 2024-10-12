#ifndef CSTYLEINDENTER_H
#define CSTYLEINDENTER_H

#include "texteditor_global.h"
#include "textindenter.h"

namespace CStyle {


TEXTEDITOR_EXPORT TextEditor::TextIndenter *createIndenter(QTextDocument *doc);

TEXTEDITOR_EXPORT void indent(QTextDocument *doc, int startLine, int endLine,const TextEditor::TabSettings &tabSettings);

}
#endif // CSTYLEINDENTER_H
