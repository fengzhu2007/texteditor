// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "cppcodeformatter.h"

#include "textdocumentlayout.h"

#include "utils/qtcassert.h"

#include <QDebug>
#include <QMetaEnum>
#include <QTextDocument>
#include <QTextBlock>

using namespace CPlusPlus;
using namespace TextEditor;

namespace CppEditor {

CodeFormatter::~CodeFormatter() = default;

void CodeFormatter::setTabSize(int tabSize)
{
    m_tabSize = tabSize;
}

void CodeFormatter::recalculateStateAfter(const QTextBlock &block)
{

}

void CodeFormatter::indentFor(const QTextBlock &block, int *indent, int *padding)
{
//    qDebug() << "indenting for" << block.blockNumber() + 1;

    restoreCurrentState(block.previous());
    correctIndentation(block);
    *indent = m_indentDepth;
    *padding = m_paddingDepth;
}

void CodeFormatter::indentForNewLineAfter(const QTextBlock &block, int *indent, int *padding)
{
    restoreCurrentState(block);
    *indent = m_indentDepth;
    *padding = m_paddingDepth;

    int lexerState = loadLexerState(block);
   // m_tokens.clear();
    m_currentLine.clear();
    //adjustIndent(m_tokens, lexerState, indent, padding);
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
        if (previousState.isEmpty() || blockData.m_beginState.isEmpty()
                || previousState != blockData.m_beginState)
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

bool CodeFormatter::isInRawStringLiteral(const QTextBlock &block) const
{
    if (!block.previous().isValid())
        return false;
    BlockData blockData;
    if (!loadBlockData(block.previous(), &blockData))
        return false;
    return !blockData.m_endState.isEmpty() && blockData.m_endState.top().type == raw_string_open;
}

CodeFormatter::State CodeFormatter::state(int belowTop) const
{
    if (belowTop < m_currentState.size())
        return m_currentState.at(m_currentState.size() - 1 - belowTop);
    else
        return {};
}

int CodeFormatter::tokenIndex() const
{
    return m_tokenIndex;
}

int CodeFormatter::tokenCount() const
{
    return 0;
}

/*const Token &CodeFormatter::currentToken() const
{
    return m_currentToken;
}*/

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
    int savedPaddingDepth = m_paddingDepth;
    onEnter(newState, &m_indentDepth, &savedIndentDepth, &m_paddingDepth, &savedPaddingDepth);
    State s(newState, savedIndentDepth, savedPaddingDepth);
    m_currentState.push(s);
    m_newStates.push(s);
}

void CodeFormatter::leave(bool statementDone)
{
    QTC_ASSERT(m_currentState.size() > 1, return);
    if (m_currentState.top().type == topmost_intro)
        return;

    if (m_newStates.size() > 0)
        m_newStates.pop();

    // restore indent depth
    State poppedState = m_currentState.pop();
    m_indentDepth = poppedState.savedIndentDepth;
    m_paddingDepth = poppedState.savedPaddingDepth;

    int topState = m_currentState.top().type;

    // does it suffice to check if token is T_SEMICOLON or T_RBRACE?
    // maybe distinction between leave and turnInto?
    if (statementDone) {
        if (topState == substatement
                || topState == statement_with_condition
                || topState == for_statement
                || topState == switch_statement
                || topState == do_statement) {
            leave(true);
        } else if (topState == if_statement) {
            if (poppedState.type != maybe_else)
                enter(maybe_else);
            else
                leave(true);
        } else if (topState == else_clause) {
            // leave the else *and* the surrounding if, to prevent another else
            leave();
            leave(true);
        }
    }
}

void CodeFormatter::correctIndentation(const QTextBlock &block)
{
    const int lexerState = tokenizeBlock(block);
    QTC_ASSERT(m_currentState.size() >= 1, return);

    //adjustIndent(m_tokens, lexerState, &m_indentDepth, &m_paddingDepth);
}

bool CodeFormatter::tryExpression(bool alsoExpression)
{
    int newState = -1;



    return false;
}

bool CodeFormatter::tryDeclaration()
{
    return false;
}

bool CodeFormatter::tryStatement()
{
    return false;
}

bool CodeFormatter::isBracelessState(int type) const
{
    return type == substatement
        || type == if_statement
        || type == else_clause
        || type == statement_with_condition
        || type == for_statement
        || type == do_statement;
}

/*const Token &CodeFormatter::tokenAt(int idx) const
{
    static const Token empty;
    if (idx < 0 || idx >= m_tokens.size())
        return empty;
    else
        return m_tokens.at(idx);
}*/

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
    /*if (m_currentToken.utf16charsEnd() > m_currentLine.size())
        return QStringView(m_currentLine).mid(m_currentToken.utf16charsBegin());
    return QStringView(m_currentLine).mid(m_currentToken.utf16charsBegin(), m_currentToken.utf16chars());*/
    return {};
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
    blockData.m_paddingDepth = m_paddingDepth;

