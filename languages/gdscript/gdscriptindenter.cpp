// GDScript language support for Godot Engine

#include "gdscriptindenter.h"
#include "gdscriptscanner.h"

#include "tabsettings.h"

#include <algorithm>

namespace GDScript {

static bool isEmptyLine(const QString &t)
{
    return std::all_of(t.cbegin(), t.cend(), [] (QChar c) { return c.isSpace(); });
}

static inline bool isEmptyLine(const QTextBlock &block)
{
    return isEmptyLine(block.text());
}

static QTextBlock previousNonEmptyBlock(const QTextBlock &block)
{
    QTextBlock result = block;
    while (result.isValid() && isEmptyLine(result))
        result = result.previous();
    return result;
}

class GDScriptIndenter : public TextEditor::TextIndenter
{
public:
    explicit GDScriptIndenter(QTextDocument *doc)
        : TextEditor::TextIndenter(doc)
    {}

    virtual QString name() override {return QString::fromUtf8("GDScript");}

private:
    bool isElectricCharacter(const QChar &ch) const override;
    int indentFor(const QTextBlock &block,
                  const TextEditor::TabSettings &tabSettings,
                  int cursorPositionInEditor = -1) override;

    bool isElectricLine(const QString &line) const;
    int getIndentDiff(const QString &previousLine,
                      const TextEditor::TabSettings &tabSettings) const;
};

/**
 * @brief Does given character change indentation level?
 * @param ch Any value
 * @return True if character increases indentation level at the next line
 */
bool GDScriptIndenter::isElectricCharacter(const QChar &ch) const
{
    return ch == ':';
}

int GDScriptIndenter::indentFor(const QTextBlock &block,
                              const TextEditor::TabSettings &tabSettings,
                              int /*cursorPositionInEditor*/)
{
    QTextBlock previousBlock = block.previous();
    if (!previousBlock.isValid())
        return 0;

    // When pasting in actual code, try to skip back past empty lines to an
    // actual code line to find a suitable indentation.
    if (!isEmptyLine(block)) {
        const QTextBlock previousNonEmpty = previousNonEmptyBlock(previousBlock);
        if (previousNonEmpty.isValid())
            previousBlock = previousNonEmpty;
    }

    QString previousLine = previousBlock.text();
    int indentation = tabSettings.indentationColumn(previousLine);

    if (isElectricLine(previousLine))
        indentation += tabSettings.m_indentSize;
    else
        indentation = qMax<int>(0, indentation + getIndentDiff(previousLine, tabSettings));

    return indentation;
}

/// @return True if electric character is last non-space character at given string
bool GDScriptIndenter::isElectricLine(const QString &line) const
{
    if (line.isEmpty())
        return false;

    // trim spaces in 'if True:  '
    int index = line.length() - 1;
    while (index > 0 && line[index].isSpace())
        --index;

    return isElectricCharacter(line[index]);
}

/// @return negative indent diff if previous line breaks control flow branch
int GDScriptIndenter::getIndentDiff(const QString &previousLine,
                                  const TextEditor::TabSettings &tabSettings) const
{
    // GDScript keywords that break control flow
    static const QStringList jumpKeywords = {
        "return", "break", "continue", "pass", "yield"
    };

    Scanner sc(previousLine.constData(), previousLine.length());
    forever {
        auto tk = sc.read();
        if (tk.kind == Token::Keyword && jumpKeywords.contains(sc.value(tk)))
            return -tabSettings.m_indentSize;
        if (tk.kind != Token::Whitespace)
            break;
    }
    return 0;
}

TextEditor::TextIndenter *createIndenter(QTextDocument *doc)
{
    return new GDScriptIndenter(doc);
}

} // namespace GDScript
