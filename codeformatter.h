#ifndef CODEFORMATTER_H
#define CODEFORMATTER_H

#include "texteditor_global.h"

#include <QTextDocument>


namespace TextEditor{





class Indenter;
class TextDocument;

class TEXTEDITOR_EXPORT CodeFormatter
{
public:
    CodeFormatter();
    virtual ~CodeFormatter();

    // updates all states up until block if necessary
    // it is safe to call indentFor on block afterwards
    virtual void updateStateUntil(const QTextBlock &block);

    // calculates the state change introduced by changing a single line
    virtual void updateLineStateChange(const QTextBlock &block);

    virtual int indentFor(const QTextBlock &block);
    virtual int indentForNewLineAfter(const QTextBlock &block);

    virtual void setTabSize(int tabSize);

    virtual void invalidateCache(QTextDocument *document);
};

CodeFormatter* createCodeFormatter(TextDocument* textDoc,const QString& style,const QString& name);
Indenter* createCodeIndenter(QTextDocument* doc,const QString& style,const QString& name);





}

#endif // CODEFORMATTER_H
