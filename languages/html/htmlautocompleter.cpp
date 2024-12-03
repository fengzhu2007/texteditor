#include "htmlautocompleter.h"
#include "htmlscanner.h"
#include "htmlcodeformatter.h"
#include "highlighter.h"
#include "codeassist/documentcontentcompletion.h"
#include "textdocumentlayout.h"

#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QDebug>



using namespace Html;

static int blockStartState(const QTextBlock &block)
{
    int state = block.previous().userState();
    if (state == -1)
        return 0;
    else
        return state ;
}

static Token tokenUnderCursor(const QTextCursor &cursor)
{
    const QString blockText = cursor.block().text();
    const int blockState = blockStartState(cursor.block());

    Scanner tokenize;
    int index = 0;
    const QList<Token> tokens = tokenize(index,blockText, blockState);
    const int pos = cursor.positionInBlock();
    int tokenIndex = 0;
    for (; tokenIndex < tokens.size(); ++tokenIndex) {
        const Token &token = tokens.at(tokenIndex);
        //qDebug()<<"token:"<<token.kind<<"index:"<<tokenIndex<<token.begin()<<token.end();

        if (token.is(Token::Comment,Token::Html) || token.is(Token::String,Token::Html)) {
            if (pos > token.begin() && pos <= token.end())
                break;
        } else {
            if (pos >= token.begin() && pos < token.end())
                break;
        }
    }

    if (tokenIndex != tokens.size())
        return tokens.at(tokenIndex);

    return Token();
}

static bool shouldInsertMatchingText(QChar lookAhead)
{
    switch (lookAhead.unicode()) {
    case '>': case '-':case '/':
    case '{': case '}':
    case ']': case ')':
    case ';': case ',':
    case '"': case '\'':
        return true;

    default:
        if (lookAhead.isSpace())
            return true;

        return false;
    } // switch
}

static bool shouldInsertMatchingText(const QTextCursor &tc)
{
    QTextDocument *doc = tc.document();
    return shouldInsertMatchingText(doc->characterAt(tc.selectionEnd()));
}

static bool shouldInsertNewline(const QTextCursor &tc)
{
    return false;
}

static bool isCompleteStringLiteral(QStringView text)
{
    if (text.length() < 2)
        return false;

    const QChar quote = text.at(0);

    if (text.at(text.length() - 1) == quote)
        return text.at(text.length() - 2) != QLatin1Char('\\'); // ### not exactly.

    return false;
}

static QString findNearlyTagName(const QTextBlock& bk,int position){
    QTextBlock block = bk;
    Scanner tokenize;
    QString tag;
    while(block.isValid()){
        const int blockState = blockStartState(block);
        const QString blockText = block.text();
        if(position>0){
            //find previous
            QChar ch = blockText.at(position-1);
            if(ch==QLatin1Char('/') || ch==QLatin1Char('?') || ch==QLatin1Char('-') || ch==QLatin1Char(']')){
                return tag;
            }
        }
        int index = 0;
        const QList<Token> tokens = tokenize(index,blockText, blockState);
        for (auto it = tokens.rbegin(); it != tokens.rend(); ++it) {
            if(it->kind==Token::TagStart){
                tag = blockText.mid(it->begin(),it->length);
                if(!Html::isAutoClose(QStringView(tag))){
                    return tag;
                }
            }else if(it->kind==Token::TagRightBracket || it->kind==Token::CommentTagEnd || it->kind==Token::PhpRightBracket || it->kind==Token::PhpLeftBracket){
                return QString();
            }
        }
        block = block.previous();
    }
    return QString();
}

