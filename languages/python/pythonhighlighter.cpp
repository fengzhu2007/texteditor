// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

/**
 * @brief The Highlighter class pre-highlights Python source using simple scanner.
 *
 * Highlighter doesn't highlight user types (classes and enumerations), syntax
 * and semantic errors, unnecessary code, etc. It's implements only
 * basic highlight mechanism.
 *
 * Main highlight procedure is highlightBlock().
 */

#include "pythonhighlighter.h"
#include "pythonscanner.h"

#include "textdocument.h"
#include "textdocumentlayout.h"
#include "texteditorconstants.h"
#include <utils/qtcassert.h>


namespace Python {

using namespace Internal;

/**
 * @class PythonEditor::Internal::PythonHighlighter
 * @brief Handles incremental lexical highlighting, but not semantic
 *
 * Incremental lexical highlighting works every time when any character typed
 * or some text inserted (i.e. copied & pasted).
 * Each line keeps associated scanner state - integer number. This state is the
 * scanner context for next line. For example, 3 quotes begin a multiline
 * string, and each line up to next 3 quotes has state 'MultiLineString'.
 *
 * @code
 *  def __init__:               # Normal
 *      self.__doc__ = """      # MultiLineString (next line is inside)
 *                     banana   # MultiLineString
 *                     """      # Normal
 * @endcode
 */

static TextEditor::TextStyle styleForFormat(int format)
{
    using namespace TextEditor;
    const auto f = Internal::Format(format);
    switch (f) {
    case Internal::Format_Number: return C_NUMBER;
    case Internal::Format_String: return C_STRING;
    case Internal::Format_Keyword: return C_KEYWORD;
    case Internal::Format_Type: return C_TYPE;
    case Internal::Format_ClassField: return C_FIELD;
    case Internal::Format_MagicAttr: return C_JS_SCOPE_VAR;
    case Internal::Format_Operator: return C_OPERATOR;
    case Internal::Format_Comment: return C_COMMENT;
    case Internal::Format_Doxygen: return C_DOXYGEN_COMMENT;
    case Internal::Format_Identifier: return C_TEXT;
    case Internal::Format_Whitespace: return C_VISUAL_WHITESPACE;
    case Internal::Format_ImportedModule: return C_STRING;
    case Internal::Format_LParen: return C_OPERATOR;
    case Internal::Format_RParen: return C_OPERATOR;
    case Internal::Format_FormatsAmount:
        QTC_CHECK(false); // should never get here
        return C_TEXT;
    }
    QTC_CHECK(false); // should never get here
    return C_TEXT;
}

Highlighter::Highlighter()
{
    setTextFormatCategories(Internal::Format_FormatsAmount, styleForFormat);
}

/**
 * @brief PythonHighlighter::highlightBlock highlights single line of Python code
 * @param text is single line without EOLN symbol. Access to all block data
 * can be obtained through inherited currentBlock() function.
 *
 * This function receives state (int number) from previously highlighted block,
 * scans block using received state and sets initial highlighting for current
 * block. At the end, it saves internal state in current block.
 */
void Highlighter::highlightBlock(const QString &text)
{
    int initialState = previousBlockState();
    if (initialState == -1)
        initialState = 0;
    setCurrentBlockState(highlightLine(text, initialState));
}

/**
 * @return True if this keyword is acceptable at start of import line
 */
static bool isImportKeyword(const QString &keyword)
{
    return keyword == "import" || keyword == "from";
}

static int indent(const QString &line)
{
    for (int i = 0, size = line.size(); i < size; ++i) {
        if (!line.at(i).isSpace())
            return i;
    }
    return -1;
}

static void setFoldingIndent(const QTextBlock &block, int indent)
{
    if (TextEditor::TextBlockUserData *userData = TextEditor::TextDocumentLayout::userData(block)) {
         userData->setFoldingIndent(indent);
         userData->setFoldingStartIncluded(false);
         userData->setFoldingEndIncluded(false);
    }
}

/**
 * @brief Highlight line of code, returns new block state
 * @param text Source code to highlight
 * @param initialState Initial state of scanner, retrieved from previous block
 * @return Final state of scanner, should be saved with current block
 */
int Highlighter::highlightLine(const QString &text, int initialState)
{
    Scanner scanner(text.constData(), text.size());
    scanner.setState(initialState);

    const int pos = indent(text);
    if (pos < 0) {
        // Empty lines do not change folding indent
        setFoldingIndent(currentBlock(), m_lastIndent);
    } else {
        m_lastIndent = pos;
        if (pos == 0 && text.startsWith('#') && !text.startsWith("#!")) {
            // A comment block at indentation 0. Fold on first line.
            setFoldingIndent(currentBlock(), withinLicenseHeader ? 1 : 0);
            withinLicenseHeader = true;
        } else {
            // Normal Python code. Line indentation can be used as folding indent.
            setFoldingIndent(currentBlock(), m_lastIndent);
            withinLicenseHeader = false;
        }
    }

    Internal::FormatToken tk;
    TextEditor::Parentheses parentheses;
    bool hasOnlyWhitespace = true;
    while (!(tk = scanner.read()).isEndOfBlock()) {
        Internal::Format format = tk.format();
        if (format == Internal::Format_Keyword && isImportKeyword(scanner.value(tk)) && hasOnlyWhitespace) {
            setFormat(tk.begin(), tk.length(), formatForCategory(format));
            highlightImport(scanner);
        } else if (format == Internal::Format_Comment
                   || format == Internal::Format_String
                   || format == Internal::Format_Doxygen) {
            setFormatWithSpaces(text, tk.begin(), tk.length(), formatForCategory(format));
        } else {
            if (format == Format_LParen) {
                parentheses.append(TextEditor::Parenthesis(TextEditor::Parenthesis::Opened,
                                                           text.at(tk.begin()), tk.begin()));
            } else if (format == Format_RParen) {
                parentheses.append(TextEditor::Parenthesis(TextEditor::Parenthesis::Closed,
                                                           text.at(tk.begin()), tk.begin()));
            }
            setFormat(tk.begin(), tk.length(), formatForCategory(format));
        }

        if (format != Format_Whitespace)
            hasOnlyWhitespace = false;
    }
    TextEditor::TextDocumentLayout::setParentheses(currentBlock(), parentheses);
    return scanner.state();
}

/**
 * @brief Highlights rest of line as import directive
 */
void Highlighter::highlightImport(Scanner &scanner)
{
    FormatToken tk;
    while (!(tk = scanner.read()).isEndOfBlock()) {
        Format format = tk.format();
        if (tk.format() == Format_Identifier)
            format = Format_ImportedModule;
        setFormat(tk.begin(), tk.length(), formatForCategory(format));
    }
}

} // namespace Python
