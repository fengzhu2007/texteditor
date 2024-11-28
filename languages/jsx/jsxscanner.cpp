#include "jsxscanner.h"
#include "../html/htmlscanner.h"
#include <QDebug>
#include <algorithm>

using namespace Jsx;
using namespace Code;

namespace {
static const QString jsx_keywords[] = {
    QLatin1String("as"),
    QLatin1String("async"),
    QLatin1String("await"),
    QLatin1String("break"),
    QLatin1String("case"),
    QLatin1String("catch"),
    QLatin1String("class"),
    QLatin1String("const"),
    QLatin1String("constructor"),
    QLatin1String("continue"),
    QLatin1String("debugger"),
    QLatin1String("default"),
    QLatin1String("delete"),
    QLatin1String("do"),
    QLatin1String("else"),
    QLatin1String("enum"),
    QLatin1String("export"),
    QLatin1String("extends"),
    QLatin1String("finally"),
    QLatin1String("for"),
    QLatin1String("from"),
    QLatin1String("function"),
    QLatin1String("if"),
    QLatin1String("implements"),
    QLatin1String("import"),
    QLatin1String("in"),
    QLatin1String("instanceof"),
    QLatin1String("interface"),
    QLatin1String("let"),
    QLatin1String("new"),
    QLatin1String("null"),
    QLatin1String("package"),
    QLatin1String("private"),
    QLatin1String("protected"),
    QLatin1String("public"),
    QLatin1String("React"),
    QLatin1String("return"),
    QLatin1String("static"),
    QLatin1String("super"),
    QLatin1String("switch"),
    QLatin1String("this"),
    QLatin1String("throw"),
    QLatin1String("try"),
    QLatin1String("typeof"),
    QLatin1String("undefined"),
    QLatin1String("var"),
    QLatin1String("void"),
    QLatin1String("while"),
    QLatin1String("with"),
    QLatin1String("yield")
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

Scanner::Scanner()
    : _state(Normal),
    _scanComments(true)
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

static bool isAttrName(QChar ch){
    switch (ch.unicode()) {
    case '-': case '_':
        return true;

    default:
        return ch.isLetterOrNumber();
    }
}

static bool isTagName(QChar ch){
    switch (ch.unicode()) {
    case '-': case '_':case '.':case ':':
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

static int findRegExpEnd(const QString &text, int start)
{
    if (start >= text.size() || text.at(start) != QLatin1Char('/'))
        return start;

    // find the second /
    int index = start + 1;
    for (; index < text.length(); ++index) {
        const QChar ch = text.at(index);

        if (ch == QLatin1Char('\\')) {
            ++index;
        } else if (ch == QLatin1Char('[')) {
            // find closing ]
            for (; index < text.length(); ++index) {
                const QChar ch2 = text.at(index);
                if (ch2 == QLatin1Char('\\')) {
                    ++index;
                } else if (ch2 == QLatin1Char(']'))
                    break;
            }
            if (index >= text.size())
                return text.size();
        } else if (ch == QLatin1Char('/'))
            break;
    }
    if (index >= text.size())
        return text.size();
    ++index;

    // find end of reg exp flags
    for (; index < text.size(); ++index) {
        const QChar ch = text.at(index);
        if (!isIdentifierChar(ch))
            break;
    }

    return index;
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

static inline bool regexpMayFollow(int state)
{
    //return state & Scanner::RegexpMayFollow;
    return isMarkState(state,Scanner::RegexpMayFollow);
}

static inline void setRegexpMayFollow(int *state, bool on)
{
    //*state = (on ? Scanner::RegexpMayFollow : 0) | (*state & ~Scanner::RegexpMayFollow);
    on?setMarkState(state,Scanner::RegexpMayFollow):unSetMarkState(state,Scanner::RegexpMayFollow);
}

static inline int templateExpressionDepth(int state)
{
    if((state & Scanner::TemplateExpressionOpenBracesMask4) == Scanner::TemplateExpressionOpenBracesMask4){
        return 4;
    }else if((state & Scanner::TemplateExpressionOpenBracesMask3) == Scanner::TemplateExpressionOpenBracesMask3){
        return 3;
    }else if((state & Scanner::TemplateExpressionOpenBracesMask2) == Scanner::TemplateExpressionOpenBracesMask2){
        return 2;
    }else if((state & Scanner::TemplateExpressionOpenBracesMask1) == Scanner::TemplateExpressionOpenBracesMask1){
        return 1;
    }else{
        return 0;
    }
}

static inline void setExpressionMask(int *state,int depth){
    if(depth>=4){
        *state |= (Scanner::TemplateExpressionOpenBracesMask0 |Scanner::TemplateExpressionOpenBracesMask1 | Scanner::TemplateExpressionOpenBracesMask2 | Scanner::TemplateExpressionOpenBracesMask3 | Scanner::TemplateExpressionOpenBracesMask4);
    }else if(depth == 3){
        *state |= (Scanner::TemplateExpressionOpenBracesMask0 |Scanner::TemplateExpressionOpenBracesMask1 | Scanner::TemplateExpressionOpenBracesMask2 | Scanner::TemplateExpressionOpenBracesMask3);
        *state &= ~Scanner::TemplateExpressionOpenBracesMask4;
    }else if(depth == 2){
        *state |= (Scanner::TemplateExpressionOpenBracesMask0 |Scanner::TemplateExpressionOpenBracesMask1 | Scanner::TemplateExpressionOpenBracesMask2);
        *state &= ~(Scanner::TemplateExpressionOpenBracesMask3 | Scanner::TemplateExpressionOpenBracesMask4);
    }else if(depth == 1){
        *state |= (Scanner::TemplateExpressionOpenBracesMask0 | Scanner::TemplateExpressionOpenBracesMask1);
        *state &= ~(Scanner::TemplateExpressionOpenBracesMask2 | Scanner::TemplateExpressionOpenBracesMask3 | Scanner::TemplateExpressionOpenBracesMask4);
    }else if(depth == 0){
        *state |= (Scanner::TemplateExpressionOpenBracesMask0 );
        *state &= ~(Scanner::TemplateExpressionOpenBracesMask1 | Scanner::TemplateExpressionOpenBracesMask2 | Scanner::TemplateExpressionOpenBracesMask3 | Scanner::TemplateExpressionOpenBracesMask4);
    }else{
        *state &= ~(Scanner::TemplateExpressionOpenBracesMask0 | Scanner::TemplateExpressionOpenBracesMask1 | Scanner::TemplateExpressionOpenBracesMask2 | Scanner::TemplateExpressionOpenBracesMask3 | Scanner::TemplateExpressionOpenBracesMask4);
    }
}

static inline int elementDepth(int state){
    if((state & Scanner::MultiLineElement6) == Scanner::MultiLineElement6){
        return 7;
    }else if((state & Scanner::MultiLineElement5) == Scanner::MultiLineElement5){
        return 6;
    }else if((state & Scanner::MultiLineElement4) == Scanner::MultiLineElement4){
        return 5;
    }else if((state & Scanner::MultiLineElement3) == Scanner::MultiLineElement3){
        return 4;
    }else if((state & Scanner::MultiLineElement2) == Scanner::MultiLineElement2){
        return 3;
    }else if((state & Scanner::MultiLineElement1) == Scanner::MultiLineElement1){
        return 2;
    }else if((state & Scanner::MultiLineElement) == Scanner::MultiLineElement){
        return 1;
    }else{
        return 0;
    }
}

static inline void setElementMask(int *state,int depth){
    *state &= ~(Scanner::MultiLineElementMask);//clear all
    if(depth>=7){
        *state |= Scanner::MultiLineElement6;
    }
    if(depth>=6){
        *state |= Scanner::MultiLineElement5;
    }
    if(depth>=5){
        *state |= Scanner::MultiLineElement4;
    }
    if(depth>=4){
        *state |= Scanner::MultiLineElement3;
    }
    if(depth>=3){
        *state |= Scanner::MultiLineElement2;
    }
    if(depth>=2){
        *state |= Scanner::MultiLineElement1;
    }
    if(depth>=1){
        *state |= Scanner::MultiLineElement;
    }
}

QList<Token> Scanner::operator()(int& from,const QString &text, int startState)
{
    qDebug()<<"start1111"<<startState;
    _state = startState;
    QList<Token> tokens;

    int index = from;
    int start = -1;

    auto scanTemplateString = [&index, &text, &tokens, this](int startShift = 0){
        const QChar quote = QLatin1Char('`');
        const int start = index + startShift;
        while (index < text.length()) {
            const QChar ch = text.at(index);
            if (ch == quote)
                break;
            else if (ch == QLatin1Char('$') && index + 1 < text.length() && text.at(index + 1) == QLatin1Char('{')) {
                tokens.append(Token(start, index - start, Token::String,Code::Token::Javascript));
                tokens.append(Token(index, 2, Token::Delimiter,Code::Token::Javascript));
                //qDebug()<<"token:"<<text.mid(index,2);
                index += 2;
                setRegexpMayFollow(&_state, true);
                //setMultiLineState(&_state, Normal);
                unSetMarkState(&_state,MultiLineStringBQuote);
                int depth = templateExpressionDepth(_state);
                if (depth == 4) {
                    qWarning() << "QQmljs::Dom::Scanner reached maximum nesting of template expressions (4), parsing will fail";
                } else {
                    //_state |= 1 << (4 + depth * 7);
                    setExpressionMask(&_state,depth+1);
                }
                return;
            } else if (ch == QLatin1Char('\\') && index + 1 < text.length())
                index += 2;
            else
                ++index;
        }

        if (index < text.length()) {
            //setMultiLineState(&_state, Normal);
            unSetMarkState(&_state,MultiLineStringBQuote);
            ++index;

            // good one
        } else {
            //setMultiLineState(&_state, MultiLineStringBQuote);
            setMarkState(&_state,MultiLineStringBQuote);
        }

        tokens.append(Token(start, index - start, Token::String,Code::Token::Javascript));
        setRegexpMayFollow(&_state, false);
    };

    if (isMarkState(_state,MultiLineComment)) {
        //qDebug()<<"comment"<<text;
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
            tokens.append(Token(start, index - start, Token::Comment,Code::Token::Javascript));
    } else if (isMarkState(_state,MultiLineStringDQuote)|| isMarkState(_state,MultiLineStringSQuote)) {
        //const QChar quote = (_state == MultiLineStringDQuote ? QLatin1Char('"') : QLatin1Char('\''));
        const QChar quote = isMarkState(_state,MultiLineStringDQuote)? QLatin1Char('"') : QLatin1Char('\'');
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
            //setMultiLineState(&_state, Normal);
            unSetMarkState(&_state,isMarkState(_state,MultiLineStringDQuote)?MultiLineStringDQuote:MultiLineStringSQuote);
        }
        if (start < index)
            tokens.append(Token(start, index - start, Token::String,Code::Token::Javascript));
        setRegexpMayFollow(&_state, false);
    } else if (isMarkState(_state,MultiLineStringBQuote)) {
        scanTemplateString();
    }


    while (index < text.length()) {
        const QChar ch = text.at(index);
        //qDebug()<<"ch"<<ch;
        QChar la; // lookahead char
        if (index + 1 < text.length())
            la = text.at(index + 1);

        //qDebug()<<"1111"<<_state<<ch;
        switch (ch.unicode()) {
        case '/':
            if (la == QLatin1Char('/')) {
                if (_scanComments)
                    tokens.append(Token(index, text.length() - index, Token::Comment,Code::Token::Javascript));
                index = text.length();
            } else if (la == QLatin1Char('*')) {
                const int start = index;
                index += 2;
                //setMultiLineState(&_state, MultiLineComment);
                setMarkState(&_state,MultiLineComment);
                while (index < text.length()) {
                    const QChar ch = text.at(index);
                    QChar la;
                    if (index + 1 < text.length())
                        la = text.at(index + 1);

                    if (ch == QLatin1Char('*') && la == QLatin1Char('/')) {
                        //setMultiLineState(&_state, Normal);
                        unSetMarkState(&_state,MultiLineComment);
                        index += 2;
                        break;
                    } else {
                        ++index;
                    }
                }
                if (_scanComments)
                    tokens.append(Token(start, index - start, Token::Comment,Code::Token::Javascript));
            }else if(la == QLatin1Char('>')){
                //<div/>
                tokens.append(Token(index, 2, Token::TagRightBracket,Code::Token::Javascript));
                index+=2;
                int depth = elementDepth(_state);
                setElementMask(&_state,depth-1);

            } else if (regexpMayFollow(_state)) {
                const int end = findRegExpEnd(text, index);
                tokens.append(Token(index, end - index, Token::RegExp,Code::Token::Javascript));
                index = end;
                setRegexpMayFollow(&_state, false);
            } else if (la == QLatin1Char('=')) {
                tokens.append(Token(index, 2, Token::Delimiter,Code::Token::Javascript));
                index += 2;
            } else {
                tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
                setRegexpMayFollow(&_state, true);
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
                    setMarkState(&_state,MultiLineStringDQuote);//setMultiLineState(&_state, MultiLineStringDQuote);
                else
                    setMarkState(&_state,MultiLineStringSQuote);//setMultiLineState(&_state, MultiLineStringSQuote);
            }

            tokens.append(Token(start, index - start, Token::String,Code::Token::Javascript));
            setRegexpMayFollow(&_state, false);
        } break;

        case '`': {
            ++index;
            scanTemplateString(-1);
        } break;

        case '.':
            if (la.isDigit()) {
                const int start = index;
                do {
                    ++index;
                } while (index < text.length() && isNumberChar(text.at(index)));
                tokens.append(Token(start, index - start, Token::Number,Code::Token::Javascript));
                break;
            }
            tokens.append(Token(index++, 1, Token::Dot,Code::Token::Javascript));
            setRegexpMayFollow(&_state, true);
            break;

        case '(':
            tokens.append(Token(index++, 1, Token::LeftParenthesis,Code::Token::Javascript));
            setRegexpMayFollow(&_state, true);
            break;

        case ')':
            tokens.append(Token(index++, 1, Token::RightParenthesis,Code::Token::Javascript));
            setRegexpMayFollow(&_state, false);
            break;

        case '[':
            tokens.append(Token(index++, 1, Token::LeftBracket,Code::Token::Javascript));
            setRegexpMayFollow(&_state, true);
            break;

        case ']':
            tokens.append(Token(index++, 1, Token::RightBracket,Code::Token::Javascript));
            setRegexpMayFollow(&_state, false);
            break;

        case '{':{
            tokens.append(Token(index++, 1, Token::LeftBrace,Code::Token::Javascript));
            setRegexpMayFollow(&_state, true);
            int depth = templateExpressionDepth(_state);
            if (depth > 0) {
                setExpressionMask(&_state,depth+1);
            }
        } break;

        case '}': {
            setRegexpMayFollow(&_state, false);
            int depth = templateExpressionDepth(_state);
            if (depth > 0) {
                setExpressionMask(&_state,depth-1);
                if(depth==1){
                    tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
                    scanTemplateString();
                    break;
                }
            }
            tokens.append(Token(index++, 1, Token::RightBrace,Code::Token::Javascript));
        } break;

        case ';':
            tokens.append(Token(index++, 1, Token::Semicolon,Code::Token::Javascript));
            setRegexpMayFollow(&_state, true);
            break;

        case ':':
            tokens.append(Token(index++, 1, Token::Colon,Code::Token::Javascript));
            setRegexpMayFollow(&_state, true);
            break;

        case ',':
            tokens.append(Token(index++, 1, Token::Comma,Code::Token::Javascript));
            setRegexpMayFollow(&_state, true);
            break;

        case '+':
        case '-':
        case '<':
            if(la.isLetter()){
                //<div>a<b</div>
                if(isMarkState(_state,MultiLineInnerText) && start>-1 && index>start){
                    //qDebug()<<"_state"<<_state<<start<<index;
                    tokens.append(Token(start, index - start, Token::InnerText));
                    unSetMarkState(&_state,MultiLineInnerText);
                }
                int from = index++;
                //find tag name
                int i = index;
                const int ss = index;
                QChar cc;
                int length = 0;
                while(i  < text.length()){
                    cc = text.at(i);
                    if(!isTagName(cc)){
                        break;
                    }
                    ++length;
                    ++i;
                }
                if(length>0){
                    if(cc.isSpace() || cc.isLetterOrNumber()){
                        //like <div
                        bool isTag = false;
                        if(tokens.size()==0){
                            isTag = true;
                        }else{
                            //check previous token is not
                            auto tk = tokens.last();
                            if(tk.kind!=Token::Identifier && tk.kind!=Token::RightParenthesis && tk.kind!=Token::RightBrace && tk.kind!=Token::RightBracket){
                                isTag = true;
                            }
                        }
                        if(isTag){
                            index = i;
                            tokens.append(Token(from, 1, Token::TagLeftBracket));
                            //TAG
                            tokens.append(Token(ss, length, Token::TagStart));
                            int depth = elementDepth(_state);
                            setElementMask(&_state,depth+1);
                            setMarkState(&_state,ElementStartTag);
                            qDebug()<<"state"<<_state<<ElementStartTag;
                            setRegexpMayFollow(&_state, false);//end JS expression
                            break;
                        }
                    }else if(cc==QLatin1Char('>')){
                        //like <div>
                        index = i;
                        tokens.append(Token(from, 1, Token::TagLeftBracket));
                        //TAG
                        tokens.append(Token(ss, length, Token::TagStart));
                        int depth = elementDepth(_state);
                        setElementMask(&_state,depth+1);
                        tokens.append(Token(index++, 1, Token::TagRightBracket));
                        setMarkState(&_state,MultiLineInnerText);
                        start = index;
                        setRegexpMayFollow(&_state, false);//end JS expression
                        break;
                    }else if((cc==QLatin1Char('/') && (i+1) < text.length() && text.at(i+1)==QLatin1Char('>'))){
                        //like <div/>
                        index = i;
                        tokens.append(Token(from, 1, Token::TagLeftBracket));
                        tokens.append(Token(ss, length, Token::TagStart));
                        tokens.append(Token(index, 2, Token::TagRightBracket));
                        index += 2;
                        setRegexpMayFollow(&_state, false);//end JS expression
                        break;
                    }

                }

                //lessthan
                tokens.append(Token(from, 1, Token::Delimiter,Code::Token::Javascript));

            }else if(la==QLatin1Char('>')){
                //<>
                tokens.append(Token(index++, 1, Token::TagLeftBracket));
                tokens.append(Token(index++, 1, Token::TagRightBracket));
                //int depth = elementDepth(_state);
                //setElementMask(&_state,depth-1);
                //begin inner text
                setMarkState(&_state,MultiLineInnerText);
                start = index;//inner start
                break;
            }else if(la == QLatin1Char('/')){
                //like  </
                tokens.append(Token(index, 2, Token::TagLeftBracket,Code::Token::Javascript));
                index+=2;
                unSetMarkState(&_state,MultiLineInnerText);
                setMarkState(&_state,ElementEndTag);
                //find end tag
                const int from = index;
                QChar cc ;
                while(index  < text.length()){
                    cc = text.at(index);
                    if(!isTagName(cc)){
                        break;
                    }
                    ++index;
                }

                int length = index - from;
                if(length>0){
                    if(cc.isSpace()){
                        tokens.append(Token(from, length, Token::TagStart));
                    }else if(cc == QLatin1Char('>')){
                        tokens.append(Token(from, length, Token::TagStart));
                        tokens.append(Token(index++, 1, Token::TagRightBracket));
                        int depth = elementDepth(_state);
                        setElementMask(&_state,depth-1);
                        setMarkState(&_state,ElementEndTag);
                    }else{
                        tokens.append(Token(from, length, Token::Identifier));
                    }
                }else{
                    tokens.append(Token(index++, 1, Token::TagRightBracket));
                    int depth = elementDepth(_state);
                    setElementMask(&_state,depth-1);
                    setMarkState(&_state,ElementEndTag);
                }
                break;
            }else if (la == ch || la == QLatin1Char('=')) {
                tokens.append(Token(index, 2, Token::Delimiter,Code::Token::Javascript));
                index += 2;
            }else if (la == ch || la == QLatin1Char('?')) {
                tokens.append(Token(index, 2, Token::Keyword,Code::Token::Javascript));
                index += 2;
            }else {
                tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
            }
            setRegexpMayFollow(&_state, true);
            break;
        case '>':
            if(isMarkState(_state,ElementStartTag)){
                //html element end tag
                tokens.append(Token(index++, 1, Token::TagRightBracket));
                //start html inner text
                setMarkState(&_state,MultiLineInnerText);
                qDebug()<<1111;
                unSetMarkState(&_state,ElementStartTag|MultiLineAttrValue);
                start = index;//text start
                break;
            }else if(isMarkState(_state,ElementEndTag)){
                tokens.append(Token(index++, 1, Token::TagRightBracket));
                unSetMarkState(&_state,ElementEndTag);
                int depth = elementDepth(_state);
                setElementMask(&_state,depth-1);
                if(depth>1){
                    setMarkState(&_state,MultiLineInnerText);
                    start = index;//text start
                }else{
                    //last html element end inner html
                    unSetMarkState(&_state,MultiLineInnerText);
                    start = -1;
                }
                break;
            }
            if (la == ch && index + 2 < text.length() && text.at(index + 2) == ch) {
                tokens.append(Token(index, 2, Token::Delimiter,Code::Token::Javascript));
                index += 3;
            } else if (la == ch || la == QLatin1Char('=')) {
                tokens.append(Token(index, 2, Token::Delimiter,Code::Token::Javascript));
                index += 2;
            } else {
                tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
            }
            setRegexpMayFollow(&_state, true);
            break;

        case '|':
        case '&':
            if (la == ch) {
                tokens.append(Token(index, 2, Token::Delimiter,Code::Token::Javascript));
                index += 2;
            } else {
                tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
            }
            setRegexpMayFollow(&_state, true);
            break;

        case '?':
            switch (la.unicode()) {
            case '?':
            case '.':
                tokens.append(Token(index, 2, Token::Delimiter,Code::Token::Javascript));
                index += 2;
                break;
            default:
                tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
                break;
            }
            setRegexpMayFollow(&_state, true);
            break;
        case '=':
            if(isMarkState(_state,ElementStartTag) && isMarkState(_state,RegexpMayFollow)==false){
                tokens.append(Token(index++, 1, Token::Equal,Code::Token::Javascript));
                setMarkState(&_state,MultiLineAttrValue);
                //find { or " or '
                while(index < text.size() && text.at(index).isSpace()){
                    ++index;
                }
                break;
            }else if (la == ch) {
                tokens.append(Token(index, 2, Token::Delimiter,Code::Token::Javascript));
                index += 2;
            } else {
                tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
            }
            setRegexpMayFollow(&_state, true);
            break;
        default:
            if (ch.isSpace()) {
                do {
                    ++index;
                } while (index < text.length() && text.at(index).isSpace());
            }else if(isMarkState(_state,ElementStartTag) && isMarkState(_state,RegexpMayFollow)==false){
                const int ss = index;
                //find attrname
                do {
                    ++index;
                } while (index < text.length() && isAttrName(text.at(index)));
                tokens.append(Token(ss, index - ss, Token::AttrName,Code::Token::Javascript));

            }else if(isMarkState(_state,MultiLineAttrValue) && isMarkState(_state,RegexpMayFollow)==false){
                //error attr value
                const int start = index;
                do {
                    ++index;
                } while (index < text.length() && text.at(index).isSpace()==false);
                tokens.append(Token(start, index - start, Token::AttrValue,Code::Token::Javascript));
            }else if (ch.isNumber()) {
                const int start = index;
                do {
                    ++index;
                } while (index < text.length() && isNumberChar(text.at(index)));
                tokens.append(Token(start, index - start, Token::Number,Code::Token::Javascript));
                setRegexpMayFollow(&_state, false);
            } else if (ch.isLetter() || ch == QLatin1Char('_') || ch == QLatin1Char('$')) {
                const int start = index;
                do {
                    ++index;
                } while (index < text.length() && isIdentifierChar(text.at(index)));

                if(text.mid(start,index-start)==QLatin1String("style")){
                    qDebug()<<"result"<<_state;
                }
                if (isKeyword(text.mid(start, index - start)))
                    tokens.append(Token(start, index - start, Token::Keyword,Code::Token::Javascript)); // ### fixme
                else
                    tokens.append(Token(start, index - start, Token::Identifier,Code::Token::Javascript));
                setRegexpMayFollow(&_state, false);
            } else {
                tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
                setRegexpMayFollow(&_state, true);
            }
        } // end of switch
    }
    from = index;
    //Html::Scanner().dump(text,tokens);
    qDebug()<<"result state"<<_state;
    return tokens;
}

int Scanner::state() const
{
    return _state;
}

bool Scanner::isKeyword(const QString &text) const
{
    return std::binary_search(begin(jsx_keywords), end(jsx_keywords), text);
}

QStringList Scanner::keywords()
{
    static QStringList words = [] {
        QStringList res;
        for (const QString *word = begin(jsx_keywords); word != end(jsx_keywords); ++word)
            res.append(*word);
        return res;
    }();
    return words;
}
