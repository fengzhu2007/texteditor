// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
//
// v2 split of texteditor.cpp (legacy file kept at ../texteditor.cpp).
// Commands: goto/navigation, block selection, line editing, deletion, case conversion, indentation, line sorting.
//

#include "texteditorwidget_p.h"

namespace TextEditor {

using namespace Internal;

QList<QTextCursor> TextEditorWidgetPrivate::generateCursorsForBlockSelection(
    const BlockSelection &blockSelection)
{
    const TabSettings tabSettings = m_document->tabSettings();

    QList<QTextCursor> result;
    QTextBlock block = m_document->document()->findBlockByNumber(blockSelection.anchorBlockNumber);
    QTextCursor cursor(block);
    cursor.setPosition(block.position()
                       + tabSettings.positionAtColumn(block.text(), blockSelection.anchorColumn));

    const bool forward = blockSelection.blockNumber > blockSelection.anchorBlockNumber
                         || (blockSelection.blockNumber == blockSelection.anchorBlockNumber
                             && blockSelection.column == blockSelection.anchorColumn);

    while (block.isValid()) {
        const QString &blockText = block.text();
        const int columnCount = tabSettings.columnCountForText(blockText);
        if (blockSelection.anchorColumn <= columnCount || blockSelection.column <= columnCount) {
            const int anchor = tabSettings.positionAtColumn(blockText, blockSelection.anchorColumn);
            const int position = tabSettings.positionAtColumn(blockText, blockSelection.column);
            cursor.setPosition(block.position() + anchor);
            cursor.setPosition(block.position() + position, QTextCursor::KeepAnchor);
            result.append(cursor);
        }
        if (block.blockNumber() == blockSelection.blockNumber)
            break;
        block = forward ? block.next() : block.previous();
    }
    return result;
}


void TextEditorWidgetPrivate::initBlockSelection()
{
    const TabSettings tabSettings = m_document->tabSettings();
    for (const QTextCursor &cursor : m_cursors) {
        const int column = tabSettings.columnAtCursorPosition(cursor);
        QTextCursor anchor = cursor;
        anchor.setPosition(anchor.anchor());
        const int anchorColumn = tabSettings.columnAtCursorPosition(anchor);
        m_blockSelections.append({cursor.blockNumber(), column, anchor.blockNumber(), anchorColumn});
    }
}


void TextEditorWidgetPrivate::clearBlockSelection()
{
    m_blockSelections.clear();
}


void TextEditorWidgetPrivate::handleMoveBlockSelection(QTextCursor::MoveOperation op)
{
    if (m_blockSelections.isEmpty())
        initBlockSelection();
    QList<QTextCursor> cursors;
    for (BlockSelection &blockSelection : m_blockSelections) {
        switch (op) {
        case QTextCursor::Up:
            blockSelection.blockNumber = qMax(0, blockSelection.blockNumber - 1);
            break;
        case QTextCursor::Down:
            blockSelection.blockNumber = qMin(m_document->document()->blockCount() - 1,
                                              blockSelection.blockNumber + 1);
            break;
        case QTextCursor::NextCharacter:
            ++blockSelection.column;
            break;
        case QTextCursor::PreviousCharacter:
            blockSelection.column = qMax(0, blockSelection.column - 1);
            break;
        default:
            return;
        }
        cursors.append(generateCursorsForBlockSelection(blockSelection));
    }
    q->setMultiTextCursor(MultiTextCursor(cursors));
}


void TextEditorWidget::gotoBlockStart()
{
    if (multiTextCursor().hasMultipleCursors())
        return;

    QTextCursor cursor = textCursor();
    if (TextBlockUserData::findPreviousOpenParenthesis(&cursor, false)) {
        setTextCursor(cursor);
        d->_q_matchParentheses();
    }
}


void TextEditorWidget::gotoBlockEnd()
{
    if (multiTextCursor().hasMultipleCursors())
        return;

    QTextCursor cursor = textCursor();
    if (TextBlockUserData::findNextClosingParenthesis(&cursor, false)) {
        setTextCursor(cursor);
        d->_q_matchParentheses();
    }
}


void TextEditorWidget::gotoBlockStartWithSelection()
{
    if (multiTextCursor().hasMultipleCursors())
        return;

    QTextCursor cursor = textCursor();
    if (TextBlockUserData::findPreviousOpenParenthesis(&cursor, true)) {
        setTextCursor(cursor);
        d->_q_matchParentheses();
    }
}


void TextEditorWidget::gotoBlockEndWithSelection()
{
    if (multiTextCursor().hasMultipleCursors())
        return;

    QTextCursor cursor = textCursor();
    if (TextBlockUserData::findNextClosingParenthesis(&cursor, true)) {
        setTextCursor(cursor);
        d->_q_matchParentheses();
    }
}


void TextEditorWidget::gotoDocumentStart()
{
    d->moveCursor(QTextCursor::Start);
}


void TextEditorWidget::gotoDocumentEnd()
{
    d->moveCursor(QTextCursor::End);
}


void TextEditorWidget::gotoLineStart()
{
    d->handleHomeKey(false, true);
}


void TextEditorWidget::gotoLineStartWithSelection()
{
    d->handleHomeKey(true, true);
}


void TextEditorWidget::gotoLineEnd()
{
    d->moveCursor(QTextCursor::EndOfLine);
}


void TextEditorWidget::gotoLineEndWithSelection()
{
    d->moveCursor(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
}


void TextEditorWidget::gotoNextLine()
{
    d->moveCursor(QTextCursor::Down);
}


void TextEditorWidget::gotoNextLineWithSelection()
{
    d->moveCursor(QTextCursor::Down, QTextCursor::KeepAnchor);
}


void TextEditorWidget::gotoPreviousLine()
{
    d->moveCursor(QTextCursor::Up);
}


void TextEditorWidget::gotoPreviousLineWithSelection()
{
    d->moveCursor(QTextCursor::Up, QTextCursor::KeepAnchor);
}


void TextEditorWidget::gotoPreviousCharacter()
{
    d->moveCursor(QTextCursor::PreviousCharacter);
}


void TextEditorWidget::gotoPreviousCharacterWithSelection()
{
    d->moveCursor(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
}


void TextEditorWidget::gotoNextCharacter()
{
    d->moveCursor(QTextCursor::NextCharacter);
}


void TextEditorWidget::gotoNextCharacterWithSelection()
{
    d->moveCursor(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
}


void TextEditorWidget::gotoPreviousWord()
{
    d->moveCursor(QTextCursor::PreviousWord);
}


void TextEditorWidget::gotoPreviousWordWithSelection()
{
    d->moveCursor(QTextCursor::PreviousWord, QTextCursor::KeepAnchor);
}


void TextEditorWidget::gotoNextWord()
{
    d->moveCursor(QTextCursor::NextWord);
}


void TextEditorWidget::gotoNextWordWithSelection()
{
    d->moveCursor(QTextCursor::NextWord, QTextCursor::KeepAnchor);
}


void TextEditorWidget::gotoPreviousWordCamelCase()
{
    MultiTextCursor cursor = multiTextCursor();
    CamelCaseCursor::left(&cursor, this, QTextCursor::MoveAnchor);
    setMultiTextCursor(cursor);
}


void TextEditorWidget::gotoPreviousWordCamelCaseWithSelection()
{
    MultiTextCursor cursor = multiTextCursor();
    CamelCaseCursor::left(&cursor, this, QTextCursor::KeepAnchor);
    setMultiTextCursor(cursor);
}


void TextEditorWidget::gotoNextWordCamelCase()
{
    MultiTextCursor cursor = multiTextCursor();
    CamelCaseCursor::right(&cursor, this, QTextCursor::MoveAnchor);
    setMultiTextCursor(cursor);
}


void TextEditorWidget::gotoNextWordCamelCaseWithSelection()
{
    MultiTextCursor cursor = multiTextCursor();
    CamelCaseCursor::right(&cursor, this, QTextCursor::KeepAnchor);
    setMultiTextCursor(cursor);
}


bool TextEditorWidget::selectBlockUp()
{
    if (multiTextCursor().hasMultipleCursors())
        return false;

    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection())
        d->m_selectBlockAnchor = cursor;
    else
        cursor.setPosition(cursor.selectionStart());

    if (!TextBlockUserData::findPreviousOpenParenthesis(&cursor, false))
        return false;
    if (!TextBlockUserData::findNextClosingParenthesis(&cursor, true))
        return false;

    setTextCursor(Text::flippedCursor(cursor));
    d->_q_matchParentheses();
    return true;
}


bool TextEditorWidget::selectBlockDown()
{
    if (multiTextCursor().hasMultipleCursors())
        return false;

    QTextCursor tc = textCursor();
    QTextCursor cursor = d->m_selectBlockAnchor;

    if (!tc.hasSelection() || cursor.isNull())
        return false;
    tc.setPosition(tc.selectionStart());

    forever {
        QTextCursor ahead = cursor;
        if (!TextBlockUserData::findPreviousOpenParenthesis(&ahead, false))
            break;
        if (ahead.position() <= tc.position())
            break;
        cursor = ahead;
    }
    if ( cursor != d->m_selectBlockAnchor)
        TextBlockUserData::findNextClosingParenthesis(&cursor, true);

    setTextCursor(Text::flippedCursor(cursor));
    d->_q_matchParentheses();
    return true;
}


void TextEditorWidget::selectWordUnderCursor()
{
    MultiTextCursor cursor = multiTextCursor();
    for (QTextCursor &c : cursor) {
        if (!c.hasSelection())
            c.select(QTextCursor::WordUnderCursor);
    }
    setMultiTextCursor(cursor);
}


void TextEditorWidget::joinLines()
{
    MultiTextCursor cursor = multiTextCursor();
    cursor.beginEditBlock();
    for (QTextCursor &c : cursor) {
        QTextCursor start = c;
        QTextCursor end = c;

        start.setPosition(c.selectionStart());
        end.setPosition(c.selectionEnd() - 1);

        int lineCount = qMax(1, end.blockNumber() - start.blockNumber());

        c.setPosition(c.selectionStart());
        while (lineCount--) {
            c.movePosition(QTextCursor::NextBlock);
            c.movePosition(QTextCursor::StartOfBlock);
            c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            QString cutLine = c.selectedText();

            // Collapse leading whitespaces to one or insert whitespace
            cutLine.replace(QRegularExpression(QLatin1String("^\\s*")), QLatin1String(" "));
            c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            c.removeSelectedText();

            c.movePosition(QTextCursor::PreviousBlock);
            c.movePosition(QTextCursor::EndOfBlock);

            c.insertText(cutLine);
        }
    }
    cursor.endEditBlock();
    cursor.mergeCursors();
    setMultiTextCursor(cursor);
}


void TextEditorWidget::insertLineAbove()
{
    MultiTextCursor cursor = multiTextCursor();
    cursor.beginEditBlock();
    for (QTextCursor &c : cursor) {
        // If the cursor is at the beginning of the document,
        // it should still insert a line above the current line.
        c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        c.insertBlock();
        c.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor);
        d->m_document->autoIndent(c);
    }
    cursor.endEditBlock();
    setMultiTextCursor(cursor);
}


void TextEditorWidget::insertLineBelow()
{
    MultiTextCursor cursor = multiTextCursor();
    cursor.beginEditBlock();
    for (QTextCursor &c : cursor) {
        //qDebug()<<"line"<<c.block().blockNumber()<<c.block().text()<<"pos:"<<c.positionInBlock();

        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
        c.insertBlock();
        //c.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
        d->m_document->autoIndent(c);
    }
    cursor.endEditBlock();
    setMultiTextCursor(cursor);
}


void TextEditorWidget::moveLineUp()
{
    d->moveLineUpDown(true);
}


void TextEditorWidget::moveLineDown()
{
    d->moveLineUpDown(false);
}


void TextEditorWidget::uppercaseSelection()
{
    d->transformSelection([](const QString &str) { return str.toUpper(); });
}


void TextEditorWidget::lowercaseSelection()
{
    d->transformSelection([](const QString &str) { return str.toLower(); });
}


void TextEditorWidget::sortSelectedLines()
{
    d->transformSelectedLines([](QStringList &list) { list.sort(); });
}


void TextEditorWidget::indent()
{
    setMultiTextCursor(textDocument()->indent(multiTextCursor()));
}


void TextEditorWidget::unindent()
{
    setMultiTextCursor(textDocument()->unindent(multiTextCursor()));
}


void TextEditorWidget::undo()
{
    doSetTextCursor(multiTextCursor().mainCursor());
    QPlainTextEdit::undo();
}


void TextEditorWidget::redo()
{
    doSetTextCursor(multiTextCursor().mainCursor());
    QPlainTextEdit::redo();
}


void TextEditorWidgetPrivate::moveLineUpDown(bool up)
{
    if (m_cursors.hasMultipleCursors())
        return;
    QTextCursor cursor = q->textCursor();
    QTextCursor move = cursor;

    move.setVisualNavigation(false); // this opens folded items instead of destroying them

    if (m_moveLineUndoHack)
        move.joinPreviousEditBlock();
    else
        move.beginEditBlock();

    bool hasSelection = cursor.hasSelection();

    if (hasSelection) {
        move.setPosition(cursor.selectionStart());
        move.movePosition(QTextCursor::StartOfBlock);
        move.setPosition(cursor.selectionEnd(), QTextCursor::KeepAnchor);
        move.movePosition(move.atBlockStart() ? QTextCursor::PreviousCharacter: QTextCursor::EndOfBlock,
                          QTextCursor::KeepAnchor);
    } else {
        move.movePosition(QTextCursor::StartOfBlock);
        move.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    }
    QString text = move.selectedText();

    RefactorMarkers affectedMarkers;
    RefactorMarkers nonAffectedMarkers;
    QList<int> markerOffsets;

    const QList<RefactorMarker> markers = m_refactorOverlay->markers();
    for (const RefactorMarker &marker : markers) {
        //test if marker is part of the selection to be moved
        if ((move.selectionStart() <= marker.cursor.position())
                && (move.selectionEnd() >= marker.cursor.position())) {
            affectedMarkers.append(marker);
            //remember the offset of markers in text
            int offset = marker.cursor.position() - move.selectionStart();
            markerOffsets.append(offset);
        } else {
            nonAffectedMarkers.append(marker);
        }
    }

    move.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    move.removeSelectedText();

    if (up) {
        move.movePosition(QTextCursor::PreviousBlock);
        move.insertBlock();
        move.movePosition(QTextCursor::PreviousCharacter);
    } else {
        move.movePosition(QTextCursor::EndOfBlock);
        if (move.atBlockStart()) { // empty block
            move.movePosition(QTextCursor::NextBlock);
            move.insertBlock();
            move.movePosition(QTextCursor::PreviousCharacter);
        } else {
            move.insertBlock();
        }
    }

    int start = move.position();
    move.clearSelection();
    move.insertText(text);
    int end = move.position();

    if (hasSelection) {
        move.setPosition(end);
        move.setPosition(start, QTextCursor::KeepAnchor);
    } else {
        move.setPosition(start);
    }

    //update positions of affectedMarkers
    for (int i=0;i < affectedMarkers.count(); i++) {
        int newPosition = start + markerOffsets.at(i);
        affectedMarkers[i].cursor.setPosition(newPosition);
    }
    m_refactorOverlay->setMarkers(nonAffectedMarkers + affectedMarkers);

    bool shouldReindent = true;
    if (m_commentDefinition.isValid()) {
        if (m_commentDefinition.hasMultiLineStyle()) {
            // Don't have any single line comments; try multi line.
            if (text.startsWith(m_commentDefinition.multiLineStart)
                && text.endsWith(m_commentDefinition.multiLineEnd)) {
                shouldReindent = false;
            }
        }
        if (shouldReindent && m_commentDefinition.hasSingleLineStyle()) {
            shouldReindent = false;
            QTextBlock block = move.block();
            while (block.isValid() && block.position() < end) {
                if (!block.text().startsWith(m_commentDefinition.singleLine))
                    shouldReindent = true;
                block = block.next();
            }
        }
    }

    if (shouldReindent) {
        // The text was not commented at all; re-indent.
        m_document->autoReindent(move);
    }
    move.endEditBlock();

    q->setTextCursor(move);
    m_moveLineUndoHack = true;
}


void TextEditorWidget::cleanWhitespace()
{
    d->m_document->cleanWhitespace(textCursor());
}


void TextEditorWidget::viewPageUp()
{
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
}


void TextEditorWidget::viewPageDown()
{
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
}


void TextEditorWidget::viewLineUp()
{
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
}


void TextEditorWidget::viewLineDown()
{
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
}


void TextEditorWidgetPrivate::moveCursor(QTextCursor::MoveOperation operation,
                                         QTextCursor::MoveMode mode)
{
    MultiTextCursor cursor = m_cursors;
    cursor.movePosition(operation, mode);
    q->setMultiTextCursor(cursor);
}


void TextEditorWidgetPrivate::addCursorsToLineEnds()
{
    MultiTextCursor multiCursor = q->multiTextCursor();
    MultiTextCursor newMultiCursor;
    const QList<QTextCursor> cursors = multiCursor.cursors();

    if (multiCursor.cursorCount() == 0)
        return;

    QTextDocument *document = q->document();

    for (const QTextCursor &cursor : cursors) {
        if (!cursor.hasSelection())
            continue;

        QTextBlock block = document->findBlock(cursor.selectionStart());

        while (block.isValid()) {
            int blockEnd = block.position() + block.length() - 1;
            if (blockEnd >= cursor.selectionEnd()) {
                break;
            }

            QTextCursor newCursor(document);
            newCursor.setPosition(blockEnd);
            newMultiCursor.addCursor(newCursor);

            block = block.next();
        }
    }

    if (!newMultiCursor.isNull()) {
        q->setMultiTextCursor(newMultiCursor);
    }
}


void TextEditorWidgetPrivate::addSelectionNextFindMatch()
{
    MultiTextCursor cursor = q->multiTextCursor();
    const QList<QTextCursor> cursors = cursor.cursors();

    if (cursor.cursorCount() == 0 || !cursors.first().hasSelection())
        return;

    const QTextCursor &firstCursor = cursors.first();
    const QString selection = firstCursor.selectedText();
    if (selection.contains(QChar::ParagraphSeparator))
        return;
    QTextDocument *document = firstCursor.document();

    if (Utils::anyOf(cursors, [selection = selection.toCaseFolded()](const QTextCursor &c) {
            return c.selectedText().toCaseFolded() != selection;
        })) {
        return;
    }

    const QTextDocument::FindFlags findFlags = textDocumentFlagsForFindFlags(m_findFlags);

    int searchFrom = cursors.last().selectionEnd();
    while (true) {
        QTextCursor next = document->find(selection, searchFrom, findFlags);
        if (next.isNull()) {
            QTC_ASSERT(searchFrom != 0, return);
            searchFrom = 0;
            continue;
        }
        if (next.selectionStart() == firstCursor.selectionStart())
            break;
        cursor.addCursor(next);
        q->setMultiTextCursor(cursor);
        break;
    }
}


void TextEditorWidgetPrivate::duplicateSelection(bool comment)
{
    if (comment && !m_commentDefinition.hasMultiLineStyle())
        return;

    MultiTextCursor cursor = q->multiTextCursor();
    cursor.beginEditBlock();
    for (QTextCursor &c : cursor) {
        if (c.hasSelection()) {
            // Cannot "duplicate and comment" files without multi-line comment

            QString dupText = c.selectedText().replace(QChar::ParagraphSeparator,
                                                            QLatin1Char('\n'));
            if (comment) {
                dupText = (m_commentDefinition.multiLineStart + dupText
                           + m_commentDefinition.multiLineEnd);
            }
            const int selStart = c.selectionStart();
            const int selEnd = c.selectionEnd();
            const bool cursorAtStart = (c.position() == selStart);
            c.setPosition(selEnd);
            c.insertText(dupText);
            c.setPosition(cursorAtStart ? selEnd : selStart);
            c.setPosition(cursorAtStart ? selStart : selEnd, QTextCursor::KeepAnchor);
        } else if (!m_cursors.hasMultipleCursors()) {
            const int curPos = c.position();
            const QTextBlock &block = c.block();
            QString dupText = block.text() + QLatin1Char('\n');
            if (comment && m_commentDefinition.hasSingleLineStyle())
                dupText.append(m_commentDefinition.singleLine);
            c.setPosition(block.position());
            c.insertText(dupText);
            c.setPosition(curPos);
        }
    }
    cursor.endEditBlock();
    q->setMultiTextCursor(cursor);
}


void TextEditorWidget::duplicateSelection()
{
    d->duplicateSelection(false);
}


void TextEditorWidget::addCursorsToLineEnds()
{
    d->addCursorsToLineEnds();
}


void TextEditorWidget::addSelectionNextFindMatch()
{
    d->addSelectionNextFindMatch();
}


void TextEditorWidget::duplicateSelectionAndComment()
{
    d->duplicateSelection(true);
}


void TextEditorWidget::deleteLine()
{
    d->maybeSelectLine();
    textCursor().removeSelectedText();
}


void TextEditorWidget::deleteEndOfLine()
{
    d->moveCursor(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    MultiTextCursor cursor = multiTextCursor();
    cursor.removeSelectedText();
    setMultiTextCursor(cursor);
}


void TextEditorWidget::deleteEndOfWord()
{
    d->moveCursor(QTextCursor::NextWord, QTextCursor::KeepAnchor);
    MultiTextCursor cursor = multiTextCursor();
    cursor.removeSelectedText();
    setMultiTextCursor(cursor);
}


void TextEditorWidget::deleteEndOfWordCamelCase()
{
    MultiTextCursor cursor = multiTextCursor();
    CamelCaseCursor::right(&cursor, this, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    setMultiTextCursor(cursor);
}


void TextEditorWidget::deleteStartOfLine()
{
    d->moveCursor(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
    MultiTextCursor cursor = multiTextCursor();
    cursor.removeSelectedText();
    setMultiTextCursor(cursor);
}


void TextEditorWidget::deleteStartOfWord()
{
    d->moveCursor(QTextCursor::PreviousWord, QTextCursor::KeepAnchor);
    MultiTextCursor cursor = multiTextCursor();
    cursor.removeSelectedText();
    setMultiTextCursor(cursor);
}


void TextEditorWidget::deleteStartOfWordCamelCase()
{
    MultiTextCursor cursor = multiTextCursor();
    CamelCaseCursor::left(&cursor, this, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    setMultiTextCursor(cursor);
}


void TextEditorWidget::autoIndent()
{
    MultiTextCursor cursor = multiTextCursor();
    cursor.beginEditBlock();
    // The order is important, since some indenter refer to previous indent positions.
    QList<QTextCursor> cursors = cursor.cursors();
    Utils::sort(cursors, [](const QTextCursor &lhs, const QTextCursor &rhs) {
        return lhs.selectionStart() < rhs.selectionStart();
    });
    for (const QTextCursor &c : cursors){
        d->m_document->autoFormatOrIndent(c);
    }

    cursor.mergeCursors();
    cursor.endEditBlock();
    setMultiTextCursor(cursor);
}


void TextEditorWidget::rewrapParagraph()
{
    const int paragraphWidth = marginSettings().m_marginColumn;
    const QRegularExpression anyLettersOrNumbers("\\w");
    const TabSettings ts = d->m_document->tabSettings();

    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    // Find start of paragraph.

    while (cursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor)) {
        QTextBlock block = cursor.block();
        QString text = block.text();

        // If this block is empty, move marker back to previous and terminate.
        if (!text.contains(anyLettersOrNumbers)) {
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
            break;
        }
    }

    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);

    // Find indent level of current block.
    const QString text = cursor.block().text();
    int indentLevel = ts.indentationColumn(text);

    // If there is a common prefix, it should be kept and expanded to all lines.
    // this allows nice reflowing of doxygen style comments.
    QTextCursor nextBlock = cursor;
    QString commonPrefix;

    const QString doxygenPrefix("^\\s*(?:///|/\\*\\*|/\\*\\!|\\*)?[ *]+");
    if (nextBlock.movePosition(QTextCursor::NextBlock))
    {
         QString nText = nextBlock.block().text();
         int maxLength = qMin(text.length(), nText.length());

         const auto hasDoxygenPrefix = [&] {
             static const QRegularExpression pattern(doxygenPrefix);
             return pattern.match(commonPrefix).hasMatch();
         };

         for (int i = 0; i < maxLength; ++i) {
             const QChar ch = text.at(i);

             if (ch != nText[i] || ch.isLetterOrNumber()
                     || ((ch == '@' || ch == '\\' ) && hasDoxygenPrefix())) {
                 break;
             }
             commonPrefix.append(ch);
         }
    }

    // Find end of paragraph.
    const QRegularExpression immovableDoxygenCommand(doxygenPrefix + "[@\\\\].*");
    QTC_CHECK(immovableDoxygenCommand.isValid());
    while (cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor)) {
        QString text = cursor.block().text();

        if (!text.contains(anyLettersOrNumbers) || immovableDoxygenCommand.match(text).hasMatch())
            break;
    }


    QString selectedText = cursor.selectedText();

    // Preserve initial indent level.or common prefix.
    QString spacing;

    if (commonPrefix.isEmpty()) {
        spacing = ts.indentationString(0, indentLevel, 0, textCursor().block());
    } else {
        spacing = commonPrefix;
        indentLevel = ts.columnCountForText(spacing);
    }

    int currentLength = indentLevel;
    QString result;
    result.append(spacing);

    // Remove existing instances of any common prefix from paragraph to
    // reflow.
    selectedText.remove(0, commonPrefix.length());
    commonPrefix.prepend(QChar::ParagraphSeparator);
    selectedText.replace(commonPrefix, QLatin1String("\n"));

    // remove any repeated spaces, trim lines to PARAGRAPH_WIDTH width and
    // keep the same indentation level as first line in paragraph.
    QString currentWord;

    for (const QChar &ch : std::as_const(selectedText)) {
        if (ch.isSpace() && ch != QChar::Nbsp) {
            if (!currentWord.isEmpty()) {
                currentLength += currentWord.length() + 1;

                if (currentLength > paragraphWidth) {
                    currentLength = currentWord.length() + 1 + indentLevel;
                    result.chop(1); // remove trailing space
                    result.append(QChar::ParagraphSeparator);
                    result.append(spacing);
                }

                result.append(currentWord);
                result.append(QLatin1Char(' '));
                currentWord.clear();
            }

            continue;
        }

        currentWord.append(ch);
    }
    result.chop(1);
    result.append(QChar::ParagraphSeparator);

    cursor.insertText(result);
    cursor.endEditBlock();
}


void TextEditorWidget::unCommentSelection()
{
    const bool singleLine = d->m_document->typingSettings().m_preferSingleLineComments;
    const MultiTextCursor cursor = Utils::unCommentSelection(multiTextCursor(),
                                                             d->m_commentDefinition,
                                                             singleLine);
    setMultiTextCursor(cursor);
}


void TextEditorWidget::autoFormat()
{
    QTextCursor cursor = textCursor();
    d->m_document->autoFormat(cursor);
}


void TextEditorWidget::encourageApply()
{
    if (!d->m_snippetOverlay->isVisible() || d->m_snippetOverlay->isEmpty())
        return;
    d->m_snippetOverlay->updateEquivalentSelections(textCursor());
}


void TextEditorWidgetPrivate::transformSelection(TransformationMethod method)
{
    MultiTextCursor cursor = m_cursors;
    cursor.beginEditBlock();
    for (QTextCursor &c : cursor) {
        int pos = c.position();
        int anchor = c.anchor();

        if (!c.hasSelection() && !m_cursors.hasMultipleCursors()) {
            // if nothing is selected, select the word under the cursor
            c.select(QTextCursor::WordUnderCursor);
        }

        QString text = c.selectedText();
        QString transformedText = method(text);

        if (transformedText == text)
            continue;

        c.insertText(transformedText);

        // (re)select the changed text
        // Note: this assumes the transformation did not change the length,
        c.setPosition(anchor);
        c.setPosition(pos, QTextCursor::KeepAnchor);
    }
    cursor.endEditBlock();
    q->setMultiTextCursor(cursor);
}


void TextEditorWidgetPrivate::transformSelectedLines(ListTransformationMethod method)
{
    if (!method || m_cursors.hasMultipleCursors())
        return;

    QTextCursor cursor = q->textCursor();
    if (!cursor.hasSelection())
        return;

    const bool downwardDirection = cursor.anchor() < cursor.position();
    int startPosition = cursor.selectionStart();
    int endPosition = cursor.selectionEnd();

    cursor.setPosition(startPosition);
    cursor.movePosition(QTextCursor::StartOfBlock);
    startPosition = cursor.position();

    cursor.setPosition(endPosition, QTextCursor::KeepAnchor);
    if (cursor.positionInBlock() == 0)
        cursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    endPosition = qMax(cursor.position(), endPosition);

    const QString text = cursor.selectedText();
    QStringList lines = text.split(QChar::ParagraphSeparator);
    method(lines);
    cursor.insertText(lines.join(QChar::ParagraphSeparator));

    // (re)select the changed lines
    // Note: this assumes the transformation did not change the length
    cursor.setPosition(downwardDirection ? startPosition : endPosition);
    cursor.setPosition(downwardDirection ? endPosition : startPosition, QTextCursor::KeepAnchor);
    q->setTextCursor(cursor);
}


QTextBlock TextEditorWidget::blockForVisibleRow(int row) const
{
    const int count = rowCount();
    if (row < 0 && row >= count)
        return QTextBlock();

    QTextBlock block = firstVisibleBlock();
    for (int i = 0; i < count;) {
        if (!block.isValid() || i >= row)
            return block;

        i += block.lineCount();
        block = d->nextVisibleBlock(block);
    }
    return QTextBlock();

}


QTextBlock TextEditorWidget::blockForVerticalOffset(int offset) const
{
    QTextBlock block = firstVisibleBlock();
    while (block.isValid()) {
        offset -= blockBoundingRect(block).height();
        if (offset < 0)
            return block;
        block = block.next();
    }
    return block;
}


int TextEditorWidget::blockNumberForVisibleRow(int row) const
{
    QTextBlock block = blockForVisibleRow(row);
    return block.isValid() ? block.blockNumber() : -1;
}


int TextEditorWidget::firstVisibleBlockNumber() const
{
    return blockNumberForVisibleRow(0);
}


int TextEditorWidget::lastVisibleBlockNumber() const
{
    QTextBlock block = blockForVerticalOffset(viewport()->height() - 1);
    if (!block.isValid()) {
        block = document()->lastBlock();
        while (block.isValid() && !block.isVisible())
            block = block.previous();
    }
    return block.isValid() ? block.blockNumber() : -1;
}


int TextEditorWidget::centerVisibleBlockNumber() const
{
    QTextBlock block = blockForVerticalOffset(viewport()->height() / 2);
    if (!block.isValid())
        block.previous();
    return block.isValid() ? block.blockNumber() : -1;
}


} // namespace TextEditor

