// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
//
// v2 split of texteditor.cpp (legacy file kept at ../texteditor.cpp).
// Highlighting: parentheses matching, block highlighting, auto-complete highlight, current line highlight, cursor flash.
//

#include "texteditorwidget_p.h"

namespace TextEditor {

using namespace Internal;

int TextEditorWidgetPrivate::visualIndent(const QTextBlock &block) const
{
    if (!block.isValid())
        return 0;
    const QTextDocument *document = block.document();
    int i = 0;
    while (i < block.length()) {
        if (!document->characterAt(block.position() + i).isSpace()) {
            QTextCursor cursor(block);
            cursor.setPosition(block.position() + i);
            return q->cursorRect(cursor).x();
        }
        ++i;
    }

    return 0;
}


void TextEditorWidgetPrivate::updateAutoCompleteHighlight()
{
    const QTextCharFormat matchFormat = m_document->fontSettings().toTextCharFormat(C_AUTOCOMPLETE);

    QList<QTextEdit::ExtraSelection> extraSelections;
    for (const QTextCursor &cursor : std::as_const(m_autoCompleteHighlightPos)) {
        QTextEdit::ExtraSelection sel;
        sel.cursor = cursor;
        sel.format.setBackground(matchFormat.background());
        extraSelections.append(sel);
    }
    q->setExtraSelections(TextEditorWidget::AutoCompleteSelection, extraSelections);
}


void TextEditorWidget::setVisualIndentOffset(int offset)
{
    d->m_visualIndentOffset = qMax(0, offset);
}


void TextEditorWidgetPrivate::startCursorFlashTimer()
{
    const int flashTime = QApplication::cursorFlashTime();
    if (flashTime > 0) {
        m_cursorFlashTimer.stop();
        m_cursorFlashTimer.start(flashTime / 2, q);
    }
    if (!m_cursorVisible) {
        m_cursorVisible = true;
        q->viewport()->update(cursorUpdateRect(m_cursors));
    }
}


void TextEditorWidgetPrivate::resetCursorFlashTimer()
{
    if (!m_cursorFlashTimer.isActive())
        return;
    const int flashTime = QApplication::cursorFlashTime();
    if (flashTime > 0) {
        m_cursorFlashTimer.stop();
        m_cursorFlashTimer.start(flashTime / 2, q);
    }
    if (!m_cursorVisible) {
        m_cursorVisible = true;
        q->viewport()->update(cursorUpdateRect(m_cursors));
    }
}


void TextEditorWidgetPrivate::updateCursorSelections()
{
    const QTextCharFormat selectionFormat = TextEditorSettings::fontSettings().toTextCharFormat(
        C_SELECTION);
    QList<QTextEdit::ExtraSelection> selections;
    for (const QTextCursor &cursor : m_cursors) {
        if (cursor.hasSelection())
            selections << QTextEdit::ExtraSelection{cursor, selectionFormat};
    }
    q->setExtraSelections(TextEditorWidget::CursorSelection, selections);


    m_selectionHighlightOverlay->clear();

    this->highlightSelectionResultsInScrollBar();


    /*m_selectionResults.clear();
    if (!m_highlightScrollBarController)
        return;
    m_highlightScrollBarController->removeHighlights(Constants::SCROLL_BAR_SELECTION);

    if (!m_displaySettings.m_highlightSelection || m_cursors.hasMultipleCursors())
        return;

    const QString txt = m_cursors.selectedText();
    if (txt.trimmed().isEmpty())
        return;*/





}


QRect TextEditorWidgetPrivate::cursorUpdateRect(const MultiTextCursor &cursor)
{
    QRect result(0, 0, 0, 0);
    for (const QTextCursor &c : cursor)
        result |= q->cursorRect(c);
    return result;
}


void TextEditorWidgetPrivate::updateCurrentLineHighlight()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (m_highlightCurrentLine) {
        for (const QTextCursor &c : m_cursors) {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(
                        m_document->fontSettings().toTextCharFormat(C_CURRENT_LINE).background());
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            sel.cursor = c;
            sel.cursor.clearSelection();
            extraSelections.append(sel);
        }
    }
    updateCurrentLineInScrollbar();

