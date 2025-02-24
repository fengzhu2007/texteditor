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
        m_hasPHPState = false;
    }

    CodeFormatter::CodeFormatter(const TabSettings &tabSettings)
        : m_tokenIndex(0)
        , m_indentDepth(0)
        , m_tabSize(4)
        , phpFormatter(this)
        , jsFormatter(this)
        , cssFormatter(this)
    {
        m_hasPHPState = false;
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

        //Scanner scanner;
        //qDebug()<<"token size:"<<m_tokens.size();
        //scanner.dump(block.text(),m_tokens);

        for (; m_tokenIndex < m_tokens.size(); ) {
            m_currentToken = tokenAt(m_tokenIndex);
            const int kind = extendedTokenKind(m_currentToken);
           // qDebug() << "Token" << m_currentLine.mid(m_currentToken.begin(), m_currentToken.length)<<m_currentToken.kind << m_tokenIndex << "in line" << block.blockNumber() + 1;
            if(kind == Token::PhpLeftBracket){
                this->enterPHP();
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
                    //qDebug()<<"js enter size:"<<m_currentState.size();
                    turnInto(Javascript::CodeFormatter::topmost_intro_js);
                    m_tokenIndex += 1;
                    continue;

                }else if((lexerState & Scanner::MultiLineCss)== Scanner::MultiLineCss){
                    turnInto(Css::CodeFormatter::topmost_intro_css);
                    m_tokenIndex += 1;
                    continue;

                }

            }
            int type = m_currentState.top().type;
            if(kind == Token::TagLeftBracket){

                if(type>=topmost_intro_css){
                    this->leaveCSS();
                    goto result;
                }else if(type>=topmost_intro_js){
                    this->leaveJS();
                    goto result;
                }
            }

            switch (type) {
            case top_html:
                if(kind == Token::TagLeftBracket && m_currentToken.length == 1){
                    //<
                    enter(html);
                    enter(open_tag);
                }
                break;
            case html:
                if(kind == Token::TagLeftBracket && m_currentToken.length == 1){
                    //<
                    enter(html);
                    enter(open_tag);
                }else if(kind == Token::TagLeftBracket && m_currentToken.length == 2){
                    //</
                    const QStringView tok = QStringView(m_currentLine).mid(m_currentToken.begin(),m_currentToken.length);
                    if (tok == QLatin1String("</")){
                        enter(close_tag);
                    }
                }else if(kind == Token::CommentTagStart){
                    //<!
                    enter(comment_content);
                }
                break;
            case open_tag:
                if(kind == Token::TagRightBracket){
                    if(m_currentToken.length==1){
                        //>
                        leave();
                    }else if(m_currentToken.length==2){
                        //  />
                        leave(true);//leave open_tag and html
                    }
                }else if(kind== Token::TagStart){
                    //auto_tag
                    const QStringView tok = QStringView(m_currentLine).mid(m_currentToken.begin(),m_currentToken.length);
                    if(isAutoClose(tok)){
                        enter(auto_tag);
                    }
                }
                break;
            case auto_tag:
                if(kind == Token::TagRightBracket){
                    leave();//leave auto_tag
                    leave(true);//leave  open_tag and html
                }
                break;
            case close_tag:
                if(kind == Token::TagRightBracket){
                    leave(true);//leave close_tag and html
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
                if(type>=topmost_intro_php && type <topmost_intro_js){
                    this->phpFormatter.recalculateStateAfter(block,lexerState,m_currentLine,&m_tokenIndex);
                    goto result;
                }else{
                    qWarning() << "html Unhandled state" << m_currentState.top().type;
                }
                break;
            } // end of state switch

            ++m_tokenIndex;
        }

        result:
        saveCurrentState(block);
    }

    int CodeFormatter::indentFor(const QTextBlock &block)
    {
        //qDebug()<<"11111:"<<m_indentDepth<<m_currentLine;
        restoreCurrentState(block.previous());
        //qDebug()<<"22222:"<<m_indentDepth<<m_currentLine;
        correctIndentation(block);

        //qDebug()<<"33333:"<<m_indentDepth<<m_currentLine;
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
        //qDebug() << "html enter state" << stateToString(newState)<<tag<<"indent:"<<m_indentDepth <<currentTokenText()<<"size:"<<m_currentState.size();

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
        if(/*poppedState.type==comment_content || */poppedState.type==topmost_intro_js || poppedState.type==top_css){
            m_indentDepth = m_currentState.top().savedIndentDepth;
        }else{
            m_indentDepth = poppedState.savedIndentDepth;
        }


        int topState = m_currentState.top().type;

        if(topState==top_html){
            m_codeStoredState.clear();
        }

        //qDebug() << "html left state" << stateToString(poppedState.type) << ", now in state" << stateToString(topState) <<"indent:"<<m_indentDepth<<currentTokenText()<<"size:"<<m_currentState.size();

        // if statement is done, may need to leave recursively
        if (statementDone) {

            if (topState == topmost_intro_php) {
                //qDebug()<<"topmost_intro_php"<<currentTokenText();
                leave(true);
            }else if(topState==html){
                leave();
            }
        }
    }

    void CodeFormatter::enterPHP(){
        //remove last html state
        auto savedIndentDepth = m_currentState.top().savedIndentDepth;
        while(m_currentState.size()>0){
            auto type = m_currentState.top().type;
            if(type<topmost_intro_php || type>=topmost_intro_js){
                //not php state
                m_htmlStoredState.push(m_currentState.pop());
            }else{
                break;
            }
        }

        if(m_codeStoredState.size()>0){
            //qDebug()<<"restore php111"<<m_indentDepth<<m_currentLine;
            //restore php state
            while(m_codeStoredState.size()>0){
                m_currentState.push(m_codeStoredState.pop());
            }
            //m_indentDepth = m_currentState.top().savedIndentDepth;
            //qDebug()<<"restore php222"<<m_indentDepth<<m_currentLine;
        }else{
            m_indentDepth = savedIndentDepth;
            //qDebug()<<"enterPHP1111"<<m_indentDepth;
            enter(Php::CodeFormatter::topmost_intro_php);
            //qDebug()<<"enterPHP2222"<<m_indentDepth;
        }
    }

    void CodeFormatter::leavePHP(){
        //qDebug()<<"leavePHP0000000000000"<< m_currentState.top().type<<phpFormatter.stateToString(m_currentState.top().type)<<m_currentLine;
        int type = m_currentState.top().type;
        while(m_currentState.size()>0){
            auto type = m_currentState.top().type;
            if(type>=topmost_intro_php && type<topmost_intro_js){
                //php code state
                //qDebug()<<"leavePHP state"<<type;
                m_codeStoredState.push(m_currentState.pop());
            }else{
                //qDebug()<<"leavePHP last state"<<type;
                break;
            }
        }
        while(m_htmlStoredState.size()>0){
            m_currentState.push(m_htmlStoredState.pop());
        }
        if(m_currentState.size()>0){
            for(int i=0;i<m_currentState.size();i++){
                auto state = m_currentState.at(i);
                //qDebug()<<"leave php html state"<<stateToString(state.type)<<m_currentLine;
            }
            m_indentDepth = m_currentState.top().savedIndentDepth;
            //qDebug()<<"leavePHP"<<stateToString(m_currentState.top().type)<<m_currentState.top().savedIndentDepth<<m_currentLine;
        }else{
            //qDebug()<<"leavePHP empty html";
        }

        m_indentDepth += m_indentSize;

        //qDebug()<<"leavePHP"<< type<<phpFormatter.stateToString(type)<<"current:"<<stateToString(m_currentState.top().type)<<m_currentLine;

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
        leave();//leave "script" html
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
        leave();//leave "style" html
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
        blockData.m_storedState = m_htmlStoredState;
        blockData.m_otherState = m_codeStoredState;
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
                m_htmlStoredState = blockData.m_storedState;
                m_codeStoredState = blockData.m_otherState;
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
            initialState.push(State(top_html, 0));
        return initialState;
    }

    int CodeFormatter::tokenizeBlock(const QTextBlock &block)
    {
        const QTextBlock previous = block.previous();
        int startState = loadLexerState(previous);
        QByteArray startTQouteTag;
        if((startState&Php::Scanner::MultiLineStringTQuote) == Php::Scanner::MultiLineStringTQuote){
            startTQouteTag = loadExpectedString(previous);
            Php::Scanner::setCurrentTQouteTag(startTQouteTag);
        }
        if (block.blockNumber() == 0){
            startState = 0;
        }
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

        if((lexerState&Php::Scanner::MultiLineStringTQuote) == Php::Scanner::MultiLineStringTQuote){
            //load expected suffix
            if(startTQouteTag.isEmpty()){
                startTQouteTag = Php::Scanner::currentTQouteTag().toLatin1();
            }
            saveExpectedString(&saveableBlock,startTQouteTag);
        }
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
        if(type>=topmost_intro_css){
            return cssFormatter.stateToString(type);
        }else if(type>=topmost_intro_js){
            return jsFormatter.stateToString(type);
        }else if(type>=topmost_intro_php){
            return phpFormatter.stateToString(type);
        }else{
            const QMetaEnum &metaEnum = staticMetaObject.enumerator(staticMetaObject.indexOfEnumerator("StateType"));
            return QString::fromUtf8(metaEnum.valueToKey(type));
        }
    }


    void CodeFormatter::onEnter(int newState, int *indentDepth, int *savedIndentDepth) const
    {
        const State &parentState = state();
        const Token &tk = currentToken();
        const int tokenPosition = column(tk.begin());
        const bool firstToken = (tokenIndex() == 0);
        const bool lastToken = (tokenIndex() == tokenCount() - 1);

        switch (newState) {
        case html:
            if(firstToken){
                if(parentState.type!=top_html){

                }
                *savedIndentDepth = *indentDepth;
                *indentDepth += m_indentSize;
                //qDebug()<<"enter html 123"<<m_currentLine<<stateToString(parentState.type);
            }
            break;
        case close_tag:
            if(firstToken){
                *indentDepth = *savedIndentDepth - m_indentSize;
                *savedIndentDepth = *indentDepth;
            }
            break;
        case comment_content:
            *indentDepth = tokenPosition;
            *savedIndentDepth = *indentDepth;

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
                    if((topState.type >= top_js &&  topState.type < topmost_intro_css) || topState.type >= top_css){
                        *indentDepth -= m_indentSize;
                    }
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

        //userData->setExpectedRawStringSuffix("111");
        cppData->m_data = data;
    }

    bool CodeFormatter::loadBlockData(const QTextBlock &block, BlockData *data) const
    {
        TextBlockUserData *userData = TextDocumentLayout::textUserData(block);
        if (!userData)
            return false;
        //qDebug()<<"expectedRawStringSuffix"<<userData->expectedRawStringSuffix();
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


    //save php <<< string tag
    void CodeFormatter::saveExpectedString(QTextBlock *block,const QByteArray& tag) const {
        TextDocumentLayout::setExpectedRawStringSuffix(*block, tag);
    }

    //load php <<< string tag
    QByteArray CodeFormatter::loadExpectedString(const QTextBlock &block) const {
        return TextDocumentLayout::expectedRawStringSuffix(block);
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

    bool CodeFormatter::isIdentifier(QChar chr){
        return chr.isLetterOrNumber() || chr == '_' || chr == '-' || chr == '$';
    }

}
