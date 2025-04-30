#pragma once

#include "texteditor_global.h"

#include "jsxscanner.h"
#include "codeformatter.h"
#include "languages/token.h"
#include "textdocumentlayout.h"
#include <QStack>
#include <QList>
#include <QVector>
#include <QMetaObject>

QT_BEGIN_NAMESPACE
class QTextDocument;
class QTextBlock;
QT_END_NAMESPACE

namespace Html{
class CodeFormatter;
class EditorCodeFormatter;
}

using namespace Code;


namespace Jsx {

class TEXTEDITOR_EXPORT CodeFormatter : public TextEditor::CodeFormatter
{
    Q_GADGET
public:
    CodeFormatter();
    explicit CodeFormatter(const TextEditor::TabSettings &tabSettings);
    virtual ~CodeFormatter();

    // updates all states up until block if necessary
    // it is safe to call indentFor on block afterwards
    virtual void updateStateUntil(const QTextBlock &block) override;

    // calculates the state change introduced by changing a single line
    virtual void updateLineStateChange(const QTextBlock &block) override;

    virtual int indentFor(const QTextBlock &block) override;
    virtual int indentForNewLineAfter(const QTextBlock &block) override;

    virtual void setTabSize(int tabSize) override;

    virtual void invalidateCache(QTextDocument *document) override;

    void initState();

    virtual bool isIdentifier(QChar chr) override;
    virtual int indentifierPosition(const QTextBlock& block,int pos) override;
    virtual bool isVariantKind(int kind) override;



public: // must be public to make Q_GADGET introspection work
    enum StateType {
        invalid = 0,
        html=2,
        topmost_intro_js=80,
        top_js, // root for js
        objectdefinition_or_js, // file starts with identifier

        multiline_comment_start,
        multiline_comment_cont,

        import_start, // after 'import'
        import_maybe_dot_or_version_or_as, // after string or identifier
        import_dot, // after .
        import_maybe_as, // after version
        import_as,

        property_start, // after 'property'
        property_modifiers, // after 'default' or readonly
        required_property, // after required
        property_list_open, // after 'list' as a type
        property_name, // after the type
        property_maybe_initializer, // after the identifier
        component_start, // after component
        component_name, // after component Name

        enum_start, // after 'enum'


        function_start, // after 'function'
        function_arglist_open, // after '(' starting function argument list
        function_arglist_closed, // after ')' in argument list, expecting '{'
        function_type_annotated_return, // after ':' expecting a type

        binding_or_objectdefinition, // after an identifier

        binding_assignment, // after : in a binding
        objectdefinition_open, // after {

        expression,
        expression_continuation, // at the end of the line, when the next line definitely is a continuation
        expression_maybe_continuation, // at the end of the line, when the next line may be an expression
        expression_or_objectdefinition, // after a binding starting with an identifier ("x: foo")
        expression_or_label, // when expecting a statement and getting an identifier

        paren_open, // opening ( in expression
        bracket_open, // opening [ in expression
        objectliteral_open, // opening { in expression

        objectliteral_assignment, // after : in object literal

        bracket_element_start, // after starting bracket_open or after ',' in bracket_open
        bracket_element_maybe_objectdefinition, // after an identifier in bracket_element_start

        ternary_op, // The ? : operator
        ternary_op_after_colon, // after the : in a ternary

        block_open,

        empty_statement, // for a ';', will be popped directly
        breakcontinue_statement, // for continue/break, may be followed by identifier

        if_statement, // After 'if'
        maybe_else, // after the first substatement in an if
        else_clause, // The else line of an if-else construct.

        condition_open, // Start of a condition in 'if', 'while', entered after opening paren

        substatement, // The first line after a conditional or loop construct.
        substatement_open, // The brace that opens a substatement block.

        labelled_statement, // after a label

        return_statement, // After 'return'
        throw_statement, // After 'throw'

        statement_with_condition, // After the 'for', 'while', ... token
        statement_with_condition_paren_open, // While inside the (...)

        try_statement, // after 'try'
        catch_statement, // after 'catch', nested in try_statement
        finally_statement, // after 'finally', nested in try_statement
        maybe_catch_or_finally, // after ther closing '}' of try_statement and catch_statement, nested in try_statement

        do_statement, // after 'do'
        do_statement_while_paren_open, // after '(' in while clause

        switch_statement, // After 'switch' token
        case_start, // after a 'case' or 'default' token
        case_cont, // after the colon in a case/default


        html_element,//<div
        html_element_inner,
        html_element_start_tag,
        html_element_end_tag,

    };
    Q_ENUM(StateType)

protected:
    // extends Token::Kind from qmljsscanner.h
    // the entries until EndOfExistingTokenKinds must match
    enum TokenKind {
        None = Code::Token::TokenEnd+1,
        EndOfExistingTokenKinds,

        Break,
        Case,
        Catch,
        Continue,
        Debugger,
        Default,
        Delete,
        Do,
        Else,
        Enum,
        Finally,
        For,
        Function,
        If,
        In,
        Instanceof,
        New,
        Return,
        Switch,
        This,
        Throw,
        Try,
        Typeof,
        Var,
        Void,
        While,
        With,

        Import,
        Signal,
        On,
        As,
        List,
        Property,
        Required,
        Component,
        Readonly,

        Question,
        PlusPlus,
        MinusMinus
    };

    TokenKind extendedTokenKind(const Code::Token &token) const;



    State state(int belowTop = 0) const;
    const QVector<State> &newStatesThisLine() const;
    int tokenIndex() const;
    int tokenCount() const;
    const Code::Token &currentToken() const;
    const Code::Token &tokenAt(int idx) const;
    int column(int position) const;

    bool isBracelessState(int type) const;
    bool isExpressionEndState(int type) const;

    void dump() const;
    QString stateToString(int type) const;

private:
    void recalculateStateAfter(const QTextBlock &block);
    inline void setTokens(const QList<Token>& tokens){
        this->m_tokens = tokens;
    }
    void recalculateStateAfter(const QTextBlock &block,int lexerState,const QString& currentLine,int* tokenIndex);
    void saveCurrentState(const QTextBlock &block);
    void restoreCurrentState(const QTextBlock &block);

    QStringView currentTokenText() const;

    int tokenizeBlock(const QTextBlock &block);

    void turnInto(int newState);

    bool tryInsideExpression(bool alsoExpression = false);
    bool tryStatement();

    void enter(int newState);
    void leave(bool statementDone = false);
    void correctIndentation(const QTextBlock &block);

private:
    static QStack<State> initialState();

    QStack<State> m_beginState;
    QStack<State> m_currentState;
    QStack<State> m_newStates;

    QList<Token> m_tokens;
    QString m_currentLine;
    Token m_currentToken;
    int m_tokenIndex;

    // should store indent level and padding instead
    int m_indentDepth;

    int m_tabSize;

public:

    void setIndentSize(int size);

protected:
    void onEnter(int newState, int *indentDepth, int *savedIndentDepth) const ;
    void adjustIndent(const QList<Code::Token> &tokens, int lexerState, int *indentDepth) const ;

    void saveBlockData(QTextBlock *block, const BlockData &data) const ;
    bool loadBlockData(const QTextBlock &block, BlockData *data) const ;

    void saveLexerState(QTextBlock *block, int state) const ;
    int loadLexerState(const QTextBlock &block) const ;

private:
    int m_indentSize;

};




} // namespace


