#include "codeformatter.h"
#include "languages/cstyle/cstyleqtcodeformatter.h"
#include "languages/cstyle/cstyleindenter.h"

#include "languages/python/pythonindenter.h"
#include "textindenter.h"

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


CodeFormatter* createCodeFormatter(TextDocument* textDoc,const QString& style,const QString& name){
    //qDebug()<<"createCodeFormatter"<<style<<name;
    if(!style.isEmpty()){
        if(style=="cstyle"){
            return new CStyle::CreatorCodeFormatter(textDoc->tabSettings());
        }
    }
    return nullptr;
}



Indenter* createCodeIndenter(QTextDocument* doc,const QString& style,const QString& name){
    if(!style.isEmpty()){
        if(style=="cstyle"){
            return CStyle::createIndenter(doc);
        }
    }
    if(!name.isEmpty()){
        if(name=="python"){
            return Python::createPythonIndenter(doc);
        }
    }
    return new TextIndenter(doc);
}



}
