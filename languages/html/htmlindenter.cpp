#include "htmlindenter.h"

#include "htmlcodeformatter.h"
#include "tabsettings.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QDebug>
namespace Html {

class Indenter final : public TextEditor::TextIndenter
{
public:
    explicit Indenter(QTextDocument *doc)
        : TextEditor::TextIndenter(doc)
    {}

    virtual QString name() override {return QString::fromUtf8("Html");}

    bool isElectricCharacter(const QChar &ch) const final;
    void indentBlock(const QTextBlock &block,
                     const QChar &typedChar,
                     const TextEditor::TabSettings &tabSettings,
                     int cursorPositionInEditor = -1) final;
    void invalidateCache() final;

    int indentFor(const QTextBlock &block,
                  const TextEditor::TabSettings &tabSettings,
                  int cursorPositionInEditor = -1) final;
    int visualIndentFor(const QTextBlock &block,
                        const TextEditor::TabSettings &tabSettings) final;
    TextEditor::IndentationForBlock indentationForBlocks(const QVector<QTextBlock> &blocks,
                                                         const TextEditor::TabSettings &tabSettings,
                                                         int cursorPositionInEditor = -1) final;
};

bool Indenter::isElectricCharacter(const QChar &ch) const
{
    return ch == QLatin1Char('{')
           || ch == QLatin1Char('}')
           || ch == QLatin1Char(']')
           || ch == QLatin1Char(':');
}

void Indenter::indentBlock(const QTextBlock &block,
                                 const QChar &typedChar,
                                 const TextEditor::TabSettings &tabSettings,
                                 int /*cursorPositionInEditor*/)
{
    const int depth = indentFor(block, tabSettings);
    if (depth == -1)
        return;

    // may be can delete.
    Html::CodeFormatter codeFormatter(tabSettings);
    codeFormatter.updateStateUntil(block);

    if (isElectricCharacter(typedChar)) {
        // only reindent the current line when typing electric characters if the
        // indent is the same it would be if the line were empty
        const int newlineIndent = codeFormatter.indentForNewLineAfter(block.previous());
        if (tabSettings.indentationColumn(block.text()) != newlineIndent)
            return;
    }

    tabSettings.indentLine(block, depth);
}

void Indenter::invalidateCache()
{
    Html::CodeFormatter codeFormatter;
    codeFormatter.invalidateCache(m_doc);
}

int Indenter::indentFor(const QTextBlock &block,
                              const TextEditor::TabSettings &tabSettings,
                              int /*cursorPositionInEditor*/)
{
    Html::CodeFormatter codeFormatter(tabSettings);
    codeFormatter.updateStateUntil(block);
    return codeFormatter.indentFor(block);
}

int Indenter::visualIndentFor(const QTextBlock &block, const TextEditor::TabSettings &tabSettings)
{
    return indentFor(block, tabSettings);
}

TextEditor::IndentationForBlock Indenter::indentationForBlocks(
    const QVector<QTextBlock> &blocks,
    const TextEditor::TabSettings &tabSettings,
    int /*cursorPositionInEditor*/)
{
    Html::CodeFormatter codeFormatter(tabSettings);

    codeFormatter.updateStateUntil(blocks.last());

    TextEditor::IndentationForBlock ret;
    for (QTextBlock block : blocks)
        ret.insert(block.blockNumber(), codeFormatter.indentFor(block));
    return ret;
}



TextEditor::TextIndenter *createIndenter(QTextDocument *doc)
{
    return new Html::Indenter(doc);
}

void indent(QTextDocument *doc, int startLine, int endLine, const TextEditor::TabSettings &tabSettings)
{
    if (startLine <= 0)
        return;

    QTextCursor tc(doc);

    tc.beginEditBlock();
    for (int i = startLine; i <= endLine; i++) {
        // FIXME: block.next() should be faster.
        QTextBlock block = doc->findBlockByNumber(i);
        if (block.isValid()) {
            Html::Indenter indenter(doc);
            indenter.indentBlock(block, QChar::Null, tabSettings);
        }
    }
    tc.endEditBlock();
}


}
