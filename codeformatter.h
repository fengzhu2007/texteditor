#ifndef CODEFORMATTER_H
#define CODEFORMATTER_H

#include "texteditor_global.h"
#include "languages/token.h"

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

    virtual QList<Code::Token> tokenize(const QTextBlock& block);
    virtual QList<Code::Token> tokenize(const QString& text);

    virtual bool isInStringORCommentLiteral(const QTextBlock& block,int pos);
    virtual bool isIdentifier(QChar chr);
    virtual bool isVariantKind(int kind);
};




}

#endif // CODEFORMATTER_H
