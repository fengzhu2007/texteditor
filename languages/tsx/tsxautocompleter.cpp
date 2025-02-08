#include "tsxautocompleter.h"

#include "tsxscanner.h"
#include "textdocumentlayout.h"
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QDebug>

using namespace Code;
using namespace Tsx;

static int blockStartState(const QTextBlock &block)
{
    int state = block.previous().userState();
    if (state == -1)
        return 0;
    else
        return state;
}

static Code::Token tokenUnderCursor(const QTextCursor &cursor)
{
    const QString blockText = cursor.block().text();
    int blockState = blockStartState(cursor.block());

    auto previous = cursor.block().previous();
    TextEditor::TextBlockUserData *userData = nullptr;
    QStack<int> stacks;
    if(previous.isValid()){
        userData = TextEditor::TextDocumentLayout::userData(previous);
        stacks = userData->stateStack();
    }

    Scanner tokenize;
    int index = 0;
    const QList<Code::Token> tokens = tokenize(index,blockText, blockState,stacks);
    const int pos = cursor.positionInBlock();
    int tokenIndex = 0;
    for (; tokenIndex < tokens.size(); ++tokenIndex) {
        const Code::Token &token = tokens.at(tokenIndex);
        //qDebug()<<"token:"<<token.kind<<"index:"<<tokenIndex<<token.begin()<<token.end();

        if (token.is(Code::Token::Comment,Code::Token::Javascript) || token.is(Code::Token::String,Code::Token::Javascript)) {
            if (pos > token.begin() && pos <= token.end())
                break;
        } else {
            if (pos >= token.begin() && pos < token.end())
                break;
        }
    }

    if (tokenIndex != tokens.size())
        return tokens.at(tokenIndex);

    return Code::Token();
}

static bool shouldInsertMatchingText(QChar lookAhead)
{
    switch (lookAhead.unicode()) {
    case '>': case '/':
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
    QTextDocument *doc = tc.document();
    int pos = tc.selectionEnd();

    // count the number of empty lines.
    int newlines = 0;
    for (int e = doc->characterCount(); pos != e; ++pos) {
        const QChar ch = doc->characterAt(pos);

        if (! ch.isSpace())
            break;
        else if (ch == QChar::ParagraphSeparator)
            ++newlines;
    }

    if (newlines <= 1 && doc->characterAt(pos) != QLatin1Char('}'))
        return true;

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

        auto previous = block.previous();
        TextEditor::TextBlockUserData *userData = nullptr;
        QStack<int> stacks;
        if(previous.isValid()){
            userData = TextEditor::TextDocumentLayout::userData(previous);
            stacks = userData->stateStack();
        }




        const QList<Token> tokens = tokenize(index,blockText, blockState,stacks);
        for (auto it = tokens.rbegin(); it != tokens.rend(); ++it) {
            if(it->kind==Token::TagStart){
                tag = blockText.mid(it->begin(),it->length);
                return tag;
            }else if(it->kind==Token::TagRightBracket){
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

        auto previous = block.previous();
        TextEditor::TextBlockUserData *userData = nullptr;
        QStack<int> stacks;
        if(previous.isValid()){
            userData = TextEditor::TextDocumentLayout::userData(previous);
            stacks = userData->stateStack();
        }

        const QList<Token> tokens = tokenize(index,blockText, blockState,stacks);
        for (auto it = tokens.rbegin(); it != tokens.rend(); ++it) {
            if(last==-1 || ((it->offset + it->length) < last)){
                if(it->kind==Token::TagStart || it->kind == Token::TagEnd){
                    tag = blockText.mid(it->begin(),it->length);
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
            last = -1;
        }
        block = block.previous();
    }
    return QString();

}

AutoCompleter::AutoCompleter() = default;

AutoCompleter::~AutoCompleter() = default;

bool AutoCompleter::contextAllowsAutoBrackets(const QTextCursor &cursor,
                                              const QString &textToInsert) const
{
    return true;

}

bool AutoCompleter::contextAllowsAutoQuotes(const QTextCursor &cursor,
                                            const QString &textToInsert) const
{

    return true;
}

bool AutoCompleter::contextAllowsElectricCharacters(const QTextCursor &cursor) const
{
    Code::Token token = tokenUnderCursor(cursor);
    switch (token.kind) {
    case Code::Token::Comment:
    case Code::Token::String:
        return false;
    default:
        return true;
    }
}

bool AutoCompleter::isInComment(const QTextCursor &cursor) const
{
    return tokenUnderCursor(cursor).is(Code::Token::Comment);
}



QString AutoCompleter::insertMatchingBrace(const QTextCursor &cursor,
                                           const QString &text,
                                           QChar lookAhead,
                                           bool skipChars,
                                           int *skippedChars,int* adjustPos) const
{
    if (text.length() != 1)
        return QString();

    if (! shouldInsertMatchingText(cursor))
        return QString();

    const QChar ch = text.at(0);
    qDebug()<<"ch"<<ch;
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


    switch (ch.unicode()) {
    case '>':{
        //find tag name
        int index = 0;
        Scanner tokenize;
        auto previous = block.previous();
        TextEditor::TextBlockUserData *userData = nullptr;
        QStack<int> stacks;
        int state = 0;
        if(previous.isValid()){
            userData = TextEditor::TextDocumentLayout::userData(previous);
            stacks = userData->stateStack();
            state =  TextEditor::TextDocumentLayout::lexerState(previous);
        }
        int position = cursor.positionInBlock();
        const QList<Token> tokens = tokenize(index,block.text().left(position), state,stacks);
        auto s = tokenize.statesStack();
        if(s.size()>0 && s.top()==Scanner::ElementStartTag){
            const QString tag = findNearlyTagName(cursor.block(),cursor.positionInBlock());
            if(!tag.isEmpty()){
                return QString("</"+tag+">");
            }
        }
        return QString();
    }
    case '/':{

        int before = cursor.columnNumber() - 1;
        if(before>-1 && block.text().at(before)==QChar('<')){
            const QString tag = findMatchedTagName(cursor.block(),cursor.columnNumber());
            if(!tag.isEmpty()){
                *adjustPos+=(tag.length()+1);
                return QString(tag+">");
            }
            return tag;
        }else{
            const int state =  TextEditor::TextDocumentLayout::lexerState(block);
            if((state & Tsx::Scanner::ElementStartTag) == Tsx::Scanner::ElementStartTag){
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

        auto previous = block.previous();
        TextEditor::TextBlockUserData *userData = nullptr;
        QStack<int> stacks;
        if(previous.isValid()){
            userData = TextEditor::TextDocumentLayout::userData(previous);
            stacks = userData->stateStack();
        }



        const QList<Token> tokens = tokenize(index,blockText, blockState,stacks);
        Token tk;
        for(int i=0;i<tokens.length();i++){
            tk = tokens.at(i);
            if(tk.offset+tk.length>=position){
                break;
            }
        }
        if(tk.length==1 && tk.kind==Token::TagRightBracket){
            cursor.insertBlock();
            cursor.insertText("");
            cursor.setPosition(position);
            return 1;
        }
    }
    return TextEditor::AutoCompleter::paragraphSeparatorAboutToBeInserted(cursor);
}
