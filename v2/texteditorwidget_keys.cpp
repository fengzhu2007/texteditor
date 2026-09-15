// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
//
// v2 split of texteditor.cpp (legacy file kept at ../texteditor.cpp).
// Keyboard: key press handling, home/backspace handling, snippet insertion.
//

#include "texteditorwidget_p.h"

namespace TextEditor {

using namespace Internal;

bool TextEditorWidgetPrivate::cursorMoveKeyEvent(QKeyEvent *e)
{
    MultiTextCursor cursor = m_cursors;
    if (cursor.handleMoveKeyEvent(e, q, q->camelCaseNavigationEnabled())) {
        resetCursorFlashTimer();
        q->setMultiTextCursor(cursor);
        q->ensureCursorVisible();
        updateCurrentLineHighlight();
        return true;
    }
    return false;
}


static inline bool isModifier(QKeyEvent *e)
{
    if (!e)
        return false;
    switch (e->key()) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
        return true;
    default:
        return false;
    }
}


static inline bool isPrintableText(const QString &text)
{
    return !text.isEmpty() && (text.at(0).isPrint() || text.at(0) == QLatin1Char('\t'));
}


void TextEditorWidget::keyPressEvent(QKeyEvent *e)
{
    ICore::restartTrimmer();

    ExecuteOnDestruction eod([&]() { d->clearBlockSelection(); });

    if (!isModifier(e) && mouseHidingEnabled())
        viewport()->setCursor(Qt::BlankCursor);
    ToolTip::hide();

    d->m_moveLineUndoHack = false;
    d->clearVisibleFoldedBlock();

    MultiTextCursor cursor = multiTextCursor();

    if (e->key() == Qt::Key_Alt
            && d->m_behaviorSettings.m_keyboardTooltips) {
        d->m_maybeFakeTooltipEvent = true;
    } else {
        d->m_maybeFakeTooltipEvent = false;
        if (e->key() == Qt::Key_Escape ) {
            TextEditorWidgetFind::cancelCurrentSelectAll();
            if (d->m_suggestionBlock.isValid()) {
                d->clearCurrentSuggestion();
                e->accept();
                return;
            }
            if (d->m_snippetOverlay->isVisible()) {
                e->accept();
                d->m_snippetOverlay->accept();
                QTextCursor cursor = textCursor();
                cursor.clearSelection();
                setTextCursor(cursor);
                return;
            }
            if (cursor.hasMultipleCursors()) {
                QTextCursor c = cursor.mainCursor();
                c.setPosition(c.position(), QTextCursor::MoveAnchor);
                doSetTextCursor(c);
                return;
            }
        }
    }

    const bool ro = isReadOnly();
    const bool inOverwriteMode = overwriteMode();
    const bool hasMultipleCursors = cursor.hasMultipleCursors();


    if (TextSuggestion *suggestion = TextDocumentLayout::suggestion(d->m_suggestionBlock)) {
            if (e->matches(QKeySequence::MoveToNextWord)) {
                e->accept();
                if (suggestion->applyWord(this))
                    d->clearCurrentSuggestion();
                return;
            } else if (e->modifiers() == Qt::NoModifier
                       && (e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab)) {
                e->accept();
                if (suggestion->apply())
                    d->clearCurrentSuggestion();
                return;
            }
        }


    if (!ro
        && (e == QKeySequence::InsertParagraphSeparator
            || (!d->m_lineSeparatorsAllowed && e == QKeySequence::InsertLineSeparator))) {
        if (d->m_snippetOverlay->isVisible()) {
            e->accept();
            d->m_snippetOverlay->accept();
            QTextCursor cursor = textCursor();
            cursor.movePosition(QTextCursor::EndOfBlock);
            setTextCursor(cursor);
            return;
        }

        e->accept();
        cursor.beginEditBlock();
        for (QTextCursor &cursor : cursor) {
            const TabSettings ts = d->m_document->tabSettings();
            const TypingSettings &tps = d->m_document->typingSettings();

            int extraBlocks = d->m_autoCompleter->paragraphSeparatorAboutToBeInserted(cursor);

            QString previousIndentationString;
            if (tps.m_autoIndent) {
                cursor.insertBlock();
                d->m_document->autoIndent(cursor);
            } else {
                cursor.insertBlock();

                // After inserting the block, to avoid duplicating whitespace on the same line
                const QString &previousBlockText = cursor.block().previous().text();
                previousIndentationString = ts.indentationString(previousBlockText);
                if (!previousIndentationString.isEmpty())
                    cursor.insertText(previousIndentationString);
            }
            if (extraBlocks > 0) {
                const int cursorPosition = cursor.position();
                QTextCursor ensureVisible = cursor;
                while (extraBlocks > 0) {
                    --extraBlocks;
                    ensureVisible.movePosition(QTextCursor::NextBlock);
                    if (tps.m_autoIndent)
                        d->m_document->autoIndent(ensureVisible, QChar::Null, cursorPosition);
                    else if (!previousIndentationString.isEmpty())
                        ensureVisible.insertText(previousIndentationString);
                    if (d->m_animateAutoComplete || d->m_highlightAutoComplete) {
                        QTextCursor tc = ensureVisible;
                        tc.movePosition(QTextCursor::EndOfBlock);
                        tc.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
                        tc.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor);
                        d->autocompleterHighlight(tc);
                    }
                }
                cursor.setPosition(cursorPosition);
            }
        }
        cursor.endEditBlock();
        setMultiTextCursor(cursor);
        ensureCursorVisible();
        return;
    } else if (!ro
               && (e == QKeySequence::MoveToStartOfBlock || e == QKeySequence::SelectStartOfBlock
                   || e == QKeySequence::MoveToStartOfLine
                   || e == QKeySequence::SelectStartOfLine)) {
        const bool blockOp = e == QKeySequence::MoveToStartOfBlock || e == QKeySequence::SelectStartOfBlock;
        const bool select = e == QKeySequence::SelectStartOfLine || e == QKeySequence::SelectStartOfBlock;
        d->handleHomeKey(select, blockOp);
        e->accept();
        return;
    } else if (!ro && e == QKeySequence::DeleteStartOfWord) {
        e->accept();
        if (!cursor.hasSelection()) {
            if (camelCaseNavigationEnabled())
                CamelCaseCursor::left(&cursor, this, QTextCursor::KeepAnchor);
            else
                cursor.movePosition(QTextCursor::PreviousWord, QTextCursor::KeepAnchor);
        }
        cursor.removeSelectedText();
        setMultiTextCursor(cursor);
        return;
    } else if (!ro && e == QKeySequence::DeleteEndOfWord) {
        e->accept();
        if (!cursor.hasSelection()) {
            if (camelCaseNavigationEnabled())
                CamelCaseCursor::right(&cursor, this, QTextCursor::KeepAnchor);
            else
                cursor.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor);
        }
        cursor.removeSelectedText();
        setMultiTextCursor(cursor);
        return;
    } else if (!ro && e == QKeySequence::DeleteCompleteLine) {
        e->accept();
        for (QTextCursor &c : cursor)
            c.select(QTextCursor::BlockUnderCursor);
        cursor.mergeCursors();
        cursor.removeSelectedText();
        setMultiTextCursor(cursor);
        return;
    } else
        switch (e->key()) {
#if 0
    case Qt::Key_Dollar: {
            d->m_overlay->setVisible(!d->m_overlay->isVisible());
            d->m_overlay->setCursor(textCursor());
            e->accept();
        return;

    } break;
#endif
    case Qt::Key_Tab:
    case Qt::Key_Backtab: {
        if (ro) break;
        if (d->m_snippetOverlay->isVisible() && !d->m_snippetOverlay->isEmpty()) {
            d->snippetTabOrBacktab(e->key() == Qt::Key_Tab);
            e->accept();
            return;
        }
        QTextCursor cursor = textCursor();
        if (d->m_skipAutoCompletedText && e->key() == Qt::Key_Tab) {
            bool skippedAutoCompletedText = false;
            while (!d->m_autoCompleteHighlightPos.isEmpty()
                   && d->m_autoCompleteHighlightPos.last().selectionStart() == cursor.position()) {
                skippedAutoCompletedText = true;
                cursor.setPosition(d->m_autoCompleteHighlightPos.last().selectionEnd());
                d->m_autoCompleteHighlightPos.pop_back();
            }
            if (skippedAutoCompletedText) {
                setTextCursor(cursor);
                e->accept();
                d->updateAutoCompleteHighlight();
                return;
            }
        }
        int newPosition;
        if (!hasMultipleCursors
            && d->m_document->typingSettings().tabShouldIndent(document(), cursor, &newPosition)) {
            if (newPosition != cursor.position() && !cursor.hasSelection()) {
                cursor.setPosition(newPosition);
                setTextCursor(cursor);
            }
            d->m_document->autoIndent(cursor);
        } else {
            if (e->key() == Qt::Key_Tab)
                indent();
            else
                unindent();
        }
        e->accept();
        return;
    } break;
    case Qt::Key_Backspace:
        if (ro) break;
        if ((e->modifiers() & (Qt::ControlModifier
                               | Qt::ShiftModifier
                               | Qt::AltModifier
                               | Qt::MetaModifier)) == Qt::NoModifier) {
            e->accept();
            if (cursor.hasSelection()) {
                cursor.removeSelectedText();
                setMultiTextCursor(cursor);
            }else{
                d->handleBackspaceKey();
            }
            ensureCursorVisible();

            return;
        }
        break;
    case Qt::Key_Insert:
        if (ro) break;
        if (e->modifiers() == Qt::NoModifier) {
            setOverwriteMode(!inOverwriteMode);
            e->accept();
            return;
        }
        break;
    case Qt::Key_Delete:
        if (hasMultipleCursors && !ro
            && (e->modifiers() == Qt::NoModifier || e->modifiers() == Qt::KeypadModifier)) {
            if (cursor.hasSelection()) {
                cursor.removeSelectedText();
            } else {
                cursor.beginEditBlock();
                for (QTextCursor c : cursor)
                    c.deleteChar();
                cursor.mergeCursors();
                cursor.endEditBlock();
            }
            e->accept();
            return;
        }
        break;
    default:
        break;
    }

    const QString eventText = e->text();

    if (e->key() == Qt::Key_H
            && e->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
        d->universalHelper();
        e->accept();
        return;
    }

    if (ro || !isPrintableText(eventText)) {
        QTextCursor::MoveOperation blockSelectionOperation = QTextCursor::NoMove;
        if (e->modifiers() == (Qt::AltModifier | Qt::ShiftModifier) && !Utils::HostOsInfo::isMacHost()) {
            if (MultiTextCursor::multiCursorAddEvent(e, QKeySequence::MoveToNextLine))
                blockSelectionOperation = QTextCursor::Down;
            else if (MultiTextCursor::multiCursorAddEvent(e, QKeySequence::MoveToPreviousLine))
                blockSelectionOperation = QTextCursor::Up;
            else if (MultiTextCursor::multiCursorAddEvent(e, QKeySequence::MoveToNextChar))
                blockSelectionOperation = QTextCursor::NextCharacter;
            else if (MultiTextCursor::multiCursorAddEvent(e, QKeySequence::MoveToPreviousChar))
                blockSelectionOperation = QTextCursor::PreviousCharacter;
        }

        if (blockSelectionOperation != QTextCursor::NoMove) {
            auto doNothing = [](){};
            eod.reset(doNothing);
            d->handleMoveBlockSelection(blockSelectionOperation);
        } else if (!d->cursorMoveKeyEvent(e)) {
            QTextCursor cursor = textCursor();
            bool cursorWithinSnippet = false;
            if (d->m_snippetOverlay->isVisible()
                && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)) {
                cursorWithinSnippet = d->snippetCheckCursor(cursor);
            }
            if (cursorWithinSnippet)
                cursor.beginEditBlock();

            QPlainTextEdit::keyPressEvent(e);

            if (cursorWithinSnippet) {
                cursor.endEditBlock();
                d->m_snippetOverlay->updateEquivalentSelections(textCursor());
            }
        }
    } else if (hasMultipleCursors) {
        if (inOverwriteMode) {
            cursor.beginEditBlock();
            for (QTextCursor &c : cursor) {
                QTextBlock block = c.block();
                int eolPos = block.position() + block.length() - 1;
                int selEndPos = qMin(c.position() + eventText.length(), eolPos);
                c.setPosition(selEndPos, QTextCursor::KeepAnchor);
                c.insertText(eventText);
            }
            cursor.endEditBlock();
        } else {
            cursor.insertText(eventText);
        }
        setMultiTextCursor(cursor);
    } else if ((e->modifiers() & (Qt::ControlModifier|Qt::AltModifier)) != Qt::ControlModifier){
        // only go here if control is not pressed, except if also alt is pressed
        // because AltGr maps to Alt + Ctrl
        QTextCursor cursor = textCursor();
        QString autoText;
        int adjustPos = 0;
        if (!inOverwriteMode) {
            const bool skipChar = d->m_skipAutoCompletedText && !d->m_autoCompleteHighlightPos.isEmpty() && cursor == d->m_autoCompleteHighlightPos.last();
            autoText = autoCompleter()->autoComplete(cursor, eventText, skipChar,&adjustPos);
        }
        const bool cursorWithinSnippet = d->snippetCheckCursor(cursor);

        QChar electricChar;
        if (d->m_document->typingSettings().m_autoIndent) {
            for (const QChar c : eventText) {
                if (d->m_document->indenter()->isElectricCharacter(c)) {
                    electricChar = c;
                    break;
                }
            }
        }

        bool doEditBlock = !electricChar.isNull() || !autoText.isEmpty() || cursorWithinSnippet;
        if (doEditBlock)
            cursor.beginEditBlock();

        if (inOverwriteMode) {
            if (!doEditBlock)
                cursor.beginEditBlock();
            QTextBlock block = cursor.block();
            int eolPos = block.position() + block.length() - 1;
            int selEndPos = qMin(cursor.position() + eventText.length(), eolPos);
            cursor.setPosition(selEndPos, QTextCursor::KeepAnchor);
            cursor.insertText(eventText);
            if (!doEditBlock)
                cursor.endEditBlock();
        } else {
            cursor.insertText(eventText);
        }

        if (!autoText.isEmpty()) {
            int pos = cursor.position();
            cursor.insertText(autoText);
            cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
            d->autocompleterHighlight(cursor);
            //Select the inserted text, to be able to re-indent the inserted text
            cursor.setPosition(pos, QTextCursor::KeepAnchor);
        }
        if (!electricChar.isNull() && d->m_autoCompleter->contextAllowsElectricCharacters(cursor))
            d->m_document->autoIndent(cursor, electricChar, cursor.position());
        if (!autoText.isEmpty())
            cursor.setPosition(cursor.position() + adjustPos);

        if (doEditBlock) {
            cursor.endEditBlock();
            if (cursorWithinSnippet)
                d->m_snippetOverlay->updateEquivalentSelections(textCursor());
        }

        setTextCursor(cursor);
    }

    if (!ro && e->key() == Qt::Key_Delete && d->m_parenthesesMatchingEnabled)
        d->m_parenthesesMatchingTimer.start();

    if (!ro && d->m_contentsChanged && isPrintableText(eventText) && !inOverwriteMode){
        d->m_codeAssistant.process();

    }
}


