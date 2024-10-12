#include "cssscanner.h"
#include "languages/html/htmlscanner.h"
#include <QDebug>
#include <algorithm>

using namespace Css;
using namespace Code;


namespace {
static const QString css_keywords[] = {
    QLatin1String("and"),
    QLatin1String("not"),
    QLatin1String("or"),
    QLatin1String("@character"),
    QLatin1String("@charset"),
    QLatin1String("@document"),
    QLatin1String("@font-face"),
    QLatin1String("@import"),
    QLatin1String("@keyframes"),
    QLatin1String("@media"),
    QLatin1String("@namespace"),
    QLatin1String("@page"),
    QLatin1String("@supports"),
    QLatin1String("@viewport"),
};
} // end of anonymous namespace

template <typename _Tp, int N>
const _Tp *begin(const _Tp (&a)[N])
{
    return a;
}

template <typename _Tp, int N>
const _Tp *end(const _Tp (&a)[N])   
{
    return a + N;
}

Scanner::Scanner(Html::Scanner* htmlScanner)
    : _state(Normal),
    _scanComments(true),
    pHtmlScanner(htmlScanner)

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
    if(ch.isSpace()==false && ch != QLatin1Char('{') && ch != QLatin1Char(',') && ch != QLatin1Char(':') && ch != QLatin1Char(';') && ch != QLatin1Char('}') && ch != QLatin1Char('(') && ch != QLatin1Char(')') && ch != QLatin1Char('[') && ch != QLatin1Char(']') && ch != QLatin1Char(':') && ch != QLatin1Char('<') && ch != QLatin1Char('"') && ch != QLatin1Char('\'') && ch != QLatin1Char('@')){
        return true;
    }else{
        return false;
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



QList<Token> Scanner::operator()(int& from,const QString &text, int& startState)
{
    _state = startState;
    QList<Token> tokens;

    int index = from;


    if (isMarkState(_state,MultiLineComment)) {
        int start = -1;
        while (index < text.length()) {
            const QChar ch = text.at(index);

            if (start == -1 && !ch.isSpace())
                start = index;

            QChar la;
            if (index + 1 < text.length())
                la = text.at(index + 1);

            if (ch == QLatin1Char('*') && la == QLatin1Char('/')) {
                unSetMarkState(&_state,MultiLineComment);
                index += 2;
                break;
            } else {
                ++index;
            }
        }
        if (_scanComments && start != -1)
            tokens.append(Token(start, index - start, Token::Comment,Code::Token::Css));
    } else if (isMarkState(_state,MultiLineStringDQuote) || isMarkState(_state,MultiLineStringSQuote)) {
        const QChar quote = (_state == MultiLineStringDQuote ? QLatin1Char('"') : QLatin1Char('\''));
        const int start = index;
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
            unSetMarkState(&_state,MultiLineStringDQuote|MultiLineStringSQuote);
        }
        if (start < index)
            tokens.append(Token(start, index - start, Token::String,Code::Token::Css));
    }

    int start = -1;
    while (index < text.length()) {
        const QChar ch = text.at(index);
        //qDebug()<<"111111111111111111111:"<<ch;
        QChar la; // lookahead char
        if (index + 1 < text.length())
            la = text.at(index + 1);

        switch (ch.unicode()) {

        case '/':
            if (la == QLatin1Char('*')) {
                const int ss = index;
                index += 2;
                setMarkState(&_state, MultiLineComment);
                while (index < text.length()) {
                    const QChar ch = text.at(index);
                    QChar la;
                    if (index + 1 < text.length())
                        la = text.at(index + 1);

                    if (ch == QLatin1Char('*') && la == QLatin1Char('/')) {
                        unSetMarkState(&_state, MultiLineComment);
                        index += 2;
                        break;
                    } else {
                        ++index;
                    }
                }
                if (_scanComments)
                    tokens.append(Token(ss, index - ss, Token::Comment,Code::Token::Css));
            }else {
                tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Css));
            }
            break;

        case '\'':
        case '"': {
            const QChar quote = ch;
            const int ss = index;
            ++index;
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
                // good one
            } else {
                if (quote.unicode() == '"')
                    setMarkState(&_state, MultiLineStringDQuote);
                else
                    setMarkState(&_state, MultiLineStringSQuote);
            }
            tokens.append(Token(ss, index - ss, Token::String,Code::Token::Css));
        } break;
        case '(':
            tokens.append(Token(index++, 1, Token::LeftParenthesis,Code::Token::Css));
            break;
        case ')':
            tokens.append(Token(index++, 1, Token::RightParenthesis,Code::Token::Css));
            break;
        case '[':
            tokens.append(Token(index++, 1, Token::LeftBracket,Code::Token::Css));
            break;
        case ']':
            tokens.append(Token(index++, 1, Token::RightBracket,Code::Token::Css));
            break;
        case ',':
            tokens.append(Token(index++, 1, Token::Comma,Code::Token::Css));
            break;
        case '@':
            if(la.isLetter()){
                //find at rules
                int ss = index;
                index += 2;
                while (index < text.length()) {
                    const QChar ch = text.at(index);
                    if(!isIdentifierChar(ch)){
                        break;
                    }
                    ++index;
                }
                const int length = index - ss;
                tokens.append(Token(ss,length, Token::AtRules,Code::Token::Css));
                QStringView sv = QStringView(text).mid(ss,length);
                if((length==8 && sv.startsWith(QLatin1String("@charset"),Qt::CaseInsensitive))
                    ||(length==7 && sv.startsWith(QLatin1String("@import"),Qt::CaseInsensitive))
                    ||(length==9 && sv.startsWith(QLatin1String("@namespace"),Qt::CaseInsensitive)) ){
                    setMarkState(&_state,AtRulesABegin);
                }else if((length==10 && sv.startsWith(QLatin1String("@font-face"),Qt::CaseInsensitive))
                           ||(length==5 && sv.startsWith(QLatin1String("@page"),Qt::CaseInsensitive))
                           ||(length==9 && sv.startsWith(QLatin1String("@viewport"),Qt::CaseInsensitive)) ){
                     setMarkState(&_state,AtRulesBBegin);
                }else if((length==10 && sv.startsWith(QLatin1String("@keyframes"),Qt::CaseInsensitive))
                           ||(length==6 && sv.startsWith(QLatin1String("@media"),Qt::CaseInsensitive))
                           ||(length==9 && sv.startsWith(QLatin1String("@supports"),Qt::CaseInsensitive)) ){
                     setMarkState(&_state,AtRulesCBegin);
                }else{
                     setMarkState(&_state,AtRulesCBegin);//unknow
                }
            }else{
                tokens.append(Token(index,1, Token::Identifier,Code::Token::Css));
                ++index;
            }
            break;
        case '{':
            //qDebug()<<"{ current state:"<<_state;
            tokens.append(Token(index++,1, Token::LeftBrace,Code::Token::Css));

            if(isMarkState(_state,AtRulesBBegin)){
                setMarkState(&_state,AtRulesBBeginAttrList);//at rule attribute list
            }else if(isMarkState(_state,AtRulesCBeginSelectorList)){
                setMarkState(&_state,AtRulesCBeginSelectorAttrList);//at rule selector attribute list
            }else if(isMarkState(_state,AtRulesCBegin)){
                setMarkState(&_state,AtRulesCBeginSelectorList);//at rule selector list
            }else{
                setMarkState(&_state,MultiLineAttrList);//normal selector attribute list
            }
            //qDebug()<<"{ current state:"<<_state;
            break;
        case '}':
            tokens.append(Token(index++,1, Token::RightBrace,Code::Token::Css));
            if(isMarkState(_state,MultiLineAttrValue)){
                unSetMarkState(&_state,MultiLineAttrValue);
            }
            if(isMarkState(_state,AtRulesBBeginAttrList)){
                unSetMarkState(&_state,AtRulesBBeginAttrList | AtRulesBBegin);
            }else if(isMarkState(_state,AtRulesCBeginSelectorAttrList)){
                unSetMarkState(&_state,AtRulesCBeginSelectorAttrList);
            }else if(isMarkState(_state,AtRulesCBeginSelectorList)){
                unSetMarkState(&_state,AtRulesCBeginSelectorList | AtRulesCBegin);
            }else if(isMarkState(_state,MultiLineAttrList)){
                unSetMarkState(&_state,MultiLineAttrList);
            }
            break;
        case ';':
            tokens.append(Token(index++,1, Token::Semicolon,Code::Token::Css));
            if(isMarkState(_state,MultiLineAttrValue)){
                unSetMarkState(&_state,MultiLineAttrValue);
            }
            break;
        case ':':
            tokens.append(Token(index++,1, Token::Colon,Code::Token::Css));
            if(isMarkState(_state,MultiLineAttrList) || isMarkState(_state,AtRulesBBeginAttrList) || isMarkState(_state,AtRulesCBeginSelectorAttrList)){
                setMarkState(&_state,MultiLineAttrValue);
            }
            break;
        case '<':
            if(la == QLatin1Char('/')){
                //like  </
                if(this->pHtmlScanner!=nullptr){
                    //this->pHtmlScanner->endCSS();
                     unSetMarkState(&_state,Html::Scanner::MultiLineCss);
                     unSetMarkState(&_state,Html::Scanner::CSSTagStart);
                    goto result;
                }else{
                    tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Css));
                }
            }else{
                tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Css));
            }
            break;
        default:

            if(!ch.isSpace()){
                const int ss = index;
                index += 1;
                while(index < text.length()){

                    QChar la = text.at(index);
                    //qDebug()<<"default token:"<<text<<text.length()<<index<<la<<isIdentifierChar(la);

                    //is identifier char or is the last char
                    if(!isIdentifierChar(la)){
                        if(isMarkState(_state,MultiLineAttrValue)){
                            tokens.append(Code::Token(ss, index - ss, Code::Token::AttrValue,Code::Token::Css));
                        }else if(isMarkState(_state,MultiLineAttrList) || isMarkState(_state,AtRulesBBeginAttrList) || isMarkState(_state,AtRulesCBeginSelectorAttrList)){
                            tokens.append(Code::Token(ss, index - ss, Code::Token::AttrName,Code::Token::Css));
                        }else if(isMarkState(_state,AtRulesCBeginSelectorList)){
                            tokens.append(Code::Token(ss, index - ss, Code::Token::Selector,Code::Token::Css));
                        }else if(isMarkState(_state,AtRulesCBegin)){
                            tokens.append(Code::Token(ss, index - ss, Code::Token::Identifier,Code::Token::Css));
                        }else if(isMarkState(_state,AtRulesBBegin)){
                            tokens.append(Code::Token(ss, index - ss, Code::Token::Identifier,Code::Token::Css));
                        }else{
                            tokens.append(Code::Token(ss, index - ss, Code::Token::Selector,Code::Token::Css));
                        }
                        goto bk1;
                    }else if(index == text.length() - 1){
                        if(isMarkState(_state,MultiLineAttrValue)){
                            tokens.append(Code::Token(ss, index - ss + 1, Code::Token::AttrValue,Code::Token::Css));
                        }else if(isMarkState(_state,MultiLineAttrList) || isMarkState(_state,AtRulesBBeginAttrList) || isMarkState(_state,AtRulesCBeginSelectorAttrList)){
                            tokens.append(Code::Token(ss, index - ss + 1, Code::Token::AttrName,Code::Token::Css));
                        }else if(isMarkState(_state,AtRulesCBeginSelectorList)){
                            tokens.append(Code::Token(ss, index - ss + 1, Code::Token::Selector,Code::Token::Css));
                        }else if(isMarkState(_state,AtRulesCBegin)){
                            tokens.append(Code::Token(ss, index - ss + 1, Code::Token::Identifier,Code::Token::Css));
                        }else if(isMarkState(_state,AtRulesBBegin)){
                            tokens.append(Code::Token(ss, index - ss + 1, Code::Token::Identifier,Code::Token::Css));
                        }else{
                            tokens.append(Code::Token(ss, index - ss + 1, Code::Token::Selector,Code::Token::Css));
                        }
                        goto bk1;
                    }
                    ++index;
                }
            }
            ++index;
            bk1:
            break;
        } // end of switch
    }
    result:
    from = index;

    startState = _state;
    if(pHtmlScanner!=nullptr){
        pHtmlScanner->_state = _state;
    }
    return tokens;
}

int Scanner::state() const
{
    return _state;
}

bool Scanner::isKeyword(const QString &text) const
{
    return std::binary_search(begin(css_keywords), end(css_keywords), text);
}

QStringList Scanner::keywords()
{
    static QStringList words = [] {
        QStringList res;
        for (const QString *word = begin(css_keywords); word != end(css_keywords); ++word)
            res.append(*word);
        return res;
    }();
    return words;
}
