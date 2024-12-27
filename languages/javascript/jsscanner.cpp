#include "jsscanner.h"
#include "languages/html/htmlscanner.h"
#include <QDebug>
#include <algorithm>

using namespace Javascript;
using namespace Code;

namespace {
static const QString js_keywords[] = {
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
    QLatin1String("false"),
    QLatin1String("finally"),
    QLatin1String("for"),
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
    QLatin1String("return"),
    QLatin1String("static"),
    QLatin1String("super"),
    QLatin1String("switch"),
    QLatin1String("this"),
    QLatin1String("throw"),
    QLatin1String("true"),
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


static inline int multiLineState(int state)
{
    return state & Scanner::MultiLineMask;
}

static inline void setMultiLineState(int *state, int s)
{
    *state = s | (*state & ~Scanner::MultiLineMask);
}

static inline bool regexpMayFollow(int state)
{
    return state & Scanner::RegexpMayFollow;
}

static inline void setRegexpMayFollow(int *state, bool on)
{
    *state = (on ? Scanner::RegexpMayFollow : 0) | (*state & ~Scanner::RegexpMayFollow);
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

QList<Token> Scanner::operator()(int& from,const QString &text, int& startState)
{
    _state = startState;
    QList<Token> tokens;

    int index = from;

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
                setMultiLineState(&_state, Normal);
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
            setMultiLineState(&_state, Normal);
            ++index;

            // good one
        } else {
            setMultiLineState(&_state, MultiLineStringBQuote);
        }

        tokens.append(Token(start, index - start, Token::String,Code::Token::Javascript));
        setRegexpMayFollow(&_state, false);
    };

    if (multiLineState(_state) == MultiLineComment) {
        int start = -1;
        while (index < text.length()) {
            const QChar ch = text.at(index);

            if (start == -1 && !ch.isSpace())
                start = index;

            QChar la;
            if (index + 1 < text.length())
                la = text.at(index + 1);

            if (ch == QLatin1Char('*') && la == QLatin1Char('/')) {
                setMultiLineState(&_state, Normal);
                index += 2;
                break;
            } else {
                ++index;
            }
        }
        //qDebug()<<"MultiLineCommentMultiLineCommentMultiLineComment";
        if (_scanComments && start != -1)
            tokens.append(Token(start, index - start, Token::Comment,Code::Token::Javascript));
    } else if (multiLineState(_state) == MultiLineStringDQuote || multiLineState(_state) == MultiLineStringSQuote) {
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
            setMultiLineState(&_state, Normal);
        }
        if (start < index)
            tokens.append(Token(start, index - start, Token::String,Code::Token::Javascript));
        setRegexpMayFollow(&_state, false);
    } else if (multiLineState(_state) == MultiLineStringBQuote) {
        scanTemplateString();
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
                setMultiLineState(&_state, MultiLineComment);
                while (index < text.length()) {
                    const QChar ch = text.at(index);
                    QChar la;
                    if (index + 1 < text.length())
                        la = text.at(index + 1);

                    if (ch == QLatin1Char('*') && la == QLatin1Char('/')) {
                        setMultiLineState(&_state, Normal);
                        index += 2;
                        break;
                    } else {
                        ++index;
                    }
                }
                if (_scanComments)
                    tokens.append(Token(start, index - start, Token::Comment,Code::Token::Javascript));
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
                    setMultiLineState(&_state, MultiLineStringDQuote);
                else
                    setMultiLineState(&_state, MultiLineStringSQuote);
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
            setRegexpMayFollow(&_state, false);
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
                //qDebug()<<ch<<"index0"<<index;
                /*int shift = braceCounterOffset(depth);
                int mask = Scanner::TemplateExpressionOpenBracesMask0 << shift;
                if ((_state & mask) == mask) {
                    qWarning() << "QQmljs::Dom::Scanner reached maximum open braces of template expressions (127), parsing will fail";
                } else {
                    _state = (_state & ~mask) | (((Scanner::TemplateExpressionOpenBracesMask0 & (_state >> shift)) + 1) << shift);
                }*/
                setExpressionMask(&_state,depth+1);
            }
        } break;

        case '}': {
            setRegexpMayFollow(&_state, false);
            int depth = templateExpressionDepth(_state);
            if (depth > 0) {
                //qDebug()<<ch<<"index1"<<index;
                /*int shift = braceCounterOffset(depth);
                int s = _state;
                int nBraces = Scanner::TemplateExpressionOpenBracesMask0 & (s >> shift);
                int mask = Scanner::TemplateExpressionOpenBracesMask0 << shift;
                _state = (s & ~mask) | ((nBraces - 1) << shift);
                if (nBraces == 1) {
                    tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
                    scanTemplateString();
                    break;
                }*/
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
            if (la == ch || la == QLatin1Char('=')) {
                tokens.append(Token(index, 2, Token::Delimiter,Code::Token::Javascript));
                index += 2;
            }else if (la == ch || la == QLatin1Char('?')) {
                tokens.append(Token(index, 2, Token::Keyword,Code::Token::Javascript));
                index += 2;
            }else if(la == QLatin1Char('/')){
                //like  </
                /*if(index + 2 < text.length() && text.at(index+2).isLetter() && this->pHtmlScanner!=nullptr){
                    //html tag
                    this->pHtmlScanner->endJS();
                }else{
                    tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
                }*/
                if(this->pHtmlScanner!=nullptr){
                    //this->pHtmlScanner->endJS();
                    //unSetMarkState(&_state,Html::Scanner::MultiLineCss);
                    //|Html::Scanner::JavascriptTagStart
                    _state &= ~(Html::Scanner::MultiLineJavascript | Html::Scanner::JavascriptTagStart);
                    goto result;
                }else{
                    tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
                }
            }else {
                tokens.append(Token(index++, 1, Token::Delimiter,Code::Token::Javascript));
            }
            setRegexpMayFollow(&_state, true);
            break;

        case '>':
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
        case '=':
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
                tokens.append(Token(start, index - start, Token::Number,Code::Token::Javascript));
                setRegexpMayFollow(&_state, false);
            } else if (ch.isLetter() || ch == QLatin1Char('_') || ch == QLatin1Char('$')) {
                const int start = index;
                do {
                    ++index;
                } while (index < text.length() && isIdentifierChar(text.at(index)));

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
    result:
    startState = _state;
    from = index;

    //qDebug()<<text;
    //Html::Scanner().dump(text,tokens);

    return tokens;
}

int Scanner::state() const
{
    return _state;
}

bool Scanner::isKeyword(const QString &text) const
{
    return std::binary_search(begin(js_keywords), end(js_keywords), text);
}

QStringList Scanner::keywords()
{
    static QStringList words = [] {
        QStringList res;
        for (const QString *word = begin(js_keywords); word != end(js_keywords); ++word)
            res.append(*word);
        return res;
    }();
    return words;
}
