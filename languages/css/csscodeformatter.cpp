#include "csscodeformatter.h"
#include "languages/html/htmlcodeformatter.h"
#include "tabsettings.h"
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QTextBlock>
#include <QTextDocument>

using namespace TextEditor;
using namespace Code;

namespace Css {


CodeFormatter::CodeFormatter(Html::CodeFormatter* formatter)
    : m_tokenIndex(0)
    , m_indentDepth(0)
    , m_tabSize(4)
    , m_indentSize(4)
    , pHtmlFormatter(formatter)
{
}

CodeFormatter::CodeFormatter(const TabSettings &tabSettings,Html::CodeFormatter* formatter)
    : m_tokenIndex(0)
    , m_indentDepth(0)
    , m_tabSize(4)
    , pHtmlFormatter(formatter)
{
    setTabSize(tabSettings.m_tabSize);
    setIndentSize(tabSettings.m_indentSize);
}

CodeFormatter::~CodeFormatter()
{
}

void CodeFormatter::setTabSize(int tabSize)
{
    m_tabSize = tabSize;
}

void CodeFormatter::recalculateStateAfter(const QTextBlock &block)
{
    restoreCurrentState(block.previous());

    if(pHtmlFormatter!=nullptr){

        const int lexerState = pHtmlFormatter->tokenizeBlock(block);
        pHtmlFormatter->m_tokenIndex = 0;
        pHtmlFormatter->m_newStates.clear();

        //qDebug() << "1Starting to look at " << block.text() << block.blockNumber() + 1;

        recalculateStateAfter(block,lexerState,pHtmlFormatter->m_currentLine,&(pHtmlFormatter->m_tokenIndex));
    }else{
        const int lexerState = tokenizeBlock(block);
        m_tokenIndex = 0;
        m_newStates.clear();

        //qDebug() << "2Starting to look at " << block.text() << block.blockNumber() + 1;

        recalculateStateAfter(block,lexerState,m_currentLine,&m_tokenIndex);
    }

}


void CodeFormatter::recalculateStateAfter(const QTextBlock &block,int lexerState,const QString& currentLine,int* tokenIndex){
    //m_tokenIndex = *tokenIndex;
    m_currentLine = currentLine;
    //Html::Scanner scanner;
    //scanner.dump(currentLine,m_tokens);
    for (; *tokenIndex < m_tokens.size(); ) {
        m_currentToken = tokenAt(*tokenIndex);
        if(pHtmlFormatter!=nullptr){
            pHtmlFormatter->m_currentToken = m_currentToken;
        }

        const int kind = extendedTokenKind(m_currentToken);
        if (kind == Code::Token::Comment && state().type != multiline_comment_cont && state().type != multiline_comment_start) {
            *tokenIndex += 1;
            continue;
        }

        if(kind == Token::TagLeftBracket && m_currentToken.length==2){
            //*tokenIndex += 1;
            //html leave CSS
            if(pHtmlFormatter!=nullptr){
                pHtmlFormatter->leaveCSS();
            }
            break;
        }

        int type = pHtmlFormatter!=nullptr?pHtmlFormatter->m_currentState.top().type:m_currentState.top().type;
        //qDebug()<<stateToString(type)<<m_currentToken.kind << "Token CSS:" << m_currentLine.mid(m_currentToken.begin(), m_currentToken.length)<<m_currentToken.kind << m_tokenIndex << "in line" << block.blockNumber() + 1;
        switch (type) {
        case topmost_intro_css:
            enter(top_css); break;
        case top_css:
            tryStatement();
            break;
        case selector_open:
            if(kind == Token::LeftBrace){
                enter(selectorliteral_open);
            }
            break;
        case atrule_attribute_open:
            if(kind == Token::LeftBrace){
                enter(atrule_attributeliteral_open);
            }
            break;
        case atrule_selector_open:
            if(kind == Token::LeftBrace){
                enter(atrule_selectorliteral_open);
            }
            break;
        case atrule_inner_selector_open:
            if(kind == Token::LeftBrace){
                enter(atrule_inner_selectorliteral_open);
            }
            break;
        case selectorliteral_open:
        case atrule_attributeliteral_open:
        case atrule_inner_selectorliteral_open:
            if(kind == Token::RightBrace){
                leave(true);
            }
            break;
        case atrule_selectorliteral_open:
            if(kind == Token::RightBrace){
                leave(true);
            }else if(kind == Token::Selector){
                enter(atrule_inner_selector_open);
            }
            break;
        default:
            if(pHtmlFormatter!=nullptr){
                qWarning() << "css Unhandled state" << pHtmlFormatter->m_currentState.top().type;
            }else{
                qWarning() << "css Unhandled state" << m_currentState.top().type;
            }
            break;
        } // end of state switch

        ++*tokenIndex;
    }

    saveCurrentState(block);
}


int CodeFormatter::indentFor(const QTextBlock &block)
{
    //qDebug() << "indenting for" << block.blockNumber() + 1;
    restoreCurrentState(block.previous());
    correctIndentation(block);
    return m_indentDepth;
}

int CodeFormatter::indentForNewLineAfter(const QTextBlock &block)
{
    restoreCurrentState(block);
    m_tokens.clear();
    m_currentLine.clear();
    const int startLexerState = loadLexerState(block.previous());
    adjustIndent(m_tokens, startLexerState, &m_indentDepth);
    return m_indentDepth;
}

void CodeFormatter::updateStateUntil(const QTextBlock &endBlock)
{

    QStack<State> previousState = initialState();
    QTextBlock it = endBlock.document()->firstBlock();

    // find the first block that needs recalculation
    for (; it.isValid() && it != endBlock; it = it.next()) {
        BlockData blockData;
        if (!loadBlockData(it, &blockData))
            break;
        if (blockData.m_blockRevision != it.revision())
            break;
        if (previousState != blockData.m_beginState)
            break;
        if (loadLexerState(it) == -1)
            break;

        previousState = blockData.m_endState;
    }

    if (it == endBlock)
        return;

    // update everthing until endBlock
    for (; it.isValid() && it != endBlock; it = it.next()) {
        recalculateStateAfter(it);
    }

    // invalidate everything below by marking the state in endBlock as invalid
    if (it.isValid()) {
        BlockData invalidBlockData;
        saveBlockData(&it, invalidBlockData);
    }
}

void CodeFormatter::updateLineStateChange(const QTextBlock &block)
{
    if (!block.isValid())
        return;

    BlockData blockData;
    if (loadBlockData(block, &blockData) && blockData.m_blockRevision == block.revision())
        return;

    recalculateStateAfter(block);

    // invalidate everything below by marking the next block's state as invalid
    QTextBlock next = block.next();
    if (!next.isValid())
        return;

    saveBlockData(&next, BlockData());
}

State CodeFormatter::state(int belowTop) const
{
    if(pHtmlFormatter!=nullptr){
        if (belowTop < pHtmlFormatter->m_currentState.size())
            return pHtmlFormatter->m_currentState.at(pHtmlFormatter->m_currentState.size() - 1 - belowTop);
        else
            return State();
    }else{
        if (belowTop < m_currentState.size())
            return m_currentState.at(m_currentState.size() - 1 - belowTop);
        else
            return State();
    }
}

const QVector<State> &CodeFormatter::newStatesThisLine() const
{
    if(pHtmlFormatter!=nullptr){
        return pHtmlFormatter->newStatesThisLine();
    }
    return m_newStates;
}

int CodeFormatter::tokenIndex() const
{
    if(pHtmlFormatter!=nullptr){
        return pHtmlFormatter->m_tokenIndex;
    }
    return m_tokenIndex;
}

int CodeFormatter::tokenCount() const
{
    if(pHtmlFormatter!=nullptr){
        return pHtmlFormatter->tokenCount();
    }
    return m_tokens.size();
}

const Code::Token &CodeFormatter::currentToken() const
{
    if(pHtmlFormatter!=nullptr){
        return pHtmlFormatter->currentToken();
    }
    return m_currentToken;
}

void CodeFormatter::invalidateCache(QTextDocument *document)
{
    if (!document)
        return;

    BlockData invalidBlockData;
    QTextBlock it = document->firstBlock();
    for (; it.isValid(); it = it.next()) {
        saveBlockData(&it, invalidBlockData);
    }
}

void CodeFormatter::initState(){
    m_currentState.clear();
    m_currentState = initialState();
    m_beginState = m_currentState;
    m_indentDepth = 0;
}

void CodeFormatter::enter(int newState)
{
    //Token tok = m_tokens.at(m_tokenIndex);
    //qDebug()<<"enter:"<<this->stateToString(newState)<<m_currentLine.mid(tok.begin(),tok.length);
    if(pHtmlFormatter!=nullptr){
        int savedIndentDepth = pHtmlFormatter->m_indentDepth;//previous block indent
        //savedIndentDepth next block indent
        onEnter(newState, &(pHtmlFormatter->m_indentDepth), &savedIndentDepth);
        Code::State s(newState, savedIndentDepth);

        pHtmlFormatter->m_currentState.push(s);
        pHtmlFormatter->m_newStates.push(s);
        //qDebug() << "css enter state 1" << stateToString(newState)<<"current indent:"<<(pHtmlFormatter->m_indentDepth)<<"next indent:"<<savedIndentDepth;
        //Token tok = pHtmlFormatter->currentToken();
        //qDebug() << "css enter state 1" << stateToString(newState)<<pHtmlFormatter->m_currentLine.mid(pHtmlFormatter->m_currentToken.begin(),pHtmlFormatter->m_currentToken.length)<<"indent:"<<pHtmlFormatter->m_indentDepth<<"size:"<<pHtmlFormatter->m_currentState.size();
    }else{
        int savedIndentDepth = m_indentDepth;
        onEnter(newState, &m_indentDepth, &savedIndentDepth);
        Code::State s(newState, savedIndentDepth);
        m_currentState.push(s);
        m_newStates.push(s);
        //qDebug() << "css enter state 2" << stateToString(newState)<<m_currentLine.mid(m_currentToken.begin(),m_currentToken.length)<<"indent:"<<m_indentDepth<<"size:"<<m_currentState.size();

    }

}

void CodeFormatter::leave(bool statementDone)
{


    //Token tok = m_tokens.at(m_tokenIndex);
    //qDebug()<<"enter:"<<this->stateToString(m_currentState.top().type)<<m_currentLine.mid(tok.begin(),tok.length);;
    int topState;
    State poppedState;
    if(pHtmlFormatter!=nullptr){
        if (pHtmlFormatter->m_currentState.size()<=1)
            return;
        if (pHtmlFormatter->m_newStates.size() > 0)
            pHtmlFormatter->m_newStates.pop();

        poppedState = pHtmlFormatter->m_currentState.pop();
        topState = pHtmlFormatter->m_currentState.top().type;
        pHtmlFormatter->m_indentDepth = poppedState.savedIndentDepth;
        //qDebug() << "css left state1" << stateToString(poppedState.type) << currentTokenText() << ", now in state" << stateToString(topState)<<" now indent:"<<pHtmlFormatter->m_indentDepth;
    }else{

        if (m_currentState.size()<=1)
            return;
        if (m_newStates.size() > 0)
            m_newStates.pop();

        poppedState = m_currentState.pop();
        topState = m_currentState.top().type;
        m_indentDepth = poppedState.savedIndentDepth;
        //qDebug() << "css left state2" << stateToString(poppedState.type) << ", now in state" << stateToString(topState)<<m_indentDepth <<"size:"<<m_currentState.size()<<"indent:"<<m_indentDepth;
    }

    if(statementDone){
        if((poppedState.type == selectorliteral_open)
            ||(poppedState.type == atrule_attributeliteral_open)
            ||(poppedState.type == atrule_selectorliteral_open) || (poppedState.type == atrule_inner_selectorliteral_open)
            ){
            leave();
        }
    }

}

void CodeFormatter::correctIndentation(const QTextBlock &block)
{
    if(pHtmlFormatter!=nullptr){
        pHtmlFormatter->tokenizeBlock(block);
        Q_ASSERT(pHtmlFormatter->m_currentState.size() >= 1);
        const int startLexerState = loadLexerState(block.previous());
        adjustIndent(pHtmlFormatter->m_tokens, startLexerState, &pHtmlFormatter->m_indentDepth);

    }else{
        tokenizeBlock(block);
        Q_ASSERT(m_currentState.size() >= 1);
        const int startLexerState = loadLexerState(block.previous());
        adjustIndent(m_tokens, startLexerState, &m_indentDepth);

    }

}



bool CodeFormatter::tryStatement()
{
    const int kind = extendedTokenKind(m_currentToken);
    //qDebug()<<"tryStatement:"<<kind<<pHtmlFormatter->m_currentToken.kind<<pHtmlFormatter->m_currentLine.mid(pHtmlFormatter->m_currentToken.begin(),pHtmlFormatter->m_currentToken.length)

    switch (kind) {
    case AtViewport:
    case AtPage:
    case AtFontFace:
    case Code::Token::AtRules:
        enter(atrule_attribute_open);
        return true;
    case AtSupports:
    case AtMedia:
        enter(atrule_selector_open);
        return true;
    case Code::Token::Dot:
    case Code::Token::Selector:
        enter(selector_open);
        return true;
    }
    return false;
}

bool CodeFormatter::isBracelessState(int type) const
{

    return false;
}

bool CodeFormatter::isExpressionEndState(int type) const
{
    return false;
}

const Code::Token &CodeFormatter::tokenAt(int idx) const
{
    if(pHtmlFormatter!=nullptr){
        return pHtmlFormatter->tokenAt(idx);
    }
    static const Code::Token empty;
    if (idx < 0 || idx >= m_tokens.size())
        return empty;
    else
        return m_tokens.at(idx);
}

int CodeFormatter::column(int index) const
{
    if(pHtmlFormatter!=nullptr){
        return pHtmlFormatter->column(index);
    }
    int col = 0;
    if (index > m_currentLine.length())
        index = m_currentLine.length();

    const QChar tab = QLatin1Char('\t');

    for (int i = 0; i < index; i++) {
        if (m_currentLine[i] == tab)
            col = ((col / m_tabSize) + 1) * m_tabSize;
        else
            col++;
    }
    return col;
}

QStringView CodeFormatter::currentTokenText() const
{
    if(pHtmlFormatter!=nullptr){
        return pHtmlFormatter->currentTokenText();
    }
    return QStringView(m_currentLine).mid(m_currentToken.begin(), m_currentToken.length);
}

void CodeFormatter::turnInto(int newState)
{
    leave(false);
    enter(newState);
}

void CodeFormatter::saveCurrentState(const QTextBlock &block)
{
    if (!block.isValid())
        return;

    BlockData blockData;
    blockData.m_blockRevision = block.revision();
    blockData.m_beginState = pHtmlFormatter!=nullptr?pHtmlFormatter->m_beginState:m_beginState;
    blockData.m_endState = pHtmlFormatter!=nullptr?pHtmlFormatter->m_currentState:m_currentState;
    blockData.m_indentDepth = pHtmlFormatter!=nullptr?pHtmlFormatter->m_indentDepth:m_indentDepth;

    QTextBlock saveableBlock(block);
    saveBlockData(&saveableBlock, blockData);
}

void CodeFormatter::restoreCurrentState(const QTextBlock &block)
{
    if(pHtmlFormatter!=nullptr){
        pHtmlFormatter->recalculateStateAfter(block);
        return ;
    }
    if (block.isValid()) {
        BlockData blockData;
        if (loadBlockData(block, &blockData)) {
            m_indentDepth = blockData.m_indentDepth;
            m_currentState = blockData.m_endState;
            m_beginState = m_currentState;
            return;
        }
    }

    m_currentState = initialState();
    m_beginState = m_currentState;
    m_indentDepth = 0;
}

QStack<State> CodeFormatter::initialState()
{
    static QStack<State> initialState;
    if (initialState.isEmpty())
        initialState.push(State(topmost_intro_css, 0));
    return initialState;
}

int CodeFormatter::tokenizeBlock(const QTextBlock &block)
{
    int startState = loadLexerState(block.previous());
    if (block.blockNumber() == 0)
        startState = 0;
    Q_ASSERT(startState != -1);

    Scanner tokenize;
    tokenize.setScanComments(true);

    m_currentLine = block.text();
    // to determine whether a line was joined, Tokenizer needs a
    // newline character at the end
    m_currentLine.append(QLatin1Char('\n'));
    int index = 0;
    m_tokens = tokenize(index,m_currentLine, startState);

    const int lexerState = tokenize.state();
    QTextBlock saveableBlock(block);
    saveLexerState(&saveableBlock, lexerState);
    return lexerState;
}

CodeFormatter::TokenKind CodeFormatter::extendedTokenKind(const Code::Token &token) const
{
    const int kind = token.kind;
    const QStringView text = QStringView(pHtmlFormatter!=nullptr?pHtmlFormatter->m_currentLine:m_currentLine).mid(token.begin(), token.length);

    if (kind == Code::Token::Identifier) {
        if (text == QLatin1String("and"))
            return And;
        else if(text == QLatin1String("or")){
            return Or;
        }
    } else if (kind == Code::Token::AtRules) {
        const char char1 = text.at(1).toLatin1();
        switch (char1) {
        case 'c':
            return AtCharset;
        case 'i':
            return AtImport;
        case 'f':
            return AtFontFace;
        case 'p':
            return AtPage;
        case 'n':
            return AtNamespace;
        case 'm':
            return AtMedia;
        case 's':
            return AtSupports;
        case 'v':
            return AtViewport;
        }
    }

    return static_cast<TokenKind>(kind);
}

void CodeFormatter::dump() const
{
    qDebug() << "js Current token index" << m_tokenIndex;
    qDebug()<< "js Current state:";
    for (const State &s : pHtmlFormatter->m_currentState) {
        qDebug() << stateToString(s.type) << s.savedIndentDepth;
    }
    qDebug() << "js Current indent depth:" << m_indentDepth;
}

QString CodeFormatter::stateToString(int type) const
{
    const QMetaEnum &metaEnum = staticMetaObject.enumerator(staticMetaObject.indexOfEnumerator("StateType"));
    return QString::fromUtf8(metaEnum.valueToKey(type));
}



void CodeFormatter::onEnter(int newState, int *indentDepth, int *savedIndentDepth) const
{
    const State &parentState = state();
    const Code::Token &tk = currentToken();
    const int tokenPosition = column(tk.begin());
    const bool firstToken = (tokenIndex() == 0);
    const bool lastToken = (tokenIndex() == tokenCount() - 1);
    //qDebug()<<"onEnter:"<<*savedIndentDepth<<*indentDepth << stateToString(newState) << lastToken;
    switch (newState) {
    case selector_open:
    case atrule_attribute_open:
    case atrule_selector_open:

        break;
    case atrule_inner_selector_open:

        break;

    case selectorliteral_open:
    case atrule_attributeliteral_open:
    case atrule_selectorliteral_open:
        if (firstToken)
            *savedIndentDepth = tokenPosition;

        *indentDepth = *savedIndentDepth + m_indentSize;
        break;

    case atrule_inner_selectorliteral_open:
        //qDebug()<<"atrule_inner_selectorliteral_open"<<*savedIndentDepth<<pHtmlFormatter->m_currentLine;
        *savedIndentDepth += m_indentSize;
        //*savedIndentDepth += m_indentSize;
        *indentDepth = *savedIndentDepth ;
        break;
    case multiline_comment_start:
        *indentDepth = tokenPosition + 2;
        break;

    case multiline_comment_cont:
        *indentDepth = tokenPosition;
        break;
    }
}

void CodeFormatter::adjustIndent(const QList<Code::Token> &tokens, int startLexerState, int *indentDepth) const
{
    State topState = state();
    State previousState = state(1);


    // keep user-adjusted indent in multiline comments

    //qDebug()<<"adjustIndent:"<<this->stateToString(previousState.type);
    if (topState.type == multiline_comment_start || topState.type == multiline_comment_cont) {
        if (!tokens.isEmpty()) {
            *indentDepth = column(tokens.at(0).begin());
            return;
        }
    }

    const int kind = extendedTokenKind(tokenAt(0));
    //qDebug()<<"adjustIndent:current:"<<this->stateToString(topState.type)<<";previous:"<<this->stateToString(previousState.type)<<kind;
    switch (kind) {
    case Code::Token::LeftBrace:
        //qDebug()<<"adjust css top state:"<<stateToString(previousState.type)<<previousState.savedIndentDepth;
        //*indentDepth = previousState.savedIndentDepth + m_indentSize;
        //qDebug()<<"indent:"<<topState.savedIndentDepth;
        *indentDepth = topState.savedIndentDepth;
        break;
    case Code::Token::RightBrace: {
        *indentDepth = previousState.savedIndentDepth;
        break;
    }
    }


}



/********************************EditorCodeFormatter start ***********************************/



void CodeFormatter::setIndentSize(int size)
{
    m_indentSize = size;
}

void CodeFormatter::saveBlockData(QTextBlock *block, const BlockData &data) const
{
    TextBlockUserData *userData = TextDocumentLayout::userData(*block);
    auto cppData = static_cast<Code::CodeFormatterData *>(userData->codeFormatterData());
    if (!cppData) {
        cppData = new Code::CodeFormatterData;
        userData->setCodeFormatterData(cppData);
    }
    cppData->m_data = data;
}

bool CodeFormatter::loadBlockData(const QTextBlock &block, BlockData *data) const
{
    TextBlockUserData *userData = TextDocumentLayout::textUserData(block);
    if (!userData)
        return false;
    auto cppData = static_cast<const Code::CodeFormatterData *>(userData->codeFormatterData());
    if (!cppData)
        return false;

    *data = cppData->m_data;
    return true;
}

void CodeFormatter::saveLexerState(QTextBlock *block, int state) const
{
    TextDocumentLayout::setLexerState(*block, state);
}

int CodeFormatter::loadLexerState(const QTextBlock &block) const
{
    return TextDocumentLayout::lexerState(block);
}

QList<Code::Token> CodeFormatter::tokenize(const QTextBlock& block){
    const QTextBlock previous = block.previous();
    int startState = TextEditor::TextDocumentLayout::lexerState(previous);
    Q_ASSERT(startState != -1);
    Scanner tokenize;
    tokenize.setScanComments(true);

    QString text = block.text();
    text.append(QLatin1Char('\n'));
    int from = 0;
    return tokenize(from,text, startState);
}

QList<Code::Token> CodeFormatter::tokenize(const QString& text){
    Scanner tokenize;
    tokenize.setScanComments(true);
    int from = 0;
    int startState = 0;
    return tokenize(from,text, startState);
}

} // namespace css