    q->setExtraSelections(TextEditorWidget::CurrentLineSelection, extraSelections);

    // the extra area shows information for the entire current block, not just the currentline.
    // This is why we must force a bigger update region.
    const QPointF offset = q->contentOffset();
    auto updateBlock = [&](const QTextBlock &block) {
        if (block.isValid() && block.isVisible()) {
            QRect updateRect = q->blockBoundingGeometry(block).translated(offset).toAlignedRect();
            m_extraArea->update(updateRect);
            updateRect.setLeft(0);
            updateRect.setRight(q->viewport()->width());
            q->viewport()->update(updateRect);
        }
    };
    QList<int> cursorBlockNumbers;
    for (const QTextCursor &c : m_cursors) {
        int cursorBlockNumber = c.blockNumber();
        if (!m_cursorBlockNumbers.contains(cursorBlockNumber))
            updateBlock(c.block());
        if (!cursorBlockNumbers.contains(c.blockNumber()))
            cursorBlockNumbers << c.blockNumber();
    }
    if (m_cursorBlockNumbers != cursorBlockNumbers) {
        for (int oldBlock : m_cursorBlockNumbers) {
            if (!cursorBlockNumbers.contains(oldBlock))
                updateBlock(m_document->document()->findBlockByNumber(oldBlock));
        }
        m_cursorBlockNumbers = cursorBlockNumbers;
    }
}


void TextEditorWidget::slotCursorPositionChanged()
{
#if 0
    qDebug() << "block" << textCursor().blockNumber()+1
            << "brace depth:" << BaseTextDocumentLayout::braceDepth(textCursor().block())
            << "indent:" << BaseTextDocumentLayout::userData(textCursor().block())->foldingIndent();
#endif
    /*if (!d->m_contentsChanged && d->m_lastCursorChangeWasInteresting) {
        if (EditorManager::currentEditor() && EditorManager::currentEditor()->widget() == this)
            EditorManager::addCurrentPositionToNavigationHistory(d->m_tempNavigationState);
        d->m_lastCursorChangeWasInteresting = false;
    } else if (d->m_contentsChanged) {
        d->saveCurrentCursorPositionForNavigation();
        if (EditorManager::currentEditor() && EditorManager::currentEditor()->widget() == this)
            EditorManager::setLastEditLocation(EditorManager::currentEditor());
    }*/
    MultiTextCursor cursor = multiTextCursor();
    cursor.replaceMainCursor(textCursor());
    setMultiTextCursor(cursor);
    d->updateCursorSelections();
    d->updateHighlights();
}


void TextEditorWidgetPrivate::updateHighlights()
{
    if (m_parenthesesMatchingEnabled && q->hasFocus()) {
        // Delay update when no matching is displayed yet, to avoid flicker
        if (q->extraSelections(TextEditorWidget::ParenthesesMatchingSelection).isEmpty()
            && m_bracketsAnimator == nullptr) {
            m_parenthesesMatchingTimer.start();
        } else {
            // when we uncheck "highlight matching parentheses"
            // we need clear current selection before viewport update
            // otherwise we get sticky highlighted parentheses
            if (!m_displaySettings.m_highlightMatchingParentheses)
                q->setExtraSelections(TextEditorWidget::ParenthesesMatchingSelection, QList<QTextEdit::ExtraSelection>());

            // use 0-timer, not direct call, to give the syntax highlighter a chance
            // to update the parentheses information
            m_parenthesesMatchingTimer.start(0);
        }
    }

    if (m_highlightAutoComplete && !m_autoCompleteHighlightPos.isEmpty()) {
        QMetaObject::invokeMethod(this, [this] {
            const QTextCursor &cursor = q->textCursor();
            auto popAutoCompletion = [&]() {
                return !m_autoCompleteHighlightPos.isEmpty()
                        && m_autoCompleteHighlightPos.last() != cursor;
            };
            if ((!m_keepAutoCompletionHighlight && !q->hasFocus()) || popAutoCompletion()) {
                while (popAutoCompletion())
                    m_autoCompleteHighlightPos.pop_back();
                updateAutoCompleteHighlight();
            }
        }, Qt::QueuedConnection);
    }

    updateCurrentLineHighlight();

    if (m_displaySettings.m_highlightBlocks) {
        QTextCursor cursor = q->textCursor();
        extraAreaHighlightFoldedBlockNumber = cursor.blockNumber();
        m_highlightBlocksTimer.start(100);
    }
}


