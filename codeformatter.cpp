#include "codeformatter.h"

#include <QDebug>


namespace TextEditor{

CodeFormatter::CodeFormatter(){

}

CodeFormatter::~CodeFormatter(){

}

void CodeFormatter::updateStateUntil(const QTextBlock &block){

}

void CodeFormatter::updateLineStateChange(const QTextBlock &block){

}

int CodeFormatter::indentFor(const QTextBlock &block){
    return 0;
}

int CodeFormatter::indentForNewLineAfter(const QTextBlock &block){
    return 0;
}

void CodeFormatter::setTabSize(int tabSize){

}

void CodeFormatter::invalidateCache(QTextDocument *document){

}




}
