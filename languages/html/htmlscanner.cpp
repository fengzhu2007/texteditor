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
            int ss = index;
            //find end comment tag
            while(index + 2 < text.length()){
                if(text.at(index) == QLatin1Char('-') && text.at(index+1) == QLatin1Char('-') && text.at(index+2) == QLatin1Char('>')){

                    if(index>ss){
                        tokens.append(Token(ss, index - ss, Token::Comment));
                    }
                    //add comment end tag
                    tokens.append(Token(index, 3, Token::CommentTagEnd));
                    index += 3;
                    unSetMarkState(&_state,MultiLineComment);
                    ss = -1;
                    break;
                    //comment end
                }
                ++index;
            }
            if(ss>-1 && index > ss){
                tokens.append(Token(ss, index - ss, Token::Comment));

            }
            ++index;
            continue;
        }else if(isMarkState(_state,MultiLineXmlData)){
            int ss = index;
            //find end cdata tag
            //]]>
            while(index + 2 < text.length()){
                if(text.at(index) == QLatin1Char(']') && text.at(index+1) == QLatin1Char(']') && text.at(index+2) == QLatin1Char('>')){

                    if(index>ss){
                        tokens.append(Token(ss, index - ss, Token::XmlData));
                    }
                    //add comment end tag
                    tokens.append(Token(index, 3, Token::XmlDataRightTag));
                    index += 3;
                    unSetMarkState(&_state,MultiLineXmlData);
                    ss = -1;
                    break;
                    //comment end
                }
                ++index;
            }
            if(ss>-1 && index > ss){
                tokens.append(Token(ss, index - ss, Token::XmlData));
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
                            tokens.append(Token(ss, index - ss, Token::TagDefine));
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
                    }else if(index+8<text.size() && text.at(index+1)==QLatin1Char('!')
                               && text.at(index+2)==QLatin1Char('[')
                               && text.at(index+3)==QLatin1Char('C')
                               && text.at(index+4)==QLatin1Char('D')
                               && text.at(index+5)==QLatin1Char('A')
                               && text.at(index+6)==QLatin1Char('T')
                               && text.at(index+7)==QLatin1Char('A')
                               && text.at(index+8)==QLatin1Char('[')

                               ){
                        //<![CDATA[
                        tokens.append(Token(index, 9, Token::XmlDataLeftTag,Token::Html));
                        setMarkState(&_state,MultiLineXmlData);
                        index += 9;
                        break;
                    }
                }
                tokens.append(Token(index, 1, Token::InnerText));
            }else if(la== QLatin1Char('?')){
                if(maybyText() && start>-1 && index > start){
                    tokens.append(Token(start, index - start, Token::InnerText));
                }
                if(index+2 < text.size() && text.at(index+2)==QLatin1Char('x')){
                    //xml
                    tokens.append(Token(index, 2, Token::XmlLeftBracket,Code::Token::Html));
                    index+=2;
                    //find xml tag
                    int ss = index;
                    index+=1;
                    while(index < text.size()){
                        if(!text.at(index).isLetter()){
                            break;
                        }
                        index++;
                    }
                    tokens.append(Token(ss, index-start, Token::TagDefine,Code::Token::Html));
                    setMarkState(&_state,XMLTagStart | MultiLineElement);
                }else{
                    setMarkState(&_state,MultiLinePhp);
                }
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
                }else{
                    tokens.append(Token(index, 1, Token::UnComplate));
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
                if(isMarkState(_state,XMLTagStart)){
                    tokens.append(Code::Token(index, 2, Code::Token::XmlRightBracket,Code::Token::Html));
                    unSetMarkState(&_state,XMLTagStart | MultiLineElement);
                }else{
                    tokens.append(Code::Token(index, 2, Code::Token::PhpRightBracket,Code::Token::Html));
                }
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
                ++index;
                int ss = index;
                while(index < text.length()){
                    QChar ch = text.at(index);
                    if(!ch.isSpace()){
                        if(!isIdentifierChar(ch)){
                            goto bk2;
                        }
                        ss = index;
                        break;
                    }
                    ++index;
                }

                //++index;
                while(index<text.length()){
                    QChar ch = text.at(index);
                    if(!isIdentifierChar(ch)){
                        break;
                    }
                    ++index;
                }
                if(ss < index){
                    tokens.append(Token(ss, index - ss, Token::AttrName));
                }

                /*int ss = -1;
                while(index < text.length()){
                    QChar ch = text.at(index);
                    qDebug()<<"ccc:"<<ch<<ch.isSpace()<<isIdentifierChar(ch);
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
                qDebug()<<"ch:"<<ch<<la<<text<<ss<<index;
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
                }*/
                //++index;
                bk2:
                break;
            }else{

                //find first not space char
                int start = -1;
                while(index < text.length()){
                    const QChar ch = text.at(index);
                    if(!ch.isSpace()){
                        start = index;
                        break;
                    }
                    ++index;
                }
                //find <
                while(index<text.length()){
                    const QChar ch = text.at(index);
                    if(ch==QLatin1Char('<')){
                        break;
                    }
                    ++index;
                }
                if(start > -1 && start<index){
                    tokens.append(Token(start, index - start, Token::InnerText));
                }
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
        qDebug()<<"lang:"<<token.lang<<"kind:"<<token.kind<<"offset:"<<token.offset<<"length:"<<token.length<<";text:"<<text.mid(token.begin(),token.length);
    }
}


}