void TextEditorWidgetPrivate::_q_matchParentheses()
{
    if (q->isReadOnly()
        || !(m_displaySettings.m_highlightMatchingParentheses
             || m_displaySettings.m_animateMatchingParentheses))
        return;

    QTextCursor backwardMatch = q->textCursor();
    QTextCursor forwardMatch = q->textCursor();
    if (q->overwriteMode())
        backwardMatch.movePosition(QTextCursor::Right);
    const TextBlockUserData::MatchType backwardMatchType = TextBlockUserData::matchCursorBackward(&backwardMatch);
    const TextBlockUserData::MatchType forwardMatchType = TextBlockUserData::matchCursorForward(&forwardMatch);

    QList<QTextEdit::ExtraSelection> extraSelections;

    if (backwardMatchType == TextBlockUserData::NoMatch && forwardMatchType == TextBlockUserData::NoMatch) {
        q->setExtraSelections(TextEditorWidget::ParenthesesMatchingSelection, extraSelections); // clear
        return;
    }

    const QTextCharFormat matchFormat = m_document->fontSettings().toTextCharFormat(C_PARENTHESES);
    const QTextCharFormat mismatchFormat = m_document->fontSettings().toTextCharFormat(
        C_PARENTHESES_MISMATCH);
    int animatePosition = -1;
    if (backwardMatch.hasSelection()) {
        QTextEdit::ExtraSelection sel;
        if (backwardMatchType == TextBlockUserData::Mismatch) {
            sel.cursor = backwardMatch;
            sel.format = mismatchFormat;
            extraSelections.append(sel);
        } else {

            sel.cursor = backwardMatch;
            sel.format = matchFormat;

            sel.cursor.setPosition(backwardMatch.selectionStart());
            sel.cursor.setPosition(sel.cursor.position() + 1, QTextCursor::KeepAnchor);
            extraSelections.append(sel);

            if (m_displaySettings.m_animateMatchingParentheses && sel.cursor.block().isVisible())
                animatePosition = backwardMatch.selectionStart();

            sel.cursor.setPosition(backwardMatch.selectionEnd());
            sel.cursor.setPosition(sel.cursor.position() - 1, QTextCursor::KeepAnchor);
            extraSelections.append(sel);
        }
    }

    if (forwardMatch.hasSelection()) {
        QTextEdit::ExtraSelection sel;
        if (forwardMatchType == TextBlockUserData::Mismatch) {
            sel.cursor = forwardMatch;
            sel.format = mismatchFormat;
            extraSelections.append(sel);
        } else {

            sel.cursor = forwardMatch;
            sel.format = matchFormat;

            sel.cursor.setPosition(forwardMatch.selectionStart());
            sel.cursor.setPosition(sel.cursor.position() + 1, QTextCursor::KeepAnchor);
            extraSelections.append(sel);

            sel.cursor.setPosition(forwardMatch.selectionEnd());
            sel.cursor.setPosition(sel.cursor.position() - 1, QTextCursor::KeepAnchor);
            extraSelections.append(sel);

            if (m_displaySettings.m_animateMatchingParentheses && sel.cursor.block().isVisible())
                animatePosition = forwardMatch.selectionEnd() - 1;
        }
    }


    if (animatePosition >= 0) {
        const QList<QTextEdit::ExtraSelection> selections = q->extraSelections(
            TextEditorWidget::ParenthesesMatchingSelection);
        for (const QTextEdit::ExtraSelection &sel : selections) {
            if (sel.cursor.selectionStart() == animatePosition
                || sel.cursor.selectionEnd() - 1 == animatePosition) {
                animatePosition = -1;
                break;
            }
        }
    }

    if (animatePosition >= 0) {
        cancelCurrentAnimations();// one animation is enough
        QPalette pal;
        pal.setBrush(QPalette::Text, matchFormat.foreground());
        pal.setBrush(QPalette::Base, matchFormat.background());
        QTextCursor cursor = q->textCursor();
        cursor.setPosition(animatePosition + 1);
        cursor.setPosition(animatePosition, QTextCursor::KeepAnchor);
        m_bracketsAnimator = new TextEditorAnimator(this);
        m_bracketsAnimator->init(cursor, q->font(), pal);
        connect(m_bracketsAnimator.data(), &TextEditorAnimator::updateRequest,
                this, &TextEditorWidgetPrivate::_q_animateUpdate);
    }
    if (m_displaySettings.m_highlightMatchingParentheses)
        q->setExtraSelections(TextEditorWidget::ParenthesesMatchingSelection, extraSelections);
}