    QTextBlock saveableBlock(block);
    saveBlockData(&saveableBlock, blockData);
}

void CodeFormatter::restoreCurrentState(const QTextBlock &block)
{
    if (block.isValid()) {
        BlockData blockData;
        if (loadBlockData(block, &blockData)) {
            m_indentDepth = blockData.m_indentDepth;
            m_paddingDepth = blockData.m_paddingDepth;
            m_currentState = blockData.m_endState;
            m_beginState = m_currentState;
            return;
        }
    }

    m_currentState = initialState();
    m_beginState = m_currentState;
    m_indentDepth = 0;
    m_paddingDepth = 0;
}

QStack<CodeFormatter::State> CodeFormatter::initialState()
{
    static QStack<CodeFormatter::State> initialState;
    if (initialState.isEmpty())
        initialState.push(State(topmost_intro, 0, 0));
    return initialState;
}

int CodeFormatter::tokenizeBlock(const QTextBlock &block, bool *endedJoined)
{
    return 0;
}

void CodeFormatter::dump() const
{
    QMetaEnum metaEnum = staticMetaObject.enumerator(staticMetaObject.indexOfEnumerator("StateType"));

    qDebug() << "Current token index" << m_tokenIndex;
    qDebug() << "Current state:";
    for (const State &s : std::as_const(m_currentState))
        qDebug() << metaEnum.valueToKey(s.type) << s.savedIndentDepth << s.savedPaddingDepth;

    qDebug() << "Current indent depth:" << m_indentDepth;
    qDebug() << "Current padding depth:" << m_paddingDepth;
}


namespace Internal {
class CppCodeFormatterData: public CodeFormatterData
{
public:
    CodeFormatter::BlockData m_data;
};
} // namespace Internal

using namespace Internal;

QtStyleCodeFormatter::QtStyleCodeFormatter() = default;

QtStyleCodeFormatter::QtStyleCodeFormatter(const TabSettings &tabSettings,
                                           const CppCodeStyleSettings &settings)
    : m_tabSettings(tabSettings)
    , m_styleSettings(settings)
{
    setTabSize(tabSettings.m_tabSize);
}

void QtStyleCodeFormatter::setTabSettings(const TabSettings &tabSettings)
{
    m_tabSettings = tabSettings;
    setTabSize(tabSettings.m_tabSize);
}

void QtStyleCodeFormatter::setCodeStyleSettings(const CppCodeStyleSettings &settings)
{
    m_styleSettings = settings;
}

void QtStyleCodeFormatter::saveBlockData(QTextBlock *block, const BlockData &data) const
{
    TextBlockUserData *userData = TextDocumentLayout::userData(*block);
    auto cppData = static_cast<CppCodeFormatterData *>(userData->codeFormatterData());
    if (!cppData) {
        cppData = new CppCodeFormatterData;
        userData->setCodeFormatterData(cppData);
    }
    cppData->m_data = data;
}

bool QtStyleCodeFormatter::loadBlockData(const QTextBlock &block, BlockData *data) const
{
    TextBlockUserData *userData = TextDocumentLayout::textUserData(block);
    if (!userData)
        return false;
    auto cppData = static_cast<const CppCodeFormatterData *>(userData->codeFormatterData());
    if (!cppData)
        return false;

    *data = cppData->m_data;
    return true;
}

void QtStyleCodeFormatter::saveLexerState(QTextBlock *block, int state) const
{
    TextDocumentLayout::setLexerState(*block, state);
}

int QtStyleCodeFormatter::loadLexerState(const QTextBlock &block) const
{
    return TextDocumentLayout::lexerState(block);
}

void QtStyleCodeFormatter::addContinuationIndent(int *paddingDepth) const
{
    if (*paddingDepth == 0)
        *paddingDepth = 2*m_tabSettings.m_indentSize;
    else
        *paddingDepth += m_tabSettings.m_indentSize;
}

