#include "phpscanner.h"
#include "languages/html/htmlscanner.h"
#include <QDebug>
#include <algorithm>

using namespace Php;

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


static inline int multiLineState(int state)
{
    return state & Scanner::MultiLineMask;
}

static inline void setMultiLineState(int *state, int s)
{
    *state = s | (*state & ~Scanner::MultiLineMask);
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

QList<Code::Token> Scanner::operator()(int& from,const QString &text, int& startState)
{
    _state = startState;
    QList<Code::Token> tokens;
    int index = from;
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
            tokens.append(Code::Token(start, index - start, Code::Token::Comment,Code::Token::Php));
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
            tokens.append(Code::Token(start, index - start, Code::Token::String,Code::Token::Php));
    } else if (multiLineState(_state) == MultiLineStringTQuote) {
        //<<<

    }

    auto braceCounterOffset = [](int templateDepth) {
        return FlagsBits + (templateDepth - 1) * BraceCounterBits;
    };

    while (index < text.length()) {
        const QChar ch = text.at(index);

        QChar la; // lookahead char
        if (index + 1 < text.length())
            la = text.at(index + 1);

        switch (ch.unicode()) {
        case '/':
            if (la == QLatin1Char('/')) {
                if (_scanComments)
                    tokens.append(Code::Token(index, text.length() - index, Code::Token::Comment,Code::Token::Php));
                index = text.length();
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
                    setMultiLineState(&_state, MultiLineStringDQuote);
                else
                    setMultiLineState(&_state, MultiLineStringSQuote);
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
            int depth = templateExpressionDepth(_state);
            /*if (depth > 0) {
                int shift = braceCounterOffset(depth);
                int mask = Scanner::TemplateExpressionOpenBracesMask0 << shift;
                if ((_state & mask) == mask) {
                    qWarning() << "php::Dom::Scanner reached maximum open braces of template expressions (127), parsing will fail";
                } else {
                    _state = (_state & ~mask) | (((Scanner::TemplateExpressionOpenBracesMask0 & (_state >> shift)) + 1) << shift);
                }
            }*/
        } break;

        case '}': {
            int depth = templateExpressionDepth(_state);
            /*if (depth > 0) {
                int shift = braceCounterOffset(depth);
                int s = _state;
                int nBraces = Scanner::TemplateExpressionOpenBracesMask0 & (s >> shift);
                int mask = Scanner::TemplateExpressionOpenBracesMask0 << shift;
                _state = (s & ~mask) | ((nBraces - 1) << shift);
                if (nBraces == 1) {
                    tokens.append(Code::Token(index++, 1, Code::Token::Delimiter,Code::Token::Php));
                    break;
                }
            }*/
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
            if (la == ch || la == QLatin1Char('=')) {
                tokens.append(Code::Token(index, 2, Code::Token::Delimiter,Code::Token::Php));
                index += 2;
            }else if (la == ch || la == QLatin1Char('?')) {
                tokens.append(Code::Token(index, 2, Code::Token::PhpLeftBracket,Code::Token::Php));
                index += 2;
            } else {

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
                    tokens.append(Code::Token(start, index - start, Code::Token::Keyword,Code::Token::Php)); // ### fixme
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
    return std::binary_search(begin(php_keywords), end(php_keywords), text);
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
