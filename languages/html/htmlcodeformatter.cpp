#include "htmlcodeformatter.h"
#include "tabsettings.h"
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QTextBlock>
#include <QTextDocument>

using namespace TextEditor;
using namespace Code;

namespace Html {
bool isAutoClose(QStringView tag){
    int length = tag.length();
    if(length==2 && (tag.startsWith(QLatin1String("hr"),Qt::CaseInsensitive) || tag.startsWith(QLatin1String("br"),Qt::CaseInsensitive))){
        return true;
    }else if(length==3 && (tag.startsWith(QLatin1String("img"),Qt::CaseInsensitive))){
        return true;
    }else if(length==4 && (tag.startsWith(QLatin1String("meta"),Qt::CaseInsensitive) || tag.startsWith(QLatin1String("link"),Qt::CaseInsensitive) || tag.startsWith(QLatin1String("base"),Qt::CaseInsensitive) || tag.startsWith(QLatin1String("area"),Qt::CaseInsensitive))){
        return true;
    }else if(length==5 && (tag.startsWith(QLatin1String("input"),Qt::CaseInsensitive) || tag.startsWith(QLatin1String("track"),Qt::CaseInsensitive))){
        return true;
    }else if(length==6 && (tag.startsWith(QLatin1String("source"),Qt::CaseInsensitive))){
        return true;
    }
    return false;
}


    CodeFormatter::CodeFormatter()
        : m_tokenIndex(0)
        , m_indentDepth(0)
        , m_tabSize(4)
        , m_indentSize(4)
        , phpFormatter(this)
        , jsFormatter(this)
        , cssFormatter(this)
    {


    }

    CodeFormatter::CodeFormatter(const TabSettings &tabSettings)
        : m_tokenIndex(0)
        , m_indentDepth(0)
        , m_tabSize(4)
        , phpFormatter(this)
        , jsFormatter(this)
        , cssFormatter(this)
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

        const int lexerState = tokenizeBlock(block);
        //qDebug()<<"lexerState:"<<lexerState<<block.text();
        m_tokenIndex = 0;
        m_newStates.clear();

        //qCDebug(formatterLog) << "Starting to look at " << block.text() << block.blockNumber() + 1;

        Scanner scanner;
        //qDebug()<<"token size:"<<m_tokens.size();
        //scanner.dump(block.text(),m_tokens);

        for (; m_tokenIndex < m_tokens.size(); ) {
            m_currentToken = tokenAt(m_tokenIndex);
            const int kind = extendedTokenKind(m_currentToken);
           // qDebug() << "Token" << m_currentLine.mid(m_currentToken.begin(), m_currentToken.length)<<m_currentToken.kind << m_tokenIndex << "in line" << block.blockNumber() + 1;
            if(kind == Token::PhpLeftBracket){
                enter(Php::CodeFormatter::topmost_intro_php);
                m_tokenIndex+=1;
            }
            if(m_currentToken.lang == Token::Php){
                phpFormatter.setTokens(m_tokens);
                phpFormatter.recalculateStateAfter(block,lexerState,m_currentLine,&m_tokenIndex);
                //m_tokenIndex += 1;
                continue;
            }else if(m_currentToken.lang == Token::Javascript){
                jsFormatter.setTokens(m_tokens);
                jsFormatter.recalculateStateAfter(block,lexerState,m_currentLine,&m_tokenIndex);
                m_tokenIndex += 1;
                continue;
            }else if(m_currentToken.lang == Token::Css){
                cssFormatter.setTokens(m_tokens);
                //qDebug()<<"CSS token:"<<block.text().mid(m_currentToken.offset,m_currentToken.length);
                cssFormatter.recalculateStateAfter(block,lexerState,m_currentLine,&m_tokenIndex);
                m_tokenIndex += 1;
                continue;
            }


            if (kind == Code::Token::Comment && state().type != comment_start && state().type != comment_content) {
                m_tokenIndex += 1;
                continue;
            }

            if(m_currentToken.length == 1 && kind==Token::TagRightBracket){
                //enter js
                if((lexerState & Scanner::MultiLineJavascript)== Scanner::MultiLineJavascript){
                    turnInto(Javascript::CodeFormatter::topmost_intro_js);
                    m_tokenIndex += 1;
                    continue;

                }else if((lexerState & Scanner::MultiLineCss)== Scanner::MultiLineCss){
                    turnInto(Css::CodeFormatter::topmost_intro_css);
                    m_tokenIndex += 1;
                    continue;

                }

            }
            if(kind == Token::TagLeftBracket){
                int type = m_currentState.top().type;
                if(type>=topmost_intro_css){
                    this->leaveCSS();
                    goto result;
                }else if(type>=topmost_intro_js){
                    this->leaveJS();
                    goto result;
                }
            }
            switch (m_currentState.top().type) {
            case html:
                if(kind == Token::TagLeftBracket && m_currentToken.length == 1){
                    //<
                    enter(open_tag);
                }else if(kind == Token::TagLeftBracket && m_currentToken.length == 2){
                    //</
                    const QStringView tok = QStringView(m_currentLine).mid(m_currentToken.begin(),m_currentToken.length);
                    if (tok == QLatin1String("</")){
                        turnInto(close_tag);
                    }
                }else if(kind == Token::CommentTagStart){
                    enter(comment_content);
                }
                break;
            case open_tag:
                if(kind == Token::TagStart){
                    const QStringView tok = QStringView(m_currentLine).mid(m_currentToken.begin(),m_currentToken.length);
                    if(isAutoClose(tok)){
                        //turnInto(html);
                        leave();//leave open_tag
                    }
                }else if(kind == Token::TagRightBracket){
                    const QStringView tok = QStringView(m_currentLine).mid(m_currentToken.begin(),m_currentToken.length);
                    if(tok.length() == 2 && tok.startsWith(QLatin1String("/>"))){
                        //close tag
                        leave();//leave open_tag
                    }else{
                        turnInto(html);//begin new html
                    }
                }
                break;
            case close_tag:
                if(kind == Token::TagRightBracket){
                    leave();//leave tag_inner
                }
                break;
            case Php::CodeFormatter::topmost_intro_php:
                if(kind == Token::PhpRightBracket){
                    leave();
                }
                break;
            case comment_start:
            case comment_content:
                //qDebug()<<"token:"<<currentTokenText() << m_tokenIndex;
                if (kind != Token::Comment) {
                    //qDebug()<<"comment_contentcomment_contentcomment_contentcomment_content";
                    leave();
                    continue;
                }
                break;

            default:
                qWarning() << "html Unhandled state" << m_currentState.top().type;
                break;
            } // end of state switch

            ++m_tokenIndex;
        }