static QString findMatchedTagName(const QTextBlock& bk,int last){
    QTextBlock block = bk;
    Scanner tokenize;
    QString tag;
    int status = 0;
    while(block.isValid()){
        const int blockState = blockStartState(block);
        const QString blockText = block.text();
        int index = 0;
        const QList<Token> tokens = tokenize(index,blockText, blockState);
        for (auto it = tokens.rbegin(); it != tokens.rend(); ++it) {
            if(last==-1 || ((it->offset + it->length) < last)){
                if(it->kind==Token::TagStart || it->kind == Token::TagEnd){
                    tag = blockText.mid(it->begin(),it->length);
                    if(!Html::isAutoClose(QStringView(tag))){
                        if(it->kind==Token::TagEnd){
                            status -= 1;
                        }else{
                            status += 1;
                            if(status>=1){
                                return tag;
                            }
                        }
                    }
                }
            }
            last = -1;
        }
        block = block.previous();
    }
    return QString();

}

AutoCompleter::AutoCompleter() = default;

AutoCompleter::~AutoCompleter() = default;

bool AutoCompleter::contextAllowsAutoBrackets(const QTextCursor &cursor,const QString &textToInsert) const
{
    return true;
}

bool AutoCompleter::contextAllowsAutoQuotes(const QTextCursor &cursor,const QString &textToInsert) const
{
    return true;
}

bool AutoCompleter::contextAllowsElectricCharacters(const QTextCursor &cursor) const
{
    Token token = tokenUnderCursor(cursor);
    switch (token.kind) {
    case Token::Comment:
    case Token::String:
    case Token::XmlData:
        return false;
    default:
        return true;
    }
}

bool AutoCompleter::isInComment(const QTextCursor &cursor) const
{
    return tokenUnderCursor(cursor).is(Token::Comment,Token::Html);
}


static inline bool isInHtml(int state){
    if((state&Html::Scanner::MultiLinePhp) == Html::Scanner::MultiLinePhp || (state&Html::Scanner::MultiLineJavascript) == Html::Scanner::MultiLineJavascript || (state&Html::Scanner::MultiLineCss) == Html::Scanner::MultiLineCss){
        return false;
    }else{
        return true;
    }
}

QString AutoCompleter::insertMatchingBrace(const QTextCursor &cursor,const QString &text,QChar lookAhead,bool skipChars,int *skippedChars,int* adjustPos) const
{
    if (text.length() != 1)
        return QString();

    if (! shouldInsertMatchingText(cursor))
        return QString();


    const QChar ch = text.at(0);
    switch (ch.unicode()) {
    case '(':
        return QString(QLatin1Char(')'));

    case '[':
        return QString(QLatin1Char(']'));

    case '{':
        return QString(); // nothing to do.

    case ')':
    case ']':
    case '}':
    case ';':
        if (lookAhead == ch && skipChars)
            ++*skippedChars;
        break;
    default:
        break;
    } // end of switch

    const QTextBlock block = cursor.block();
    const int state =  TextEditor::TextDocumentLayout::lexerState(block);
    if(!isInHtml(state)){
        return QString();
    }
    switch (ch.unicode()) {
    case '>':{
        //find tag name
        //not in php css js
        qDebug()<<"html text:"<<cursor.block().text();
        //qDebug()<<"position:"<<cursor.positionInBlock();
        const QString tag = findNearlyTagName(cursor.block(),cursor.positionInBlock());
        if(!tag.isEmpty()){
            return QString("</"+tag+">");
        }
        return QString();
    }
    case '-':{
        const int pos = cursor.position();
        const QStringView sv = QStringView(cursor.block().text());
        if(sv.mid(pos-3,3)==QLatin1String("<!-")){
            return QLatin1String("-->");
        }else{
            return QString();
        }
    }
    case '/':{
        //qDebug()<<"cursor:"<<cursor.columnNumber();

        //return {};
        int before = cursor.columnNumber() - 1;
        if(before>-1 && block.text().at(before)==QChar('<')){
            const QString tag = findMatchedTagName(cursor.block(),cursor.columnNumber());
            if(!tag.isEmpty()){
                *adjustPos+=(tag.length()+1);
                return QString(tag+">");
            }
            return tag;
        }else{
            if((state & Html::Scanner::MultiLineElement) == Html::Scanner::MultiLineElement){
                *adjustPos+=1;
                return QString(">");
            }else{
                return {};
            }
        }

    }
    default:
        break;
    } // end of switch

    return QString();
}

