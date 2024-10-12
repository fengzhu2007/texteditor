#pragma once

#include "texteditor_global.h"

#include "cssscanner.h"
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


namespace Css {

class TEXTEDITOR_EXPORT CodeFormatter : public TextEditor::CodeFormatter
{
    Q_GADGET
public:
    CodeFormatter(Html::CodeFormatter* formatter=nullptr);
    explicit CodeFormatter(const TextEditor::TabSettings &tabSettings,Html::CodeFormatter* formatter=nullptr);
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



public: // must be public to make Q_GADGET introspection work
    enum StateType {
        invalid = 0,

        topmost_intro_css=130,
        top_css, // root for css

        definition_or_css, // file starts with identifier

        selector_open,//select
        atrule_attribute_open,//font-face
        atrule_selector_open, //media
        atrule_inner_selector_open,//media {div{}}

        selectorliteral_open,// after div {
        atrule_attributeliteral_open,// after @page {
        atrule_selectorliteral_open,// after @media {
        atrule_inner_selectorliteral_open,// after @media div {


        multiline_comment_start,
        multiline_comment_cont,

        block_open,
    };
    Q_ENUM(StateType)

protected:
    // extends Token::Kind from qmljsscanner.h
    // the entries until EndOfExistingTokenKinds must match
    enum TokenKind {
        None = Code::Token::TokenEnd+1,
        EndOfExistingTokenKinds,

        AtCharset,
        AtImport,
        AtNamespace,

        AtViewport,
        AtPage,
        AtFontFace,

        AtSupports,
        AtMedia,


        And,
        Or,
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

    Html::CodeFormatter* pHtmlFormatter;



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

    friend class Html::CodeFormatter;
};




} // namespace