void TextEditorWidgetPrivate::_q_highlightBlocks()
{
    TextEditorPrivateHighlightBlocks highlightBlocksInfo;

    QTextBlock block;
    if (extraAreaHighlightFoldedBlockNumber >= 0) {
        block = q->document()->findBlockByNumber(extraAreaHighlightFoldedBlockNumber);
        if (block.isValid()
            && block.next().isValid()
            && TextDocumentLayout::foldingIndent(block.next())
            > TextDocumentLayout::foldingIndent(block))
            block = block.next();
    }

    QTextBlock closeBlock = block;
    while (block.isValid()) {
        int foldingIndent = TextDocumentLayout::foldingIndent(block);

        while (block.previous().isValid() && TextDocumentLayout::foldingIndent(block) >= foldingIndent)
            block = block.previous();
        int nextIndent = TextDocumentLayout::foldingIndent(block);
        if (nextIndent == foldingIndent)
            break;
        highlightBlocksInfo.open.prepend(block.blockNumber());
        while (closeBlock.next().isValid()
            && TextDocumentLayout::foldingIndent(closeBlock.next()) >= foldingIndent )
            closeBlock = closeBlock.next();
        highlightBlocksInfo.close.append(closeBlock.blockNumber());
        int indent = qMin(visualIndent(block), visualIndent(closeBlock));
        highlightBlocksInfo.visualIndent.prepend(indent);
    }

#if 0
    if (block.isValid()) {
        QTextCursor cursor(block);
        if (extraAreaHighlightCollapseColumn >= 0)
            cursor.setPosition(cursor.position() + qMin(extraAreaHighlightCollapseColumn,
                                                        block.length()-1));
        QTextCursor closeCursor;
        bool firstRun = true;
        while (TextBlockUserData::findPreviousBlockOpenParenthesis(&cursor, firstRun)) {
            firstRun = false;
            highlightBlocksInfo.open.prepend(cursor.blockNumber());
            int visualIndent = visualIndent(cursor.block());
            if (closeCursor.isNull())
                closeCursor = cursor;
            if (TextBlockUserData::findNextBlockClosingParenthesis(&closeCursor)) {
                highlightBlocksInfo.close.append(closeCursor.blockNumber());
                visualIndent = qMin(visualIndent, visualIndent(closeCursor.block()));
            }
            highlightBlocksInfo.visualIndent.prepend(visualIndent);
        }
    }
#endif
    if (m_highlightBlocksInfo != highlightBlocksInfo) {
        m_highlightBlocksInfo = highlightBlocksInfo;
        q->viewport()->update();
        m_extraArea->update();
    }
}