        result:
        saveCurrentState(block);
    }

    int CodeFormatter::indentFor(const QTextBlock &block)
    {
        //qDebug() << "indenting for"<<block.text() << block.blockNumber() + 1;
        //qDebug()<<"indentFor:"<<m_indentDepth;
        restoreCurrentState(block.previous());
        correctIndentation(block);
        //()<<"indentFor:"<<m_indentDepth;
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

        //qDebug()<<"updateStateUntil end"<<stateToString(state().type);
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
        if (belowTop < m_currentState.size())
            return m_currentState.at(m_currentState.size() - 1 - belowTop);
        else
            return State();
    }

    const QVector<State> &CodeFormatter::newStatesThisLine() const
    {
        return m_newStates;
    }

    int CodeFormatter::tokenIndex() const
    {
        return m_tokenIndex;
    }

    int CodeFormatter::tokenCount() const
    {
        return m_tokens.size();
    }

    const Token &CodeFormatter::currentToken() const
    {
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

    void CodeFormatter::enter(int newState)
    {
        int savedIndentDepth = m_indentDepth;
        onEnter(newState, &m_indentDepth, &savedIndentDepth);
        State s(newState, savedIndentDepth);
        m_currentState.push(s);
        m_newStates.push(s);
        //qDebug()<<"token size:"<<m_tokens.length()<<m_tokenIndex;
        QString tag = m_currentLine.mid(m_currentToken.begin(),m_currentToken.length);
        if(m_tokenIndex < m_tokens.size() - 1){
            Token t = m_tokens.at(m_tokenIndex+1);
            tag += m_currentLine.mid(t.begin(),t.length);
        }
        //qDebug() << "html enter state" << stateToString(newState)<<tag<<"indent:"<<m_indentDepth <<"size:"<<m_currentState.size();

        if(newState == Javascript::CodeFormatter::topmost_intro_js){
            enter(Javascript::CodeFormatter::top_js);
        }else if(newState == Css::CodeFormatter::topmost_intro_css){
            enter(Css::CodeFormatter::top_css);
        }
    }

    void CodeFormatter::leave(bool statementDone)
    {
        if(m_currentState.size()<=1){
            return ;
        }
        if (m_currentState.top().type == invalid)
            return;

        if (m_newStates.size() > 0)
            m_newStates.pop();

        // restore indent depth
        State poppedState = m_currentState.pop();
        m_indentDepth = poppedState.savedIndentDepth;

        int topState = m_currentState.top().type;

        //qDebug() << "html left state" << stateToString(poppedState.type) << ", now in state" << stateToString(topState) <<"indent:"<<m_indentDepth<<"size:"<<m_currentState.size();

        // if statement is done, may need to leave recursively
        if (statementDone) {
            if (topState == topmost_intro_php) {
                leave(true);
            }
        }
    }

    void CodeFormatter::leavePHP(){
        while(m_currentState.size()>1){
            int type = m_currentState.top().type;
            if(type>=topmost_intro_php && type<topmost_intro_js){
                leave();
            }else{
                break;
            }
        }
    }

    void CodeFormatter::leaveJS(){
        while(m_currentState.size()>1){
            int type = m_currentState.top().type;
            if(type>=topmost_intro_js && type<topmost_intro_css){
                leave();
            }else{
                break;
            }
        }
    }

    void CodeFormatter::leaveCSS(){
        while(m_currentState.size()>1){
            int type = m_currentState.top().type;
            if(type>=topmost_intro_css){
                leave();
            }else{
                break;
            }
        }
    }

    void CodeFormatter::correctIndentation(const QTextBlock &block)
    {
        tokenizeBlock(block);
        Q_ASSERT(m_currentState.size() >= 1);
        const int startLexerState = loadLexerState(block.previous());
        adjustIndent(m_tokens, startLexerState, &m_indentDepth);
    }






    const Token &CodeFormatter::tokenAt(int idx) const
    {
        static const Token empty;
        if (idx < 0 || idx >= m_tokens.size())
            return empty;
        else
            return m_tokens.at(idx);
    }

    int CodeFormatter::column(int index) const
    {
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
        blockData.m_beginState = m_beginState;
        blockData.m_endState = m_currentState;
        blockData.m_indentDepth = m_indentDepth;

        QTextBlock saveableBlock(block);
        saveBlockData(&saveableBlock, blockData);
    }

    void CodeFormatter::restoreCurrentState(const QTextBlock &block)
    {
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
            initialState.push(State(html, 0));
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
        int from = 0;
        m_tokens = tokenize(from,m_currentLine, startState);

        const int lexerState = tokenize.state();
        QTextBlock saveableBlock(block);
        saveLexerState(&saveableBlock, lexerState);
        return lexerState;
    }

    int CodeFormatter::extendedTokenKind(const Code::Token &token) const
    {
        const int kind = token.kind;
        //const QStringView text = QStringView(m_currentLine).mid(token.begin(), token.length);
        return static_cast<TokenKind>(kind);
    }

    void CodeFormatter::dump() const
    {
        qDebug() << "Current token index" << m_tokenIndex;
        qDebug() << "Current state:";
        for (const State &s : m_currentState) {
            qDebug() << stateToString(s.type) << s.savedIndentDepth;
        }
        qDebug() << "Current indent depth:" << m_indentDepth;
    }

    QString CodeFormatter::stateToString(int type) const
    {
        const QMetaEnum &metaEnum = staticMetaObject.enumerator(staticMetaObject.indexOfEnumerator("StateType"));
        return QString::fromUtf8(metaEnum.valueToKey(type));
    }


    void CodeFormatter::onEnter(int newState, int *indentDepth, int *savedIndentDepth) const
    {
        const State &parentState = state();
        const Token &tk = currentToken();
        const int tokenPosition = column(tk.begin());
        const bool firstToken = (tokenIndex() == 0);
        const bool lastToken = (tokenIndex() == tokenCount() - 1);

        switch (newState) {
        case html: {
            // special case for things like "gradient: Gradient {"
            if (parentState.type == invalid)
                *savedIndentDepth = state(1).savedIndentDepth;

            if (firstToken)
                *savedIndentDepth = tokenPosition;

            if(parentState.type == invalid && firstToken){
                *indentDepth = 0;
            }else{
                *indentDepth = *savedIndentDepth + m_indentSize;
            }
            break;
        }
        case comment_content:
            *indentDepth = tokenPosition;

            break;
        }
    }

    void CodeFormatter::adjustIndent(const QList<Token> &tokens, int startLexerState, int *indentDepth) const
    {
        State topState = state();
        State previousState = state(1);
        //Html::Scanner scanner;
        //scanner.dump(m_currentLine,tokens);
        // keep user-adjusted indent in multiline comments
        if (topState.type == comment_start || topState.type == comment_content) {
            if (!tokens.isEmpty()) {
                *indentDepth = column(tokens.at(0).begin());
                return;
            }
        }
        // don't touch multi-line strings at all
        if ((startLexerState & Html::Scanner::MultiLineStringDQuote) == Scanner::MultiLineStringDQuote || (startLexerState & Scanner::MultiLineStringSQuote) == Scanner::MultiLineStringSQuote) {
            *indentDepth = -1;
            return;
        }
        Token tok = tokenAt(0);
        if(tok.lang == Code::Token::Php){
            //qDebug()<<"php adjustIndent"<<m_currentLine.mid(tok.begin(),tok.length)<<*indentDepth;
            phpFormatter.adjustIndent(tokens,startLexerState,indentDepth);
        }else if(tok.lang == Code::Token::Javascript){
            jsFormatter.adjustIndent(tokens,startLexerState,indentDepth);
        }else if(tok.lang == Code::Token::Css){
            cssFormatter.adjustIndent(tokens,startLexerState,indentDepth);
        }else{
            const int kind = extendedTokenKind(tok);
            switch (kind) {
            case Token::TagLeftBracket:
                if(tok.length==2){
                    *indentDepth = topState.savedIndentDepth;
                }
                break;
            }
        }
    }

    /*******************************EditorCodeFormatter  START *************************************/

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




}