class PositionedPart : public ParsedSnippet::Part
{

public:
    explicit PositionedPart(const ParsedSnippet::Part &part) : ParsedSnippet::Part(part) {}
    int start;
    int end;
};


class CursorPart : public ParsedSnippet::Part
{

public:
    CursorPart(const PositionedPart &part, QTextDocument *doc)
        : ParsedSnippet::Part(part)
        , cursor(doc)
    {
        cursor.setPosition(part.start);
        cursor.setPosition(part.end, QTextCursor::KeepAnchor);
    }
    QTextCursor cursor;
};


void TextEditorWidget::insertCodeSnippet(const QTextCursor &cursor_arg,
                                         const QString &snippet,
                                         const SnippetParser &parse)
{
    SnippetParseResult result = parse(snippet);
    if (std::holds_alternative<SnippetParseError>(result)) {
        const auto &error = std::get<SnippetParseError>(result);
        QMessageBox::warning(this, tr("Snippet Parse Error"), error.htmlMessage());
        return;
    }
    QTC_ASSERT(std::holds_alternative<ParsedSnippet>(result), return);
    ParsedSnippet data = std::get<ParsedSnippet>(result);

    QTextCursor cursor = cursor_arg;
    cursor.beginEditBlock();
    cursor.removeSelectedText();
    const int startCursorPosition = cursor.position();

    d->m_snippetOverlay->accept();

    QList<PositionedPart> positionedParts;
    for (const ParsedSnippet::Part &part : std::as_const(data.parts)) {
        if (part.variableIndex >= 0) {
            PositionedPart posPart(part);
            posPart.start = cursor.position();
            cursor.insertText(part.text);
            posPart.end = cursor.position();
            positionedParts << posPart;
        } else {
            cursor.insertText(part.text);
        }
    }

    QList<CursorPart> cursorParts = Utils::transform(positionedParts,
                                                     [doc = document()](const PositionedPart &part) {
                                                         return CursorPart(part, doc);
                                                     });

    cursor.setPosition(startCursorPosition, QTextCursor::KeepAnchor);
    d->m_document->autoIndent(cursor);
    cursor.endEditBlock();

    const QColor &occurrencesColor
        = textDocument()->fontSettings().toTextCharFormat(C_OCCURRENCES).background().color();
    const QColor &renameColor
        = textDocument()->fontSettings().toTextCharFormat(C_OCCURRENCES_RENAME).background().color();

    for (const CursorPart &part : cursorParts) {
        const QColor &color = part.cursor.hasSelection() ? occurrencesColor : renameColor;
        if (part.finalPart) {
            d->m_snippetOverlay->setFinalSelection(part.cursor, color);
        } else {
            d->m_snippetOverlay->addSnippetSelection(part.cursor,
                                                     color,
                                                     part.mangler,
                                                     part.variableIndex);
        }
    }

    cursor = d->m_snippetOverlay->firstSelectionCursor();
    if (!cursor.isNull()) {
        setTextCursor(cursor);
        if (d->m_snippetOverlay->isFinalSelection(cursor))
            d->m_snippetOverlay->accept();
        else
            d->m_snippetOverlay->setVisible(true);
    }
}


