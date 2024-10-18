#include "htmlscanner.h"
#include <QDebug>
namespace Html{

Scanner::Scanner()
    : _state(Normal),
    _scanComments(true),
    phpScanner(this),
    jsScanner(this),
    cssScanner(this)
{
}

Scanner::~Scanner()
{
}

bool Scanner::scanComments() const
{
    return _scanComments;
}

void Scanner::setScanComments(bool scanComments)
{
    _scanComments = scanComments;
}

static bool isIdentifierChar(QChar ch)
{
    switch (ch.unicode()) {
    //< > / ! - = ' " ?
    case '<':
    case '>':
    case '/':
    case '!':
    case '=':
    case '\'':
    case '"':
    case '?':
        return false;
    default:
        return !ch.isSpace();
    }
}


static inline void setMarkState(int * state,int s){
    *state |= s;
}

static inline void unSetMarkState(int * state,int s){
    *state &= ~s;
}


static inline bool isMarkState(int state,int s){
    return (state & s)==s;
}

QList<Token> Scanner::operator()(int& from,const QString &text, int startState)
{
    //qDebug()<<"Scanner:"<<text<<text.length();
    _state = startState;
    QList<Token> tokens;
    int index = from;
    int start = -1;
    while (index < text.length()) {
        const QChar ch = text.at(index);

        QChar la; // lookahead char
        if (index + 1 < text.length())
            la = text.at(index + 1);

        if(isMarkState(_state,MultiLinePhp)){
            auto phpTokens = this->phpScanner(index,text,_state);
            tokens << phpTokens;
            continue;
        }else if(isMarkState(_state,MultiLineCss)){
            auto cssTokens = this->cssScanner(index,text,_state);
            tokens << cssTokens;
            continue;
        }else if(isMarkState(_state,MultiLineJavascript)){
            auto jsTokens = this->jsScanner(index,text,_state);
            tokens << jsTokens;
            continue;
        }else if(isMarkState(_state,MultiLineStringDQuote) || isMarkState(_state,MultiLineStringSQuote)){

            const QChar quote = ((_state & MultiLineStringDQuote)==MultiLineStringDQuote ? QLatin1Char('"') : QLatin1Char('\''));
            qDebug()<<"1111111111111:"<<quote;
            const int start = index;
            while (index < text.length()) {
                const QChar ch = text.at(index);
                if (ch == quote){
                    break;
                }else if (index + 1 < text.length() && ch == QLatin1Char('\\'))
                    index += 2;
                else
                    ++index;
            }
            if (index < text.length()) {
                ++index;
                unSetMarkState(&_state, MultiLineStringDQuote|MultiLineStringSQuote);
            }
            if (start < index)
                tokens.append(Code::Token(start, index - start, Code::Token::String,Code::Token::Html));

            continue;
        }else if(isMarkState(_state,MultiLineComment)){
            const int ss = index;
            //find end comment tag
            while(index + 2 < text.length()){
                if(text.at(index) == QLatin1Char('-') && text.at(index+1) == QLatin1Char('-') && text.at(index) == QLatin1Char('>')){
                    tokens.append(Token(ss, index - ss, Token::Comment));
                    //add comment end tag
                    tokens.append(Token(index, 3, Token::CommentTagEnd));
                    index += 3;
                    unSetMarkState(&_state,MultiLineComment);
                    break;
                    //comment end
                }
                ++index;
            }

            if(ch==QLatin1Char('-') && la==QLatin1Char('-') && index+2<text.length() && text.at(index+2) == QLatin1Char('>')){
                //end comment
                if(index>start){
                    tokens.append(Token(0,index - 1, Token::Comment));
                }
                tokens.append(Token(index, 3, Token::CommentTagEnd));
                index += 3;
                continue;
            }else{
                if(ch==QLatin1Char('\xd') && index > 0){
                    tokens.append(Token(start, index - 1, Token::Comment));
                }
            }
            ++index;
            continue;
        }else if(isMarkState(_state,MultiLineAttrValue)){
            start = index;
            QChar ch = text.at(index);
            while(index < text.length()){
                if(start==-1 && isIdentifierChar(ch)){
                    start = index;
                }else if(start>-1 && isIdentifierChar(ch)==false){
                    //add attr value
                    if(index > start)
                        tokens.append(Code::Token(start, index - start, Code::Token::AttrValue,Code::Token::Html));
                    unSetMarkState(&_state,MultiLineAttrValue);
                    break;
                }
                ch = text.at(++index);
            }
            continue;
        }


        /*

<  :  <div>,</div>,<!--
>  :  <div>,<br  />,-->
/  :  </div>,<br  />
!  :  <!--
-  :  <!--,-->

*/
        //< > / ! - = ' " ?

        switch (ch.unicode()) {
        case '<':
            if(la.isLetter()){
                if(maybyText() && start>-1 && index > start){
                    tokens.append(Token(start, index - start, Token::InnerText));
                }
                tokens.append(Token(index++, 1, Token::TagLeftBracket));
                //find tag name
                const int ss = index;
                while(index  < text.length()){
                    if(!isIdentifierChar(text.at(index))){
                        break;
                    }
                    ++index;
                }
                int length = index - ss;
                tokens.append(Token(ss, length, Token::TagStart));
                setMarkState(&_state,MultiLineElement);

                //script
                //style
                if(length==6){
                    const QStringView strv = QStringView(text).mid(ss,length);
                    if(strv.startsWith(QLatin1String("script"))){
                        //javascript try start
                        setMarkState(&_state,JavascriptTagStart);
                    }
                }else if(length == 5){
                    const QStringView strv = QStringView(text).mid(ss,length);
                    if(strv.startsWith(QLatin1String("style"))){
                        //css try start
                        setMarkState(&_state,CSSTagStart);
                    }
                }
                break;
            }else if(la == QLatin1Char('/') || la == QLatin1Char('!')){
                if(index + 2 < text.length()){
                    if(text.at(index + 2).isLetter()){
                        if(maybyText() && start>-1 && index > start){
                            tokens.append(Token(start, index - start, Token::InnerText));
                        }

                        tokens.append(Token(index, 2, Token::TagLeftBracket));
                        index += 2;
                        const int ss = index;
                        while(index  < text.length()){
                            if(!isIdentifierChar(text.at(index))){
                                break;
                            }
                            ++index;
                        }
                        if(la ==  QLatin1Char('/')){
                            tokens.append(Token(ss, index - ss, Token::TagEnd));
                        }else{
                            tokens.append(Token(ss, index - ss, Token::TagStart));
                        }
                        setMarkState(&_state,MultiLineElement);

                        break;
                    }else if(text.at(index+2) == QLatin1Char('-') && index+3 < text.length() && text.at(index+3) == QLatin1Char('-')){

                        if(maybyText() && start>-1 && index > start){
                            tokens.append(Token(start, index - start, Token::InnerText));
                        }

                        //add token
                        tokens.append(Token(index, 4, Token::CommentTagStart));
                        index += 4;
                        setMarkState(&_state,MultiLineComment);
                        const int ss = index;
                        //find end comment tag
                        while(index + 2 < text.length()){
                            if(text.at(index) == QLatin1Char('-') && text.at(index+1) == QLatin1Char('-') && text.at(index+2) == QLatin1Char('>')){
                                tokens.append(Token(ss, index - ss, Token::Comment));
                                //add comment end tag
                                tokens.append(Token(index, 3, Token::CommentTagEnd));
                                index += 3;
                                unSetMarkState(&_state,MultiLineComment);
                                break;
                                //comment end
                            }
                            ++index;
                        }
                        break;
                    }
                }
            }else if(la== QLatin1Char('?')){
                if(maybyText() && start>-1 && index > start){
                    tokens.append(Token(start, index - start, Token::InnerText));
                }
                setMarkState(&_state,MultiLinePhp);
                break;
            }else{
                tokens.append(Token(index, 1, Token::InnerText));
            }
            ++index;
            break;
        case '/':
            if(isMarkState(_state,MultiLineElement)){
                if(la == QLatin1Char('>')){
                    if(isMarkState(_state,JavascriptTagStart)){
                        unSetMarkState(&_state,JavascriptTagStart);
                    }else if(isMarkState(_state,CSSTagStart)){
                        unSetMarkState(&_state,CSSTagStart);
                    }
                    tokens.append(Token(index, 2, Token::TagRightBracket));
                    unSetMarkState(&_state,MultiLineElement);
                    index  += 2;
                    start = -1;
                    break;
                }
            }else{
                tokens.append(Token(index, 1, Token::InnerText));
            }
            ++index;
            break;
        case '>':
            if(isMarkState(_state,MultiLineElement)){
                tokens.append(Token(index, 1, Token::TagRightBracket));
                unSetMarkState(&_state,MultiLineElement);
                start = -1;
                if(isMarkState(_state,JavascriptTagStart)){
                    unSetMarkState(&_state,JavascriptTagStart);
                    setMarkState(&_state,MultiLineJavascript);

                }else if(isMarkState(_state,CSSTagStart)){
                    unSetMarkState(&_state,MultiLineCss);
                    setMarkState(&_state,MultiLineCss);
                }
            }else{
                tokens.append(Token(index, 1, Token::InnerText));
            }
            ++index;
            break;
        case '=':
            if(isMarkState(_state,MultiLineElement)){
                tokens.append(Token(index++, 1, Token::Equal));
                int ss = -1;
                //find first not empty
                while(index < text.length()){
                    QChar ch = text.at(index);
                    if(ch.isSpace()==false){
                        if(isIdentifierChar(ch)==false){
                            goto bk0;
                        }else{
                            ss = index;
                            break;
                        }
                    }
                    ++index;
                }

                // attr value end
                if(ss>-1){
                    ++index;
                    while(index < text.length()){
                        QChar ch = text.at(index);
                        if(!isIdentifierChar(ch)){
                            tokens.append(Token(ss, index - ss, Token::AttrValue));
                            goto bk0;
                            break;
                        }
                        ++index;
                    }
                }else{
                    setMarkState(&_state,MultiLineAttrValue);
                }
            }else{
                tokens.append(Token(index, 1, Token::InnerText));
            }
            ++index;
            bk0:
            break;
        case '\'':
        case '"':
            if(isMarkState(_state,MultiLineElement)){

                const QChar quote = ch;
                const int start = index;
                ++index;
                while (index < text.length()) {
                    const QChar ch = text.at(index);

                    if (ch == quote)
                        break;
                    else if (index + 1 < text.length() && ch == QLatin1Char('\\'))
                        index += 2;
                    else
                        ++index;
                }

                if (index < text.length()) {
                    ++index;
                    // good one
                } else {

                    if (quote.unicode() == '"')
                        setMarkState(&_state, MultiLineStringDQuote);
                    else
                        setMarkState(&_state, MultiLineStringSQuote);
                }

                tokens.append(Code::Token(start, index - start, Code::Token::String,Code::Token::Html));
                break;
            }else{
                tokens.append(Token(index, 1, Token::InnerText));
            }
            ++index;
            break;
        case  '?':
            if(la == QLatin1Char('>')){
                tokens.append(Code::Token(index, 2, Code::Token::PhpRightBracket,Code::Token::Html));
                index += 1;
            }else{
                tokens.append(Token(index, 1, Token::InnerText));
            }
            ++index;
            break;
        default:
            if(isMarkState(_state,MultiLineElement)){
                //attr name
                //find attr
                //first not space
                int ss = -1;
                while(index < text.length()){
                    QChar ch = text.at(index);
                    if(ch.isSpace()==false){
                        if(isIdentifierChar(ch)==false){
                            goto bk2;
                        }else{
                            ss = index;
                            break;
                        }
                    }
                    ++index;
                }

                if(ss>-1){
                     //find attr name end
                    ++index;
                    while(index < text.length()){
                        QChar ch = text.at(index);
                        if(!isIdentifierChar(ch)){
                            tokens.append(Token(ss, index - ss, Token::AttrName));
                            goto bk2;
                            break;
                        }
                        ++index;
                    }
                }
                ++index;
                bk2:
                break;
            }else{
                //inner html
                /*if(start==-1 && ch.isSpace()==false){
                    start = index;
                }*/
                const int start = index;
                //find <
                while(index<text.length()){
                    const QChar ch = text.at(index);
                    if(ch==QLatin1Char('<')){
                        break;
                    }
                    ++index;
                }
                if(start<index){
                    tokens.append(Token(start, index - start, Token::InnerText));
                }
                /*if(ch==QLatin1Char('\xd') && start>-1 && index > start){

                    tokens.append(Token(start, index - start, Token::InnerText));
                }*/
                //++index;
            }
            break;
        }
    }
    from = index;
    return tokens;
}

int Scanner::state() const
{
    return _state;
}

bool Scanner::isKeyword(const QString &text) const
{
    return false;
}


QStringList Scanner::keywords()
{
    return {};
}

void Scanner::dump(const QString &text,QList<Token> tokens){
    for(auto token:tokens){
        qDebug()<<"lang:"<<token.lang<<"kind:"<<token.kind<<"s:"<<token.begin()<<"e:"<<token.end()<<";text:"<<text.mid(token.begin(),token.length);
    }
}


}