void QtStyleCodeFormatter::onEnter(int newState, int *indentDepth, int *savedIndentDepth, int *paddingDepth, int *savedPaddingDepth) const
{
    const State &parentState = state();
    //const Token &tk = currentToken();
    const bool firstToken = (tokenIndex() == 0);
    const bool lastToken = (tokenIndex() == tokenCount() - 1);
    //const int tokenPosition = column(tk.utf16charsBegin());
    //const int nextTokenPosition = lastToken ? tokenPosition + tk.utf16chars(): column(tokenAt(tokenIndex() + 1).utf16charsBegin());
    //const int spaceOrNextTokenPosition = lastToken ? tokenPosition + tk.utf16chars() + 1: nextTokenPosition;
    const int tokenPosition = 0;
    const int nextTokenPosition = 0;
    const int spaceOrNextTokenPosition = 0;
    if (shouldClearPaddingOnEnter(newState))
        *paddingDepth = 0;

    switch (newState) {
    case extern_start:
    case namespace_start:
        if (firstToken) {
            *savedIndentDepth = tokenPosition;
            *indentDepth = tokenPosition;
        }
        break;

    case enum_start:
    case class_start:
        if (firstToken) {
            *savedIndentDepth = tokenPosition;
            *indentDepth = tokenPosition;
        }
        *paddingDepth = 2*m_tabSettings.m_indentSize;
        break;

    case template_param:
        if (!lastToken)
            *paddingDepth = nextTokenPosition-*indentDepth;
        else
            addContinuationIndent(paddingDepth);
        break;

    case statement_with_condition:
    case for_statement:
    case switch_statement:
    case if_statement:
    case return_statement:
        if (firstToken)
            *indentDepth = *savedIndentDepth = tokenPosition;
        *paddingDepth = 2*m_tabSettings.m_indentSize;
        break;

    case declaration_start:
        if (firstToken) {
            *savedIndentDepth = tokenPosition;
            *indentDepth = *savedIndentDepth;
        }
        // continuation indent in function bodies only, to not indent
        // after the return type in "void\nfoo() {}"
        for (int i = 0; state(i).type != topmost_intro; ++i) {
            if (state(i).type == defun_open) {
                *paddingDepth = 2*m_tabSettings.m_indentSize;
                break;
            }
        }
        break;

    case assign_open:
        if (parentState.type == assign_open_or_initializer)
            break;
        Q_FALLTHROUGH();
    case assign_open_or_initializer:
        if (!lastToken && m_styleSettings.alignAssignments)
            *paddingDepth = nextTokenPosition-*indentDepth;
        else
            *paddingDepth = 2*m_tabSettings.m_indentSize;
        break;

    case arglist_open:
    case condition_paren_open:
    case member_init_nest_open:
        if (!lastToken)
            *paddingDepth = nextTokenPosition-*indentDepth;
        else
            addContinuationIndent(paddingDepth);
        break;

    case ternary_op:
        if (!lastToken)
            *paddingDepth = spaceOrNextTokenPosition-*indentDepth;
        else
            addContinuationIndent(paddingDepth);
        break;

    case stream_op:
        *paddingDepth = spaceOrNextTokenPosition-*indentDepth;
        break;
    case stream_op_cont:
        if (firstToken)
            *savedPaddingDepth = *paddingDepth = spaceOrNextTokenPosition-*indentDepth;
        break;

    case member_init_open:
        // undo the continuation indent of the parent
        *savedPaddingDepth = 0;

        // The paddingDepth is the expected location of the ',' and
        // identifiers are padded +2 from that in member_init_expected.
        if (firstToken)
            *paddingDepth = tokenPosition-*indentDepth;
        else
            *paddingDepth = m_tabSettings.m_indentSize - 2;
        break;

    case member_init_expected:
        *paddingDepth += 2;
        break;

    case member_init:
        // make continuation indents relative to identifier start
        *paddingDepth = tokenPosition - *indentDepth;
        if (firstToken) {
            // see comment in member_init_open
            *savedPaddingDepth = *paddingDepth - 2;
        }
        break;

    case case_cont:
        if (m_styleSettings.indentStatementsRelativeToSwitchLabels)
            *indentDepth += m_tabSettings.m_indentSize;
        break;

    case namespace_open:
    case class_open:
    case enum_open:
    case defun_open: {
        // undo the continuation indent of the parent
        *savedPaddingDepth = 0;

        // whether the { is followed by a non-comment token
        //bool followedByData = (!lastToken && !tokenAt(tokenIndex() + 1).isComment());
        bool followedByData = false;
        if (followedByData)
            *savedPaddingDepth = tokenPosition-*indentDepth; // pad the } to align with the {

        if (newState == class_open) {
            if (m_styleSettings.indentAccessSpecifiers
                || m_styleSettings.indentDeclarationsRelativeToAccessSpecifiers)
                *indentDepth += m_tabSettings.m_indentSize;
            if (m_styleSettings.indentAccessSpecifiers && m_styleSettings.indentDeclarationsRelativeToAccessSpecifiers)
                *indentDepth += m_tabSettings.m_indentSize;
        } else if (newState == defun_open) {
            if (m_styleSettings.indentFunctionBody || m_styleSettings.indentFunctionBraces)
                *indentDepth += m_tabSettings.m_indentSize;
            if (m_styleSettings.indentFunctionBody && m_styleSettings.indentFunctionBraces)
                *indentDepth += m_tabSettings.m_indentSize;
        } else if (newState == namespace_open) {
            if (m_styleSettings.indentNamespaceBody || m_styleSettings.indentNamespaceBraces)
                *indentDepth += m_tabSettings.m_indentSize;
            if (m_styleSettings.indentNamespaceBody && m_styleSettings.indentNamespaceBraces)
                *indentDepth += m_tabSettings.m_indentSize;
        } else {
            *indentDepth += m_tabSettings.m_indentSize;
        }

        if (followedByData)
            *paddingDepth = nextTokenPosition-*indentDepth;
        break;
    }

    case substatement_open:
        // undo parent continuation indent
        *savedPaddingDepth = 0;

        if (parentState.type == switch_statement) {
            if (m_styleSettings.indentSwitchLabels)
                *indentDepth += m_tabSettings.m_indentSize;
        } else {
            if (m_styleSettings.indentBlockBody || m_styleSettings.indentBlockBraces)
                *indentDepth += m_tabSettings.m_indentSize;
            if (m_styleSettings.indentBlockBody && m_styleSettings.indentBlockBraces)
                *indentDepth += m_tabSettings.m_indentSize;
        }
        break;

    case brace_list_open:
        if (!lastToken) {
            if (parentState.type == assign_open_or_initializer)
                *savedPaddingDepth = tokenPosition-*indentDepth;
            *paddingDepth = nextTokenPosition-*indentDepth;
        } else {
            // avoid existing continuation indents
            if (parentState.type == assign_open_or_initializer)
                *savedPaddingDepth = state(1).savedPaddingDepth;
            *paddingDepth = *savedPaddingDepth + m_tabSettings.m_indentSize;
        }
        break;

    case block_open:
        // case_cont already adds some indent, revert it for a block
        if (parentState.type == case_cont) {
            *indentDepth = parentState.savedIndentDepth;
            if (m_styleSettings.indentBlocksRelativeToSwitchLabels)
                *indentDepth += m_tabSettings.m_indentSize;
        }

        if (m_styleSettings.indentBlockBody)
            *indentDepth += m_tabSettings.m_indentSize;
        break;

    case condition_open:
        // undo the continuation indent of the parent
        *paddingDepth = parentState.savedPaddingDepth;
        *savedPaddingDepth = *paddingDepth;

        // fixed extra indent when continuing 'if (', but not for 'else if ('
        if (m_styleSettings.extraPaddingForConditionsIfConfusingAlign
            && nextTokenPosition-*indentDepth <= m_tabSettings.m_indentSize)
            *paddingDepth = 2*m_tabSettings.m_indentSize;
        else
            *paddingDepth = nextTokenPosition-*indentDepth;
        break;

    case substatement:
        // undo the continuation indent of the parent
        *savedPaddingDepth = 0;

        break;

    case maybe_else: {
        // set indent to outermost braceless savedIndent
        int outermostBraceless = 0;
        while (isBracelessState(state(outermostBraceless).type))
            ++outermostBraceless;
        *indentDepth = state(outermostBraceless - 1).savedIndentDepth;
        // this is where the else should go, if one appears - aligned to if_statement
        *savedIndentDepth = state().savedIndentDepth;
    }   break;

    case for_statement_paren_open:
        *paddingDepth = nextTokenPosition - *indentDepth;
        break;

    case multiline_comment_start:
        *indentDepth = tokenPosition + 2; // nextTokenPosition won't work
        break;

    case multiline_comment_cont:
        *indentDepth = tokenPosition;
        break;

    case cpp_macro:
    case cpp_macro_cont:
        *indentDepth = m_tabSettings.m_indentSize;
        break;

    case string_open:
    case raw_string_open:
        *paddingDepth = tokenPosition - *indentDepth;
        break;
    }

    // ensure padding and indent are >= 0
    *indentDepth = qMax(0, *indentDepth);
    *savedIndentDepth = qMax(0, *savedIndentDepth);
    *paddingDepth = qMax(0, *paddingDepth);
    *savedPaddingDepth = qMax(0, *savedPaddingDepth);
}


bool QtStyleCodeFormatter::shouldClearPaddingOnEnter(int state)
{
    switch (state) {
    case defun_open:
    case class_start:
    case class_open:
    case enum_start:
    case enum_open:
    case namespace_start:
    case namespace_open:
    case extern_start:
    case extern_open:
    case template_start:
    case if_statement:
    case else_clause:
    case for_statement:
    case switch_statement:
    case statement_with_condition:
    case do_statement:
    case return_statement:
    case block_open:
    case substatement_open:
    case substatement:
        return true;
    }
    return false;
}


} // namespace CppEditor
