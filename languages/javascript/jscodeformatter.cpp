#include "jscodeformatter.h"
#include "languages/html/htmlcodeformatter.h"
#include "tabsettings.h"
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QTextBlock>
#include <QTextDocument>

using namespace TextEditor;
using namespace Code;

namespace Javascript {


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
            //html leave js
            if(pHtmlFormatter!=nullptr){
                pHtmlFormatter->leaveJS();
            }
            break;
        }
        int type = pHtmlFormatter!=nullptr?pHtmlFormatter->m_currentState.top().type:m_currentState.top().type;
        switch (type) {
        case topmost_intro_js:
            switch (kind) {
            default:
                enter(top_js); break;
            } break;
        case top_js:
            tryStatement();
            break;
        case objectdefinition_or_js:
            switch (kind) {
            case Code::Token::Dot:           break;
            case Code::Token::Identifier:
                if (!m_currentLine.at(m_currentToken.begin()).isUpper()) {
                    turnInto(top_js);
                    continue;
                }
                break;
            case Code::Token::LeftBrace:     turnInto(binding_or_objectdefinition); continue;
            default:            turnInto(top_js); continue;
            } break;

        case import_start:
            enter(import_maybe_dot_or_version_or_as);
            break;

        case import_maybe_dot_or_version_or_as:
            switch (kind) {
            case Code::Token::Dot:           turnInto(import_dot); break;
            case As:            turnInto(import_as); break;
            case Code::Token::Number:        turnInto(import_maybe_as); break;
            default:            leave(); leave(); continue;
            } break;

        case import_maybe_as:
            switch (kind) {
            case As:            turnInto(import_as); break;
            default:            leave(); leave(); continue;
            } break;

        case import_dot:
            switch (kind) {
            case Code::Token::Identifier:    turnInto(import_maybe_dot_or_version_or_as); break;
            default:            leave(); leave(); continue;
            } break;

        case import_as:
            switch (kind) {
            case Code::Token::Identifier:    leave(); leave(); break;
            } break;

        case binding_or_objectdefinition:
            switch (kind) {
            case Code::Token::Colon:         enter(binding_assignment); break;
            case Code::Token::LeftBrace:     enter(objectdefinition_open); break;
            } break;

        case binding_assignment:
            switch (kind) {
            case Code::Token::Semicolon:     leave(true); break;
            case If:            enter(if_statement); break;
            case With:          enter(statement_with_condition); break;
            case Try:           enter(try_statement); break;
            case Switch:        enter(switch_statement); break;
            case Code::Token::LeftBrace:     enter(block_open); break;
            case On:
            case As:
            case Import:
            case Code::Token::Identifier:    enter(expression_or_objectdefinition); break;

                // error recovery
            case Code::Token::RightBracket:
            case Code::Token::RightParenthesis:  leave(true); break;

            default:            enter(expression); continue;
            } break;

        case objectdefinition_open:
            switch (kind) {
            case Code::Token::RightBrace:    leave(true); break;
            case Default:
            case Function:      enter(function_start); break;
            case Enum:          enter(enum_start); break;
            case On:
            case As:
            case List:
            case Import:
            case Code::Token::Identifier:    enter(binding_or_objectdefinition); break;
            } break;

        case property_modifiers:
            switch (kind) {
            case Default:break;
            default:            leave(true); break;
            } break;

        case property_start:
            switch (kind) {
            case Code::Token::Colon:         enter(binding_assignment); break; // oops, was a binding
            case Var:
            case Code::Token::Identifier:    enter(property_name); break;
            default:            leave(true); continue;
            } break;

        case required_property:
            switch (kind) {
            case Default:
                turnInto(property_modifiers); break;
            case Code::Token::Identifier:    leave(true); break;
            default:            leave(true); continue;
            } break;

        case component_start:
            switch (kind) {
            case Code::Token::Identifier:    turnInto(StateType::component_name); break;
            default:            leave(true); continue;
            } break;

        case property_name:
            turnInto(property_maybe_initializer);
            break;

        case property_list_open: {
            const QStringView tok = QStringView(m_currentLine).mid(
                m_currentToken.begin(),
                m_currentToken.length);
            if (tok == QLatin1String(">"))
                turnInto(property_name);
            break;
        }
        case property_maybe_initializer:
            switch (kind) {
            case Code::Token::Colon:         turnInto(binding_assignment); break;
            default:            leave(true); continue;
            } break;

        case enum_start:
            switch (kind) {
            case Code::Token::LeftBrace: enter(objectliteral_open); break;
            } break;
        case function_start:
            switch (kind) {
            case Code::Token::LeftParenthesis:   enter(function_arglist_open); break;
            } break;

        case function_arglist_open:
            switch (kind) {
            case Code::Token::RightParenthesis:  turnInto(function_arglist_closed); break;
            } break;

        case function_arglist_closed:
            switch (kind) {
            case Code::Token::LeftBrace:         turnInto(block_open); break;
            case Code::Token::Colon:             turnInto(function_type_annotated_return); break;
            default:                leave(true); continue; // error recovery
            } break;

        case function_type_annotated_return:
            switch (kind) {
            case Code::Token::LeftBrace:         turnInto(block_open); break;
            default:                break;
            } break;

        case expression_or_objectdefinition:
            switch (kind) {
            case Code::Token::Dot:
            case Code::Token::Identifier:        break; // need to become an objectdefinition_open in cases like "width: Qt.Foo {"
            case Code::Token::LeftBrace:         turnInto(objectdefinition_open); break;

                // propagate 'leave' from expression state
            case Code::Token::RightBracket:
            case Code::Token::RightParenthesis:  leave(); continue;

            default:                enter(expression); continue; // really? identifier and more tokens might already be gone
            } break;

        case expression_or_label:
            switch (kind) {
            case Code::Token::Colon:
                turnInto(labelled_statement); break;

                // propagate 'leave' from expression state
            case Code::Token::RightBracket:
            case Code::Token::RightParenthesis:
                leave(); continue;

            default:
                enter(expression); continue;
            } break;

        case ternary_op:
            if (kind == Code::Token::Colon) {
                enter(ternary_op_after_colon);
                enter(expression_continuation);
                break;
            }
            Q_FALLTHROUGH();
        case ternary_op_after_colon:
        case expression:
            if (tryInsideExpression())
                break;
            switch (kind) {
            case Code::Token::Comma:             leave(true); break;
            case Code::Token::Delimiter:         enter(expression_continuation); break;
            case Code::Token::RightBracket:
            case Code::Token::RightParenthesis:  leave(); continue;
            case Code::Token::RightBrace:        leave(true); continue;
            case Code::Token::Semicolon:         leave(true); break;
            } break;

        case expression_continuation:
            leave();
            continue;

        case expression_maybe_continuation:
            switch (kind) {
            case Question:
            case Code::Token::Delimiter:
            case Code::Token::LeftBracket:
            case Code::Token::LeftParenthesis:
            case Code::Token::LeftBrace:         leave(); continue;
            default:                leave(true); continue;
            } break;

        case paren_open:
            if (tryInsideExpression())
                break;
            switch (kind) {
            case Code::Token::RightParenthesis:  leave(); break;
            } break;

        case bracket_open:
            if (tryInsideExpression())
                break;
            switch (kind) {
            case Code::Token::Comma:             enter(bracket_element_start); break;
            case Code::Token::RightBracket:      leave(); break;
            } break;

        case objectliteral_open:
            if (tryInsideExpression())
                break;
            switch (kind) {
            case Code::Token::Colon:             enter(objectliteral_assignment); break;
            case Code::Token::RightBracket:
            case Code::Token::RightParenthesis:  leave(); continue; // error recovery
            case Code::Token::RightBrace:        leave(true); break;
            } break;

            // pretty much like expression, but ends with , or }
        case objectliteral_assignment:
            if (tryInsideExpression())
                break;
            switch (kind) {
            case Code::Token::Comma:             leave(); break;
            case Code::Token::Delimiter:         enter(expression_continuation); break;
            case Code::Token::RightBracket:
            case Code::Token::RightParenthesis:  leave(); continue; // error recovery
            case Code::Token::RightBrace:        leave(); continue; // so we also leave objectliteral_open
            } break;

        case bracket_element_start:
            switch (kind) {
            case Code::Token::Identifier:        turnInto(bracket_element_maybe_objectdefinition); break;
            default:                leave(); continue;
            } break;

        case bracket_element_maybe_objectdefinition:
            switch (kind) {
            case Code::Token::LeftBrace:         turnInto(objectdefinition_open); break;
            default:                leave(); continue;
            } break;

        case block_open:
        case substatement_open:
            if (tryStatement())
                break;
            switch (kind) {
            case Code::Token::RightBrace:        leave(true); break;
            } break;

        case labelled_statement:
            if (tryStatement())
                break;
            leave(true); // error recovery
            break;

        case substatement:
            // prefer substatement_open over block_open
            if (kind != Code::Token::LeftBrace) {
                if (tryStatement())
                    break;
            }
            switch (kind) {
            case Code::Token::LeftBrace:         turnInto(substatement_open); break;
            } break;

        case if_statement:
            switch (kind) {
            case Code::Token::LeftParenthesis:   enter(condition_open); break;
            default:                leave(true); break; // error recovery
            } break;

        case maybe_else:
            switch (kind) {
            case Else:              turnInto(else_clause); enter(substatement); break;
            default:                leave(true); continue;
            } break;

        case maybe_catch_or_finally:
            switch (kind) {
            case Catch:             turnInto(catch_statement); break;
            case Finally:           turnInto(finally_statement); break;
            default:                leave(true); continue;
            } break;

        case else_clause:
            // ### shouldn't happen
            dump();
            Q_ASSERT(false);
            leave(true);
            break;

        case condition_open:
            if (tryInsideExpression())
                break;
            switch (kind) {
            case Code::Token::RightParenthesis:  turnInto(substatement); break;
            } break;

        case switch_statement:
        case catch_statement:
        case statement_with_condition:
            switch (kind) {
            case Code::Token::LeftParenthesis:   enter(statement_with_condition_paren_open); break;
            default:                leave(true);
            } break;

        case statement_with_condition_paren_open:
            if (tryInsideExpression())
                break;
            switch (kind) {
            case Code::Token::RightParenthesis:  turnInto(substatement); break;
            } break;

        case try_statement:
        case finally_statement:
            switch (kind) {
            case Code::Token::LeftBrace:         enter(block_open); break;
            default:                leave(true); break;
            } break;

        case do_statement:
            switch (kind) {
            case While:             break;
            case Code::Token::LeftParenthesis:   enter(do_statement_while_paren_open); break;
            default:                leave(true); continue; // error recovery
            } break;

        case do_statement_while_paren_open:
            if (tryInsideExpression())
                break;
            switch (kind) {
            case Code::Token::RightParenthesis:  leave(); leave(true); break;
            } break;

        case breakcontinue_statement:
            switch (kind) {
            case Code::Token::Identifier:        leave(true); break;
            default:                leave(true); continue; // try again
            } break;

        case case_start:
            switch (kind) {
            case Code::Token::Colon:             turnInto(case_cont); break;
            } break;

        case case_cont:
            if (kind != Case && kind != Default && tryStatement())
                break;
            switch (kind) {
            case Code::Token::RightBrace:        leave(); continue;
            case Default:
            case Case:              leave(); continue;
            } break;

        case multiline_comment_start:
        case multiline_comment_cont:
            if (kind != Code::Token::Comment) {
                leave();
                continue;
            } else if (*tokenIndex == m_tokens.size() - 1
                       && (lexerState & Scanner::MultiLineMask) == Scanner::Normal) {
                leave();
            } else if (*tokenIndex == 0) {
                // to allow enter/leave to update the indentDepth
                turnInto(multiline_comment_cont);
            }
            break;

        default:
            if(pHtmlFormatter!=nullptr){
                qWarning() << "js Unhandled state" << pHtmlFormatter->m_currentState.top().type;
            }else{
                qWarning() << "js Unhandled state" << m_currentState.top().type;
            }
            break;
        } // end of state switch

        ++*tokenIndex;
    }

    int topState = pHtmlFormatter!=nullptr?pHtmlFormatter->m_currentState.top().type:m_currentState.top().type;

    // if there's no colon on the same line, it's not a label
    if (topState == expression_or_label)
        enter(expression);
    // if not followed by an identifier on the same line, it's done
    else if (topState == breakcontinue_statement)
        leave(true);

    topState = pHtmlFormatter!=nullptr?pHtmlFormatter->m_currentState.top().type:m_currentState.top().type;

    // some states might be continued on the next line
    if (topState == expression
        || topState == expression_or_objectdefinition
        || topState == objectliteral_assignment
        || topState == ternary_op_after_colon) {
        enter(expression_maybe_continuation);
    }
    // multi-line comment start?
    if (topState != multiline_comment_start && topState != multiline_comment_cont && (lexerState & Scanner::MultiLineMask) == Scanner::MultiLineComment) {
        enter(multiline_comment_start);
    }

    //*tokenIndex = m_tokenIndex;
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
        int savedIndentDepth = pHtmlFormatter->m_indentDepth;
        onEnter(newState, &(pHtmlFormatter->m_indentDepth), &savedIndentDepth);
        Code::State s(newState, savedIndentDepth);

        pHtmlFormatter->m_currentState.push(s);
        pHtmlFormatter->m_newStates.push(s);
        //Token tok = pHtmlFormatter->currentToken();
        //qDebug() << "js enter state 1" << stateToString(newState)<<pHtmlFormatter->m_currentLine.mid(pHtmlFormatter->m_currentToken.begin(),pHtmlFormatter->m_currentToken.length)<<"indent:"<<pHtmlFormatter->m_indentDepth<<"size:"<<pHtmlFormatter->m_currentState.size();
    }else{
        if(m_tokens.size()>m_tokenIndex){
            Token tok = m_tokens.at(m_tokenIndex);
            //qDebug()<<"enter:"<<this->stateToString(newState)<<"current"<<this->stateToString(m_currentState.top().type)<<m_currentLine.mid(tok.begin(),tok.length);
        }

        int savedIndentDepth = m_indentDepth;
        onEnter(newState, &m_indentDepth, &savedIndentDepth);
        Code::State s(newState, savedIndentDepth);
        m_currentState.push(s);
        m_newStates.push(s);
        //qDebug()<<"indent:"<<m_indentDepth<<savedIndentDepth;

    }
    //dump();

    if (newState == bracket_open)
        enter(bracket_element_start);

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
        //qDebug() << "js left state1" << stateToString(poppedState.type) << ", now in state" << stateToString(topState)<<m_indentDepth <<"size:"<<pHtmlFormatter->m_currentState.size()<<"indent:"<<pHtmlFormatter->m_indentDepth;


    }else{

        if (m_currentState.size()<=1)
            return;
        if (m_newStates.size() > 0)
            m_newStates.pop();

        poppedState = m_currentState.pop();
        topState = m_currentState.top().type;
        m_indentDepth = poppedState.savedIndentDepth;
        //qDebug() << "js left state2" << stateToString(poppedState.type) << ", now in state" << stateToString(topState)<<m_indentDepth <<"size:"<<m_currentState.size()<<"indent:"<<m_indentDepth;
    }
    // restore indent depth





    // if statement is done, may need to leave recursively
    if (statementDone) {
        if (topState == if_statement) {
            if (poppedState.type != maybe_else)
                enter(maybe_else);
            else
                leave(true);
        } else if (topState == else_clause) {
            // leave the else *and* the surrounding if, to prevent another else
            leave();
            leave(true);
        } else if (topState == try_statement) {
            if (poppedState.type != maybe_catch_or_finally
                && poppedState.type != finally_statement) {
                enter(maybe_catch_or_finally);
            } else {
                leave(true);
            }
        } else if (!isExpressionEndState(topState)) {
            leave(true);
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

bool CodeFormatter::tryInsideExpression(bool alsoExpression)
{
    int newState = -1;
    const int kind = extendedTokenKind(m_currentToken);
    switch (kind) {
    case Code::Token::LeftParenthesis:   newState = paren_open; break;
    case Code::Token::LeftBracket:       newState = bracket_open; break;
    case Code::Token::LeftBrace:         newState = objectliteral_open; break;
    case Function:          newState = function_start; break;
    case Question:          newState = ternary_op; break;
    }

    if (newState != -1) {
        if (alsoExpression)
            enter(expression);
        enter(newState);
        return true;
    }

    return false;
}

bool CodeFormatter::tryStatement()
{
    const int kind = extendedTokenKind(m_currentToken);

    //qDebug()<<"tryStatement:"<<kind<<pHtmlFormatter->m_currentToken.kind<<pHtmlFormatter->m_currentLine.mid(pHtmlFormatter->m_currentToken.begin(),pHtmlFormatter->m_currentToken.length);
    switch (kind) {
    case Code::Token::Semicolon:
        enter(empty_statement);
        leave(true);
        return true;
    case Break:
    case Continue:
        enter(breakcontinue_statement);
        return true;
    case Throw:
        enter(throw_statement);
        enter(expression);
        return true;
    case Return:
        enter(return_statement);
        enter(expression);
        return true;
    case While:
    case For:
    case Catch:
        enter(statement_with_condition);
        return true;
    case Switch:
        enter(switch_statement);
        return true;
    case If:
        enter(if_statement);
        return true;
    case Do:
        enter(do_statement);
        enter(substatement);
        return true;
    case Case:
    case Default:
        enter(case_start);
        return true;
    case Try:
        enter(try_statement);
        return true;
    case Code::Token::LeftBrace:
        enter(block_open);
        return true;
    case Code::Token::Identifier:
        enter(expression_or_label);
        return true;
    case Code::Token::Delimiter:
    case Var:
    case PlusPlus:
    case MinusMinus:
    case On:
    case As:
    case Function:
    case Code::Token::Number:
    case Code::Token::String:
    case Code::Token::LeftParenthesis:
        enter(expression);
        // look at the token again
        m_tokenIndex -= 1;
        return true;
    }
    return false;
}

bool CodeFormatter::isBracelessState(int type) const
{
    return
        type == if_statement ||
        type == else_clause ||
        type == substatement ||
        type == binding_assignment ||
        type == binding_or_objectdefinition;
}

bool CodeFormatter::isExpressionEndState(int type) const
{
    return
        type == topmost_intro_js ||
        type == top_js ||
        type == objectdefinition_open ||
        type == do_statement ||
        type == block_open ||
        type == substatement_open ||
        type == bracket_open ||
        type == paren_open ||
        type == case_cont ||
        type == objectliteral_open;
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
    blockData.m_indentDepth = m_indentDepth;

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
            if(pHtmlFormatter!=nullptr){
                pHtmlFormatter->m_currentState = blockData.m_endState;
                pHtmlFormatter->m_beginState = pHtmlFormatter->m_currentState;

            }else{
                m_currentState = blockData.m_endState;
                m_beginState = m_currentState;
            }

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
        initialState.push(State(topmost_intro_js, 0));
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
        if (text == QLatin1String("as"))
            return As;
    } else if (kind == Code::Token::Keyword) {
        const char char1 = text.at(0).toLatin1();
        const char char2 = text.at(1).toLatin1();
        const char char3 = (text.size() > 2 ? text.at(2).toLatin1() : 0);
        const char char4 = (text.size() > 3 ? text.at(3).toLatin1() : 0);
        switch (char1) {
        case 'v':
            return Var;
        case 'i':
            if (char2 == 'f')
                return If;
            else if (char3 == 's')
                return Instanceof;
            else
                return In;
        case 'f':
            if (char2 == 'o')
                return For;
            else if (char2 == 'u')
                return Function;
            else
                return Finally;
        case 'e':
            return Else;
        case 'n':
            return New;
        case 'r':
            return Return;
        case 's':
            return Switch;
        case 'w':
            return While;
        case 'c':
            if (char3 == 's')
                return Case;
            if (char3 == 't')
                return Catch;
            return Continue;
        case 'd':
            if (char3 == 'f')
                return Default;
            return Do;
        case 't':
            if (char3 == 'y')
                return Try;
            if (char3 == 'r')
                return Throw;
            return This;
        case 'b':
            return Break;
        }
    } else if (kind == Code::Token::Delimiter) {
        if (text == QLatin1String("?"))
            return Question;
        else if (text == QLatin1String("++"))
            return PlusPlus;
        else if (text == QLatin1String("--"))
            return MinusMinus;
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
    case objectdefinition_open: {
        // special case for things like "gradient: Gradient {"
        if (parentState.type == binding_assignment)
            *savedIndentDepth = state(1).savedIndentDepth;

        if (firstToken)
            *savedIndentDepth = tokenPosition;

        *indentDepth = *savedIndentDepth + m_indentSize;
        break;
    }

    case binding_or_objectdefinition:
        if (firstToken)
            *indentDepth = *savedIndentDepth = tokenPosition;
        break;

    case binding_assignment:
    //case objectliteral_assignment:
        if (lastToken)
            *indentDepth = *savedIndentDepth + 4;
        else
            *indentDepth = column(tokenAt(tokenIndex() + 1).begin());
        break;

    case expression_or_objectdefinition:
        *indentDepth = tokenPosition;
        break;

    //case expression_or_label:
        //if (*indentDepth == tokenPosition)
        //    *indentDepth += m_indentSize;
        //else
        //    *indentDepth = tokenPosition;
    //    break;

    case expression:
        if(parentState.type == top_js){

        }else if (*indentDepth == tokenPosition) {
            // expression_or_objectdefinition doesn't want the indent
            // expression_or_label already has it
            if (parentState.type != expression_or_objectdefinition && parentState.type != expression_or_label && parentState.type != binding_assignment) {
                //*indentDepth += 2*m_indentSize;
            }
        }
        // expression_or_objectdefinition and expression_or_label have already consumed the first token
        else if (parentState.type != expression_or_objectdefinition && parentState.type != expression_or_label) {
            //*indentDepth = tokenPosition;
        }
        break;

    case expression_maybe_continuation:
        // set indent depth to indent we'd get if the expression ended here
        for (int i = 1; state(i).type != topmost_intro_js; ++i) {
            const int type = state(i).type;
            if (isExpressionEndState(type) && !isBracelessState(type)) {
                *indentDepth = state(i - 1).savedIndentDepth;
                break;
            }
        }
        break;

    case bracket_open:
        if (parentState.type == expression && state(1).type == binding_assignment) {
            *savedIndentDepth = state(2).savedIndentDepth;
            *indentDepth = *savedIndentDepth + m_indentSize;
        } else if (parentState.type == objectliteral_assignment) {
            *savedIndentDepth = parentState.savedIndentDepth;
            *indentDepth = *savedIndentDepth + m_indentSize;
        } else if (!lastToken) {
            //*indentDepth = tokenPosition + 1;
        } else {
            *indentDepth = *savedIndentDepth + m_indentSize;
        }
        break;

    case function_start:
        // align to the beginning of the line
        *savedIndentDepth = *indentDepth = column(tokenAt(0).begin());
        break;

    case do_statement_while_paren_open:
    case statement_with_condition_paren_open:
    case function_arglist_open:
    //case paren_open:
        if (!lastToken){
            //*indentDepth = tokenPosition + 1;
        }else
            *indentDepth += m_indentSize;
        break;

    case ternary_op:
        if (!lastToken){
            //*indentDepth = tokenPosition + tk.length + 1;
        }else
            *indentDepth += m_indentSize;
        break;

    case block_open:
        // closing brace should be aligned to case
        if (parentState.type == case_cont) {
            *savedIndentDepth = parentState.savedIndentDepth;
            break;
        }
        Q_FALLTHROUGH();
    case substatement_open:
        // special case for "foo: {" and "property int foo: {"
        if (parentState.type == binding_assignment)
            *savedIndentDepth = state(1).savedIndentDepth;
        *indentDepth = *savedIndentDepth + m_indentSize;
        break;

    case substatement:
        *indentDepth += m_indentSize;
        break;

    case objectliteral_open:
        if (parentState.type == expression
            || parentState.type == objectliteral_assignment) {
            // undo the continuation indent of the expression
            if (state(1).type == expression_or_label)
                *indentDepth = state(1).savedIndentDepth;
            else
                *indentDepth = parentState.savedIndentDepth;
            *savedIndentDepth = *indentDepth;
        }
        *indentDepth += m_indentSize;
        break;


    case statement_with_condition:
    case try_statement:
    case catch_statement:
    case finally_statement:
    case if_statement:
    case do_statement:
    case switch_statement:
        if (firstToken || parentState.type == binding_assignment)
            *savedIndentDepth = tokenPosition;
        // ### continuation
        *indentDepth = *savedIndentDepth; // + 2*m_indentSize;
        // special case for 'else if'
        if (!firstToken
            && newState == if_statement
            && parentState.type == substatement
            && state(1).type == else_clause) {
            *indentDepth = state(1).savedIndentDepth;
            *savedIndentDepth = *indentDepth;
        }
        break;

    case maybe_else:
    case maybe_catch_or_finally: {
        // set indent to where leave(true) would put it
        int lastNonEndState = 0;
        while (!isExpressionEndState(state(lastNonEndState + 1).type))
            ++lastNonEndState;
        *indentDepth = state(lastNonEndState).savedIndentDepth;
        break;
    }

    case condition_open:
        // fixed extra indent when continuing 'if (', but not for 'else if ('
        if (tokenPosition <= *indentDepth + m_indentSize)
            *indentDepth += 2*m_indentSize;
        else
            *indentDepth = tokenPosition + 1;
        break;

    case case_start:
        *savedIndentDepth = tokenPosition;
        break;

    case case_cont:
        *indentDepth += m_indentSize;
        break;

    case multiline_comment_start:
        *indentDepth = tokenPosition + 2;
        break;

    case multiline_comment_cont:
        *indentDepth = tokenPosition;
        break;
    }

    //qDebug()<<"onEnter2:"<<*savedIndentDepth<<*indentDepth;
}

void CodeFormatter::adjustIndent(const QList<Code::Token> &tokens, int startLexerState, int *indentDepth) const
{
    State topState = state();
    State previousState = state(1);

    // keep user-adjusted indent in multiline comments
    //qDebug()<<"adjustIndent:"<<this->stateToString(topState.type);
    //qDebug()<<"adjustIndent:"<<this->stateToString(previousState.type);
    if (topState.type == multiline_comment_start || topState.type == multiline_comment_cont) {
        if (!tokens.isEmpty()) {
            *indentDepth = column(tokens.at(0).begin());
            return;
        }
    }
    // don't touch multi-line strings at all
    if ((startLexerState & Scanner::MultiLineMask) == Scanner::MultiLineStringDQuote || (startLexerState & Scanner::MultiLineMask) == Scanner::MultiLineStringSQuote) {
        *indentDepth = -1;
        return;
    }
    //qDebug()<<"adjustIndent11:"<<*indentDepth;
    const int kind = extendedTokenKind(tokenAt(0));
    switch (kind) {
    case Code::Token::LeftBrace:
        if (topState.type == substatement || topState.type == binding_assignment || topState.type == case_cont) {
            *indentDepth = topState.savedIndentDepth;
        }
        break;
    case Code::Token::RightBrace: {
        if (topState.type == block_open && previousState.type == case_cont) {
            *indentDepth = previousState.savedIndentDepth;
            break;
        }
        for (int i = 0; state(i).type != topmost_intro_js; ++i) {
            const int type = state(i).type;
            if (type == objectdefinition_open || type == block_open || type == substatement_open || type == objectliteral_open) {
                *indentDepth = state(i).savedIndentDepth;
                break;
            }
        }
        break;
    }
    case Code::Token::RightBracket:
        for (int i = 0; state(i).type != topmost_intro_js; ++i) {
            const int type = state(i).type;
            if (type == bracket_open) {
                *indentDepth = state(i).savedIndentDepth;
                break;
            }
        }
        break;

    case Code::Token::LeftBracket:
    case Code::Token::LeftParenthesis:
    case Code::Token::Delimiter:
        if (topState.type == expression_maybe_continuation)
            *indentDepth = topState.savedIndentDepth;
        break;

    case Else:
        if (topState.type == maybe_else) {
            *indentDepth = state(1).savedIndentDepth;
        } else if (topState.type == expression_maybe_continuation) {
            bool hasElse = false;
            for (int i = 1; state(i).type != topmost_intro_js; ++i) {
                const int type = state(i).type;
                if (type == else_clause)
                    hasElse = true;
                if (type == if_statement) {
                    if (hasElse) {
                        hasElse = false;
                    } else {
                        *indentDepth = state(i).savedIndentDepth;
                        break;
                    }
                }
            }
        }
        break;

    case Catch:
    case Finally:
        if (topState.type == maybe_catch_or_finally)
            *indentDepth = state(1).savedIndentDepth;
        break;

    case Code::Token::Colon:
        if (topState.type == ternary_op)
            *indentDepth -= 2;
        break;

    case Question:
        if (topState.type == expression_maybe_continuation)
            *indentDepth = topState.savedIndentDepth;
        break;

    case Default:
    case Case:
        for (int i = 0; state(i).type != topmost_intro_js; ++i) {
            const int type = state(i).type;
            if (type == switch_statement || type == case_cont) {
                *indentDepth = state(i).savedIndentDepth;
                break;
            } else if (type == topmost_intro_js) {
                break;
            }
        }
        break;
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

bool CodeFormatter::isIdentifier(QChar chr){
    return chr.isLetterOrNumber() || chr == '_' || chr == '$';
}


} // namespace js