void TextEditorWidgetPrivate::universalHelper()
{
    // Test function for development. Place your new fangled experiment here to
    // give it proper scrutiny before pushing it onto others.
}


bool TextEditorWidgetPrivate::snippetCheckCursor(const QTextCursor &cursor)
{
    if (!m_snippetOverlay->isVisible() || m_snippetOverlay->isEmpty())
        return false;

    QTextCursor start = cursor;
    start.setPosition(cursor.selectionStart());
    QTextCursor end = cursor;
    end.setPosition(cursor.selectionEnd());
    if (!m_snippetOverlay->hasCursorInSelection(start)
        || !m_snippetOverlay->hasCursorInSelection(end)
        || m_snippetOverlay->hasFirstSelectionBeginMoved()) {
        m_snippetOverlay->accept();
        return false;
    }
    return true;
}


void TextEditorWidgetPrivate::snippetTabOrBacktab(bool forward)
{
    if (!m_snippetOverlay->isVisible() || m_snippetOverlay->isEmpty())
        return;
    QTextCursor cursor = forward ? m_snippetOverlay->nextSelectionCursor(q->textCursor())
                                 : m_snippetOverlay->previousSelectionCursor(q->textCursor());
    q->setTextCursor(cursor);
    if (m_snippetOverlay->isFinalSelection(cursor))
        m_snippetOverlay->accept();
}

