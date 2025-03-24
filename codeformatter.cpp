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

QList<Code::Token> CodeFormatter::tokenize(const QTextBlock& block){
    Q_UNUSED(block);
    return {};
}

QList<Code::Token> CodeFormatter::tokenize(const QString& text){
    Q_UNUSED(text);
    return {};
}

bool CodeFormatter::isInStringORCommentLiteral(const QTextBlock& block,int pos){
    if(pos<0 || pos >= block.length()){
        return false;
    }
    auto tokens = this->tokenize(block);
    for(auto tk:tokens){
        if(tk.offset<=pos && pos <(tk.offset+tk.length)){
            if(tk.kind==Code::Token::String || tk.kind==Code::Token::Comment){
                return true;
            }
        }
    }
    return false;
}

bool CodeFormatter::isIdentifier(QChar chr){
    return (chr.isLetterOrNumber() || chr == '_');
}

bool CodeFormatter::isVariantKind(int kind){
    return (kind == Code::Token::Keyword || kind==Code::Token::Identifier || kind==Code::Token::Variant);
}

}
