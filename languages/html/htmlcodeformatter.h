#pragma once

#include "texteditor_global.h"
#include "htmlscanner.h"
#include "codeformatter.h"
#include "languages/token.h"
#include "textdocumentlayout.h"
#include "languages/php/phpcodeformatter.h"
#include "languages/javascript/jscodeformatter.h"
#include "languages/css/csscodeformatter.h"
//#include "languages/javascript/jsscanner.h"
//#include "languages/css/cssscanner.h"
#include <QStack>
#include <QList>
#include <QVector>
#include <QMetaObject>

QT_BEGIN_NAMESPACE
class QTextDocument;
class QTextBlock;
QT_END_NAMESPACE

namespace Html {

bool isAutoClose(QStringView tag);

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


    virtual QList<Code::Token> tokenize(const QTextBlock& block) override;
    virtual QList<Code::Token> tokenize(const QString& text) override;

    virtual bool isIdentifier(QChar chr) override;
    virtual int indentifierPosition(const QTextBlock& block,int pos) override;
    virtual bool isVariantKind(int kind) override;

public: // must be public to make Q_GADGET introspection work
    enum StateType {
        invalid = 0,
        top_html,
        html,
        open_tag,
        close_tag,
        auto_tag,



        css_start,
        css_content,
        js_start,
        js_content,
        php_start,
        comment_start,
        comment_content,

        //topmost_intro=30,
        topmost_intro_php=30,
        top_php,


        topmost_intro_js = 200,
        top_js,



        topmost_intro_css = 300,
        top_css,
    };
    Q_ENUM(StateType)

protected:
    // extends Token::Kind from qmljsscanner.h
    // the entries until EndOfExistingTokenKinds must match
    enum TokenKind {
        None = Token::TokenEnd+1,
    };

    int extendedTokenKind(const Code::Token &token) const;


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
    void saveCurrentState(const QTextBlock &block);
    void restoreCurrentState(const QTextBlock &block);

    QStringView currentTokenText() const;

    int tokenizeBlock(const QTextBlock &block);

    void turnInto(int newState);

    bool tryInsideExpression(bool alsoExpression = false);
    bool tryStatement();

    void enterPHP();
    void enter(int newState);
    void leave(bool statementDone = false);
    void leavePHP();
    void leaveJS();
    void leaveCSS();
    void correctIndentation(const QTextBlock &block);



private:
    static QStack<State> initialState();

    QStack<State> m_beginState;
    QStack<State> m_currentState;
    QStack<State> m_newStates;
    QStack<State> m_htmlStoredState;
    QStack<State> m_codeStoredState;

    QList<Code::Token> m_tokens;
    QString m_currentLine;
    Code::Token m_currentToken;
    int m_tokenIndex;

    // should store indent level and padding instead
    int m_indentDepth;

    int m_tabSize;

    int m_indentSize;

    bool m_hasPHPState;


protected:
    Php::CodeFormatter phpFormatter;
    Javascript::CodeFormatter jsFormatter;
    Css::CodeFormatter cssFormatter;

public:
    void setIndentSize(int size);

protected:
    void onEnter(int newState, int *indentDepth, int *savedIndentDepth) const ;
    void adjustIndent(const QList<Code::Token> &tokens, int lexerState, int *indentDepth) const ;

    void saveBlockData(QTextBlock *block, const BlockData &data) const ;
    bool loadBlockData(const QTextBlock &block, BlockData *data) const ;

    void saveLexerState(QTextBlock *block, int state) const ;
    int loadLexerState(const QTextBlock &block) const ;

    void saveExpectedString(QTextBlock *block,const QByteArray& tag) const ;
    QByteArray loadExpectedString(const QTextBlock &block) const ;




    friend class Php::CodeFormatter;
    friend class Javascript::CodeFormatter;
    friend class Css::CodeFormatter;
};





}
