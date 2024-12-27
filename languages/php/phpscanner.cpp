#include "phpscanner.h"
#include "languages/html/htmlscanner.h"
#include <QDebug>
#include <algorithm>

using namespace Php;
QString Scanner::TQuoteTag;

namespace {
static const QString php_keywords[] = {
    QLatin1String("abstract"),
    QLatin1String("and"),
    QLatin1String("array"),
    QLatin1String("as"),
    QLatin1String("break"),
    QLatin1String("callable"),
    QLatin1String("case"),
    QLatin1String("catch"),
    QLatin1String("class"),
    QLatin1String("clone"),
    QLatin1String("const"),
    QLatin1String("continue"),
    QLatin1String("declare"),
    QLatin1String("default"),
    QLatin1String("do"),
    QLatin1String("echo"),
    QLatin1String("else"),
    QLatin1String("elseif"),
    QLatin1String("enddeclare"),
    QLatin1String("endfor"),
    QLatin1String("endforeach"),
    QLatin1String("endif"),
    QLatin1String("endswitch"),
    QLatin1String("endwhile"),
    QLatin1String("enum"),
    QLatin1String("exit"),
    QLatin1String("extends"),
    QLatin1String("false"),
    QLatin1String("final"),
    QLatin1String("finally"),
    QLatin1String("fn"),
    QLatin1String("for"),
    QLatin1String("foreach"),
    QLatin1String("function"),
    QLatin1String("global"),
    QLatin1String("goto"),
    QLatin1String("if"),
    QLatin1String("implements"),
    QLatin1String("include"),
    QLatin1String("include_once"),
    QLatin1String("instanceof"),
    QLatin1String("insteadof"),
    QLatin1String("interface"),
    QLatin1String("list"),
    QLatin1String("match"),
    QLatin1String("namespace"),
    QLatin1String("new"),
    QLatin1String("or"),
    QLatin1String("print"),
    QLatin1String("private"),
    QLatin1String("protected"),
    QLatin1String("public"),
    QLatin1String("readonly"),
    QLatin1String("require"),
    QLatin1String("require_once"),
    QLatin1String("return"),
    QLatin1String("static"),
    QLatin1String("switch"),
    QLatin1String("throw"),
    QLatin1String("trait"),
    QLatin1String("true"),
    QLatin1String("try"),
    QLatin1String("use"),
    QLatin1String("var"),
    QLatin1String("while"),
    QLatin1String("xor"),
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
    switch (ch.unicode()) {
    case '$': case '_':
        return true;

    default:
        return ch.isLetterOrNumber();
    }
}

static bool isNumberChar(QChar ch)
{
    switch (ch.unicode()) {
    case '.':
    case 'e':
    case 'E': // ### more...
        return true;

    default:
        return ch.isLetterOrNumber();
    }
}


/*static inline int multiLineState(int state)
{
    return state & Scanner::MultiLineMask;
}

static inline void setMultiLineState(int *state, int s)
{
    *state = s | (*state & ~Scanner::MultiLineMask);
}*/


static inline void setMarkState(int * state,int s){
    *state |= s;
}

static inline void unSetMarkState(int * state,int s){
    *state &= ~s;
}


static inline bool isMarkState(int state,int s){
    return (state & s)==s;
}


QList<Code::Token> Scanner::operator()(int& from,const QString &text, int& startState)
{
    _state = startState;
    QList<Code::Token> tokens;
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
                //setMultiLineState(&_state, Normal);
                unSetMarkState(&_state, MultiLineComment);
                index += 2;
                break;
            } else {
                ++index;
            }
        }
        if (_scanComments && start != -1)
            tokens.append(Code::Token(start, index - start, Code::Token::Comment,Code::Token::Php));
    } else if (isMarkState(_state,MultiLineStringDQuote) || isMarkState(_state,MultiLineStringSQuote)) {
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
            //setMultiLineState(&_state, Normal);
            unSetMarkState(&_state,MultiLineStringDQuote|MultiLineStringSQuote);
        }
        if (start < index)
            tokens.append(Code::Token(start, index - start, Code::Token::String,Code::Token::Php));
    } else if (isMarkState(_state,MultiLineStringTQuote) || isMarkState(_state,PHPTQouteStart)) {
        if(isMarkState(_state,PHPTQouteStart)){
            unSetMarkState(&_state,PHPTQouteStart);
            setMarkState(&_state,MultiLineStringTQuote);
            //_state = (_state&~PHPTQouteStart);
            //_state |= MultiLineStringTQuote;
        }

        const int ss = index;
        Code::Token::Kind kind = Code::Token::String;
        while(index<text.size()){
            QChar ch = text.at(index);
            if(ch.isLetterOrNumber()==false && ch!=QLatin1Char('_')){
                if(index>ss){
                    int len = index - ss;
                    if(len==Scanner::TQuoteTag.length() && text.mid(ss,len)==Scanner::TQuoteTag){
                        //_state = Normal;//clear all multi
                        unSetMarkState(&_state,MultiLineStringTQuote);
                        kind = Code::Token::TQouteTag;
                        break;
                    }
                }
            }
            ++index;
        }
        if(index>ss){
            tokens.append(Code::Token(ss, index - ss, kind,Code::Token::Php));
        }
    }


    while (index < text.length()) {
        const QChar ch = text.at(index);
        QChar la; // lookahead char
        if (index + 1 < text.length())
            la = text.at(index + 1);

        switch (ch.unicode()) {
        case '/':
            if (la == QLatin1Char('/')) {
                int pos = text.indexOf('\n',index);
                if(pos==-1){
                    if (_scanComments){
                        tokens.append(Code::Token(index, text.length() - index, Code::Token::Comment,Code::Token::Php));
                    }
                    index = text.length();
                }else{
                    if (_scanComments){
                        tokens.append(Code::Token(index, pos - index, Code::Token::Comment,Code::Token::Php));
                    }
                    index = pos;
                }
            } else if (la == QLatin1Char('*')) {
                const int start = index;
                index += 2;
                setMarkState(&_state,MultiLineComment);
                while (index < text.length()) {
                    const QChar ch = text.at(index);
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
                if (_scanComments)
                    tokens.append(Code::Token(start, index - start, Code::Token::Comment,Code::Token::Php));
            }else if (la == QLatin1Char('=')) {
                tokens.append(Code::Token(index, 2, Code::Token::Delimiter,Code::Token::Php));
                index += 2;
            } else {
                tokens.append(Code::Token(index++, 1, Code::Token::Delimiter,Code::Token::Php));
            }
            break;

        case '\'':
        case '"': {
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

            tokens.append(Code::Token(start, index - start, Code::Token::String,Code::Token::Php));
        } break;

        case '.':
            if (la.isDigit()) {
                const int start = index;
                do {
                    ++index;
                } while (index < text.length() && isNumberChar(text.at(index)));
                tokens.append(Code::Token(start, index - start, Code::Token::Number,Code::Token::Php));
                break;
            }
            tokens.append(Code::Token(index++, 1, Code::Token::Dot,Code::Token::Php));
            break;

        case '(':
            tokens.append(Code::Token(index++, 1, Code::Token::LeftParenthesis,Code::Token::Php));
            break;

        case ')':
            tokens.append(Code::Token(index++, 1, Code::Token::RightParenthesis,Code::Token::Php));
            break;

        case '[':
            tokens.append(Code::Token(index++, 1, Code::Token::LeftBracket,Code::Token::Php));
            break;

        case ']':
            tokens.append(Code::Token(index++, 1, Code::Token::RightBracket,Code::Token::Php));
            break;

        case '{':{
            tokens.append(Code::Token(index++, 1, Code::Token::LeftBrace,Code::Token::Php));
        } break;

        case '}': {
            tokens.append(Code::Token(index++, 1, Code::Token::RightBrace,Code::Token::Php));
        } break;

        case ';':
            tokens.append(Code::Token(index++, 1, Code::Token::Semicolon,Code::Token::Php));
            break;

        case ':':
            tokens.append(Code::Token(index++, 1, Code::Token::Colon,Code::Token::Php));
            break;

        case ',':
            tokens.append(Code::Token(index++, 1, Code::Token::Comma,Code::Token::Php));
            break;

        case '+':
        case '-':
        case '<':
            if(la == ch && index + 3 < text.size() && text.at(index+1) == ch && text.at(index+2) == ch){
                tokens.append(Code::Token(index, 3, Code::Token::StringBracket,Code::Token::Php));
                index += 3;
                //find start words
                const int ss = index;
                while(index < text.size()){
                    QChar ch = text.at(index);
                    if(!ch.isLetterOrNumber()){
                        break;
                    }
                    ++index;
                }
                if(index > ss){
                    tokens.append(Code::Token(ss, index - ss, Code::Token::TQouteTag,Code::Token::Php));
                    setMarkState(&_state, PHPTQouteStart);
                    Scanner::TQuoteTag = text.mid(ss,index-ss);

                    //find last
                    const int sss = index+1;
                    index = text.size();
                    if(index>sss){
                        tokens.append(Code::Token(sss, index-sss, Code::Token::String,Code::Token::Php));
                    }
                }
            }else if (la == ch || la == QLatin1Char('=')) {
                tokens.append(Code::Token(index, 2, Code::Token::Delimiter,Code::Token::Php));
                index += 2;
            }else if (la == ch || la == QLatin1Char('?')) {
                tokens.append(Code::Token(index, 2, Code::Token::PhpLeftBracket,Code::Token::Php));
                index += 2;
            }else{
                tokens.append(Code::Token(index++, 1, Code::Token::Delimiter,Code::Token::Php));
            }
            break;

        case '>':
            if (la == ch && index + 2 < text.length() && text.at(index + 2) == ch) {
                tokens.append(Code::Token(index, 2, Code::Token::Delimiter,Code::Token::Php));
                index += 3;
            } else if (la == ch || la == QLatin1Char('=')) {
                tokens.append(Code::Token(index, 2, Code::Token::Delimiter,Code::Token::Php));
                index += 2;
            } else {
                tokens.append(Code::Token(index++, 1, Code::Token::Delimiter,Code::Token::Php));
            }
            break;

        case '|':
        case '=':
        case '&':
            if (la == ch) {
                tokens.append(Code::Token(index, 2, Code::Token::Delimiter,Code::Token::Php));
                index += 2;
            } else {
                tokens.append(Code::Token(index++, 1, Code::Token::Delimiter,Code::Token::Php));
            }
            break;

        case '?':
            switch (la.unicode()) {
            case '?':
            case '.':
                tokens.append(Code::Token(index, 2, Code::Token::Delimiter,Code::Token::Php));
                index += 2;
                break;
            case  '>':
                //end php script
                //_state = Normal;
                tokens.append(Code::Token(index, 2, Code::Token::PhpRightBracket,Code::Token::Php));
                index += 2;
                if(pHtmlScanner!=nullptr){
                    //pHtmlScanner->endPHP();
                    _state &= ~Html::Scanner::MultiLinePhp;
                    goto result;
                }
                break;
            default:
                tokens.append(Code::Token(index++, 1, Code::Token::Delimiter,Code::Token::Php));
                break;
            }
            break;

        default:
            if (ch.isSpace()) {
                do {
                    ++index;
                } while (index < text.length() && text.at(index).isSpace());
            } else if (ch.isNumber()) {
                const int start = index;
                do {
                    ++index;
                } while (index < text.length() && isNumberChar(text.at(index)));
                tokens.append(Code::Token(start, index - start, Code::Token::Number,Code::Token::Php));
            } else if (ch.isLetter() || ch == QLatin1Char('_') || ch == QLatin1Char('$')) {
                const int start = index;
                do {
                    ++index;
                } while (index < text.length() && isIdentifierChar(text.at(index)));

                if (isKeyword(text.mid(start, index - start)))
                    tokens.append(Code::Token(start, index - start, Code::Token::Keyword,Code::Token::Php));
                else
                    tokens.append(Code::Token(start, index - start, Code::Token::Identifier,Code::Token::Php));
            } else {
                tokens.append(Code::Token(index++, 1, Code::Token::Delimiter,Code::Token::Php));
            }
        } // end of switch
    }
    result:
    startState = _state;
    from = index;
    return tokens;
}

int Scanner::state() const
{
    return _state;
}

bool Scanner::isKeyword(const QString &text) const
{
    bool ret = std::binary_search(begin(php_keywords), end(php_keywords), text);
    if(ret==false){
        ret = text.size()==3 && text.startsWith(QLatin1String("php"),Qt::CaseInsensitive);
    }
    return ret;
}

QStringList Scanner::keywords()
{
    static QStringList words = [] {
        QStringList res;
        for (const QString *word = begin(php_keywords); word != end(php_keywords); ++word)
            res.append(*word);
        return res;
    }();
    return words;
}

QString Scanner::currentTQouteTag(){
    return TQuoteTag;
}

void Scanner::setCurrentTQouteTag(const QString& tag){
    TQuoteTag = tag;
}