void TextEditorWidgetPrivate::autocompleterHighlight(const QTextCursor &cursor)
{
    if ((!m_animateAutoComplete && !m_highlightAutoComplete)
            || q->isReadOnly() || !cursor.hasSelection()) {
        m_autoCompleteHighlightPos.clear();
    } else if (m_highlightAutoComplete) {
        m_autoCompleteHighlightPos.push_back(cursor);
    }
    if (m_animateAutoComplete) {
        const QTextCharFormat matchFormat = m_document->fontSettings().toTextCharFormat(
            C_AUTOCOMPLETE);
        cancelCurrentAnimations();// one animation is enough
        QPalette pal;
        pal.setBrush(QPalette::Text, matchFormat.foreground());
        pal.setBrush(QPalette::Base, matchFormat.background());
        m_autocompleteAnimator = new TextEditorAnimator(this);
        m_autocompleteAnimator->init(cursor, q->font(), pal);
        connect(m_autocompleteAnimator.data(), &TextEditorAnimator::updateRequest,
                this, &TextEditorWidgetPrivate::_q_animateUpdate);
    }
    updateAutoCompleteHighlight();
}


void TextEditorWidgetPrivate::setExtraSelections(Id kind, const QList<QTextEdit::ExtraSelection> &selections)
{
    if (selections.isEmpty() && m_extraSelections[kind].isEmpty())
        return;
    m_extraSelections[kind] = selections;

    if (kind == TextEditorWidget::CodeSemanticsSelection) {
        m_overlay->clear();
        for (const QTextEdit::ExtraSelection &selection : selections) {
            m_overlay->addOverlaySelection(selection.cursor,
                                              selection.format.background().color(),
                                              selection.format.background().color(),
                                              TextEditorOverlay::LockSize);
        }
        m_overlay->setVisible(!m_overlay->isEmpty());
    } else {
        QList<QTextEdit::ExtraSelection> all;
        for (auto i = m_extraSelections.constBegin(); i != m_extraSelections.constEnd(); ++i) {
            if (i.key() == TextEditorWidget::CodeSemanticsSelection
                || i.key() == TextEditorWidget::SnippetPlaceholderSelection)
                continue;
            all += i.value();
        }
        q->QPlainTextEdit::setExtraSelections(all);
    }
}


void TextEditorWidget::setExtraSelections(Id kind, const QList<QTextEdit::ExtraSelection> &selections)
{
    d->setExtraSelections(kind, selections);
}


QList<QTextEdit::ExtraSelection> TextEditorWidget::extraSelections(Id kind) const
{
    return d->m_extraSelections.value(kind);
}


QString TextEditorWidget::extraSelectionTooltip(int pos) const
{
    for (const QList<QTextEdit::ExtraSelection> &sel : std::as_const(d->m_extraSelections)) {
        for (const QTextEdit::ExtraSelection &s : sel) {
            if (s.cursor.selectionStart() <= pos
                && s.cursor.selectionEnd() >= pos
                && !s.format.toolTip().isEmpty())
                return s.format.toolTip();
        }
    }
    return QString();
}


void TextEditorWidget::keepAutoCompletionHighlight(bool keepHighlight)
{
    d->m_keepAutoCompletionHighlight = keepHighlight;
}


void TextEditorWidget::setAutoCompleteSkipPosition(const QTextCursor &cursor)
{
    QTextCursor tc = cursor;
    // Create a selection of the next character but keep the current position, otherwise
    // the cursor would be removed from the list of automatically inserted text positions
    tc.movePosition(QTextCursor::NextCharacter);
    tc.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
    d->autocompleterHighlight(tc);
}


void TextEditorWidgetPrivate::updateCursorPosition()
{
    //update provider if needed
    const QTextBlock block = q->textCursor().block();
    if(block.isValid()){
        const int state =  TextEditor::TextDocumentLayout::lexerState(block);
        if(!m_autoCompleter.isNull()){
            m_autoCompleter->languageState(state,q->textDocument());
        }
    }
    //m_contextHelpItem = HelpItem();
    if (!block.isVisible())
        q->ensureCursorVisible();





}


} // namespace TextEditor