// Calculate global position for a tooltip considering the left extra area.

void TextEditorWidgetPrivate::handleHomeKey(bool anchor, bool block)
{
    const QTextCursor::MoveMode mode = anchor ? QTextCursor::KeepAnchor
                                              : QTextCursor::MoveAnchor;

    MultiTextCursor cursor = q->multiTextCursor();
    for (QTextCursor &c : cursor) {
        const int initpos = c.position();
        int pos = c.block().position();

        if (!block) {
            // only go to the first non space if we are in the first line of the layout
            if (QTextLayout *layout = c.block().layout();
                layout->lineForTextPosition(initpos - pos).lineNumber() != 0) {
                c.movePosition(QTextCursor::StartOfLine, mode);
                continue;
            }
        }

        QChar character = q->document()->characterAt(pos);
        const QLatin1Char tab = QLatin1Char('\t');

        while (character == tab || character.category() == QChar::Separator_Space) {
            ++pos;
            if (pos == initpos)
                break;
            character = q->document()->characterAt(pos);
        }

        // Go to the start of the block when we're already at the start of the text
        if (pos == initpos)
            pos = c.block().position();

        c.setPosition(pos, mode);
    }
    q->setMultiTextCursor(cursor);
}


void TextEditorWidgetPrivate::handleBackspaceKey()
{
    QTC_ASSERT(!q->multiTextCursor().hasSelection(), return);
    MultiTextCursor cursor = m_cursors;
    cursor.beginEditBlock();
    for (QTextCursor &c : cursor) {
        const int pos = c.position();
        if (!pos)
            continue;

        bool cursorWithinSnippet = false;
        if (m_snippetOverlay->isVisible()) {
            QTextCursor snippetCursor = c;
            snippetCursor.movePosition(QTextCursor::Left);
            cursorWithinSnippet = snippetCheckCursor(snippetCursor);
        }

        const TabSettings tabSettings = m_document->tabSettings();
        const TypingSettings &typingSettings = m_document->typingSettings();

        if (typingSettings.m_autoIndent && !m_autoCompleteHighlightPos.isEmpty()
            && (m_autoCompleteHighlightPos.last() == c) && m_removeAutoCompletedText
            && m_autoCompleter->autoBackspace(c)) {
            continue;
        }

        bool handled = false;
        if (typingSettings.m_smartBackspaceBehavior == TypingSettings::BackspaceNeverIndents) {
            if (cursorWithinSnippet)
                c.beginEditBlock();
            c.deletePreviousChar();
            handled = true;
        } else if (typingSettings.m_smartBackspaceBehavior
                   == TypingSettings::BackspaceFollowsPreviousIndents) {
            QTextBlock currentBlock = c.block();
            int positionInBlock = pos - currentBlock.position();
            const QString blockText = currentBlock.text();
            if (c.atBlockStart() || TabSettings::firstNonSpace(blockText) < positionInBlock) {
                if (cursorWithinSnippet)
                    c.beginEditBlock();
                c.deletePreviousChar();
                handled = true;
            } else {
                if (cursorWithinSnippet)
                    m_snippetOverlay->accept();
                cursorWithinSnippet = false;
                int previousIndent = 0;
                const int indent = tabSettings.columnAt(blockText, positionInBlock);
                for (QTextBlock previousNonEmptyBlock = currentBlock.previous();
                     previousNonEmptyBlock.isValid();
                     previousNonEmptyBlock = previousNonEmptyBlock.previous()) {
                    QString previousNonEmptyBlockText = previousNonEmptyBlock.text();
                    if (previousNonEmptyBlockText.trimmed().isEmpty())
                        continue;
                    previousIndent = tabSettings.columnAt(previousNonEmptyBlockText,
                                                          TabSettings::firstNonSpace(
                                                              previousNonEmptyBlockText));
                    if (previousIndent < indent) {
                        c.beginEditBlock();
                        c.setPosition(currentBlock.position(), QTextCursor::KeepAnchor);
                        c.insertText(tabSettings.indentationString(previousNonEmptyBlockText));
                        c.endEditBlock();
                        handled = true;
                        break;
                    }
                }
            }
        } else if (typingSettings.m_smartBackspaceBehavior == TypingSettings::BackspaceUnindents) {
            const QChar previousChar = q->document()->characterAt(pos - 1);
            if (!(previousChar == QLatin1Char(' ') || previousChar == QLatin1Char('\t'))) {
                if (cursorWithinSnippet)
                    c.beginEditBlock();
                c.deletePreviousChar();
            } else {
                if (cursorWithinSnippet)
                    m_snippetOverlay->accept();
                cursorWithinSnippet = false;
                q->unindent();
            }
            handled = true;
        }

        if (!handled) {
            if (cursorWithinSnippet)
                c.beginEditBlock();
            c.deletePreviousChar();
        }

        if (cursorWithinSnippet) {
            c.endEditBlock();
            m_snippetOverlay->updateEquivalentSelections(c);
        }
    }
    cursor.endEditBlock();
    q->setMultiTextCursor(cursor);
}


void TextEditorWidget::inSnippetMode(bool *active)
{
    *active = d->m_snippetOverlay->isVisible();
}


} // namespace TextEditor

