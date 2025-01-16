#pragma once
#include "texteditor_global.h"
#include "htmlscanner.h"
#include "textdocumentlayout.h"
#include <syntaxhighlighter.h>
namespace Html{
class TEXTEDITOR_EXPORT Highlighter : public TextEditor::SyntaxHighlighter
{
    Q_OBJECT

public:
    Highlighter(QTextDocument *parent = nullptr);
    ~Highlighter() override;

protected:
    void highlightBlock(const QString &text) override;

    int onBlockStart();
    void onBlockEnd(int state);

    // The functions are notified whenever parentheses are encountered.
    // Custom behaviour can be added, for example storing info for indenting.
    void onOpeningParenthesis(QChar parenthesis, int pos, bool atStart);
    void onClosingParenthesis(QChar parenthesis, int pos, bool atEnd);

    bool maybeQmlKeyword(QStringView text) const;
    bool maybeQmlBuiltinType(QStringView text) const;

private:
    int m_braceDepth;
    int m_foldingIndent;
    bool m_inMultilineComment;

    Scanner m_scanner;
    TextEditor::Parentheses m_currentBlockParentheses;


};
}