QString AutoCompleter::insertMatchingQuote(const QTextCursor &/*tc*/, const QString &text,
                                           QChar lookAhead, bool skipChars, int *skippedChars) const
{
    if (isQuote(text)) {
        if (lookAhead == text && skipChars)
            ++*skippedChars;
        else
            return text;
    }
    return QString();
}

QString AutoCompleter::insertParagraphSeparator(const QTextCursor &cursor) const
{
    if (shouldInsertNewline(cursor)) {
        QTextCursor selCursor = cursor;
        selCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        if (! selCursor.selectedText().trimmed().isEmpty())
            return QString();

        return QLatin1String("}\n");
    }

    return QLatin1String("}");
}

int AutoCompleter::paragraphSeparatorAboutToBeInserted(QTextCursor &cursor){
    QTextDocument *doc = cursor.document();
    int position = cursor.position();
    const QChar ch = doc->characterAt(position - 1);
    if(ch==QLatin1Char('>')){
        if (!contextAllowsAutoBrackets(cursor))
            return 0;

        // verify that we indeed do have an extra opening brace in the document
        QTextBlock block = cursor.block();
        const int blockState = blockStartState(block.previous());
        const QString blockText = block.text();

        const QString textFromCusror = block.text().mid(cursor.positionInBlock()).trimmed();
        if(textFromCusror.isEmpty()==true || textFromCusror.at(0)!=QLatin1Char('<')){
            return 0;
        }
        int index = 0;
        Scanner tokenize;
        const QList<Token> tokens = tokenize(index,blockText, blockState);
        Token tk;
        for(int i=0;i<tokens.length();i++){
            tk = tokens.at(i);
            if(tk.offset+tk.length>=position){
                break;
            }
        }
        if(tk.length==1 && tk.lang==Token::Html){
            cursor.insertBlock();
            cursor.insertText("");
            cursor.setPosition(position);
            return 1;
        }
    }
    return TextEditor::AutoCompleter::paragraphSeparatorAboutToBeInserted(cursor);
}

void AutoCompleter::languageState(int state,TextEditor::TextDocument* textDocument){
    int lang = 0;
    QString mineType;
    if((state & Scanner::MultiLineJavascript) == Scanner::MultiLineJavascript){
        lang = Token::Javascript;
        mineType = "text/javascript";
    }else if((state & Scanner::MultiLineCss) == Scanner::MultiLineCss){
        lang = Token::Css;
        mineType = "text/css";
    }else if((state & Scanner::MultiLinePhp) == Scanner::MultiLinePhp){
        lang = Token::Php;
        mineType = "text/x-php";
    }else{
        lang = Token::Html;
        mineType = "text/html";
    }
    if(m_lang!=lang){

        TextEditor::Highlighter::Definitions definitions = TextEditor::Highlighter::definitionsForMimeType(mineType);
        //qDebug()<<"languageState"<<definitions.length();
        if(!definitions.isEmpty()){
            const TextEditor::Highlighter::Definition def = definitions.constFirst();
            auto provider = static_cast<TextEditor::DocumentContentCompletionProvider*>(textDocument->completionAssistProvider());
            this->initProvider(def,provider);
        }
        m_lang = lang;
    }
}



/*bool AutoCompleter::isInStringLiteral(const QTextBlock& block,int pos){
    auto tokens = this->tokenizeBlock(block);
    for(auto tk:tokens){
        if(tk.offset<=pos && pos <(tk.offset+tk.length)){
            if(tk.kind==Code::Token::String){
                return true;
            }
        }
    }
    return false;
}*/
