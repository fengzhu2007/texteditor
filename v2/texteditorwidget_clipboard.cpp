// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
//
// v2 split of texteditor.cpp (legacy file kept at ../texteditor.cpp).
// Clipboard: copy/paste, MIME data, drag and drop, circular clipboard.
//

#include "texteditorwidget_p.h"

namespace TextEditor {

using namespace Internal;

static constexpr char dropProperty[] = "dropProp";


QString TextEditorWidget::plainTextFromSelection(const QTextCursor &cursor) const
{
    // Copy the selected text as plain text
    QString text = cursor.selectedText();
    return TextDocument::convertToPlainText(text);
}


QString TextEditorWidget::plainTextFromSelection(const Utils::MultiTextCursor &cursor) const
{
    return TextDocument::convertToPlainText(cursor.selectedText());
}


static const char kTextBlockMimeType[] = "application/vnd.qtcreator.blocktext";


void TextEditorWidgetPrivate::setClipboardSelection()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (m_cursors.hasSelection() && clipboard->supportsSelection())
        clipboard->setMimeData(q->createMimeDataFromSelection(), QClipboard::Selection);
}



void TextEditorWidgetPrivate::maybeSelectLine()
{
    MultiTextCursor cursor = m_cursors;
    if (cursor.hasSelection())
        return;
    for (QTextCursor &c : cursor) {
        const QTextBlock &block = m_document->document()->findBlock(c.selectionStart());
        const QTextBlock &end = m_document->document()->findBlock(c.selectionEnd()).next();
        c.setPosition(block.position());
        if (!end.isValid()) {
            c.movePosition(QTextCursor::PreviousCharacter);
            c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        } else {
            c.setPosition(end.position(), QTextCursor::KeepAnchor);
        }
    }
    cursor.mergeCursors();
    q->setMultiTextCursor(cursor);
}

// shift+del

void TextEditorWidget::cutLine()
{
    d->maybeSelectLine();
    cut();
}

// ctrl+ins

void TextEditorWidget::copyLine()
{
    d->maybeSelectLine();
    copy();
}


void TextEditorWidget::copyWithHtml()
{
    if (!multiTextCursor().hasSelection())
        return;
    QGuiApplication::clipboard()->setMimeData(createMimeDataFromSelection(true));
}


void TextEditorWidget::cut()
{
    copy();
    MultiTextCursor cursor = multiTextCursor();
    cursor.removeSelectedText();
    setMultiTextCursor(cursor);
    d->collectToCircularClipboard();
}


void TextEditorWidget::selectAll()
{
    QPlainTextEdit::selectAll();
    // Directly update the internal multi text cursor here to prevent calling setTextCursor.
    // This would indirectly makes sure the cursor is visible which is not desired for select all.
    const_cast<MultiTextCursor &>(d->m_cursors).setCursors({QPlainTextEdit::textCursor()});
}


void TextEditorWidget::copy()
{
    QPlainTextEdit::copy();
    d->collectToCircularClipboard();
}


void TextEditorWidget::paste()
{
    QPlainTextEdit::paste();
    encourageApply();
}


void TextEditorWidgetPrivate::collectToCircularClipboard()
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    if (!mimeData)
        return;
    CircularClipboard *circularClipBoard = CircularClipboard::instance();
    circularClipBoard->collect(TextEditorWidget::duplicateMimeData(mimeData));
    // We want the latest copied content to be the first one to appear on circular paste.
    circularClipBoard->toLastCollect();
}


void TextEditorWidget::circularPaste()
{
    CircularClipboard *circularClipBoard = CircularClipboard::instance();
    if (const QMimeData *clipboardData = QApplication::clipboard()->mimeData()) {
        circularClipBoard->collect(TextEditorWidget::duplicateMimeData(clipboardData));
        circularClipBoard->toLastCollect();
    }

    if (circularClipBoard->size() > 1) {
        invokeAssist(QuickFix, d->m_clipboardAssistProvider.data());
        return;
    }

    if (const QMimeData *mimeData = circularClipBoard->next().data()) {
        QApplication::clipboard()->setMimeData(TextEditorWidget::duplicateMimeData(mimeData));
        paste();
    }
}


void TextEditorWidget::pasteWithoutFormat()
{
    d->m_skipFormatOnPaste = true;
    paste();
    d->m_skipFormatOnPaste = false;
}


QMimeData *TextEditorWidget::createMimeDataFromSelection() const
{
    return createMimeDataFromSelection(false);
}


QMimeData *TextEditorWidget::createMimeDataFromSelection(bool withHtml) const
{
    if (multiTextCursor().hasSelection()) {
        auto mimeData = new QMimeData;

        QString text = plainTextFromSelection(multiTextCursor());
        mimeData->setText(text);

        // Copy the selected text as HTML
        if (withHtml) {
            // Create a new document from the selected text document fragment
            auto tempDocument = new QTextDocument;
            QTextCursor tempCursor(tempDocument);
            for (const QTextCursor &cursor : multiTextCursor()) {
                if (!cursor.hasSelection())
                    continue;
                tempCursor.insertFragment(cursor.selection());

                // Apply the additional formats set by the syntax highlighter
                QTextBlock start = document()->findBlock(cursor.selectionStart());
                QTextBlock last = document()->findBlock(cursor.selectionEnd());
                QTextBlock end = last.next();

                const int selectionStart = cursor.selectionStart();
                const int endOfDocument = tempDocument->characterCount() - 1;
                int removedCount = 0;
                for (QTextBlock current = start; current.isValid() && current != end;
                     current = current.next()) {
                    if (selectionVisible(current.blockNumber())) {
                        const QTextLayout *layout = current.layout();
                        const QVector<QTextLayout::FormatRange> ranges = layout->formats();
                        for (const QTextLayout::FormatRange &range : ranges) {
                            const int startPosition = current.position() + range.start
                                                      - selectionStart - removedCount;
                            const int endPosition = startPosition + range.length;
                            if (endPosition <= 0 || startPosition >= endOfDocument - removedCount)
                                continue;
                            tempCursor.setPosition(qMax(startPosition, 0));
                            tempCursor.setPosition(qMin(endPosition, endOfDocument - removedCount),
                                                   QTextCursor::KeepAnchor);
                            tempCursor.setCharFormat(range.format);
                        }
                    } else {
                        const int startPosition = current.position() - selectionStart
                                                  - removedCount;
                        int endPosition = startPosition + current.text().count();
                        if (current != last)
                            endPosition++;
                        removedCount += endPosition - startPosition;
                        tempCursor.setPosition(startPosition);
                        tempCursor.setPosition(endPosition, QTextCursor::KeepAnchor);
                        tempCursor.deleteChar();
                    }
                }
            }

            // Reset the user states since they are not interesting
            for (QTextBlock block = tempDocument->begin(); block.isValid(); block = block.next())
                block.setUserState(-1);

            // Make sure the text appears pre-formatted
            tempCursor.setPosition(0);
            tempCursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            QTextBlockFormat blockFormat = tempCursor.blockFormat();
            blockFormat.setNonBreakableLines(true);
            tempCursor.setBlockFormat(blockFormat);

            mimeData->setHtml(tempCursor.selection().toHtml());
            delete tempDocument;
        }

        if (!multiTextCursor().hasMultipleCursors()) {
            /*
                Try to figure out whether we are copying an entire block, and store the
                complete block including indentation in the qtcreator.blocktext mimetype.
            */
            QTextCursor cursor = multiTextCursor().mainCursor();
            QTextCursor selstart = cursor;
            selstart.setPosition(cursor.selectionStart());
            QTextCursor selend = cursor;
            selend.setPosition(cursor.selectionEnd());

            bool startOk = TabSettings::cursorIsAtBeginningOfLine(selstart);
            bool multipleBlocks = (selend.block() != selstart.block());

            if (startOk && multipleBlocks) {
                selstart.movePosition(QTextCursor::StartOfBlock);
                if (TabSettings::cursorIsAtBeginningOfLine(selend))
                    selend.movePosition(QTextCursor::StartOfBlock);
                cursor.setPosition(selstart.position());
                cursor.setPosition(selend.position(), QTextCursor::KeepAnchor);
                text = plainTextFromSelection(cursor);
                mimeData->setData(QLatin1String(kTextBlockMimeType), text.toUtf8());
            }
        }
        return mimeData;
    }
    return nullptr;
}


bool TextEditorWidget::canInsertFromMimeData(const QMimeData *source) const
{
    return QPlainTextEdit::canInsertFromMimeData(source);
}


struct MappedText
{
    MappedText(const QString text, MultiTextCursor &cursor)
        : text(text)
    {
        if (cursor.hasMultipleCursors()) {
            texts = text.split('\n');
            if (texts.last().isEmpty())
                texts.removeLast();
            if (texts.count() != cursor.cursorCount())
                texts.clear();
        }
    }

    QString textAt(int i) const
    {
        return texts.value(i, text);
    }

    QStringList texts;
    const QString text;
};


void TextEditorWidget::insertFromMimeData(const QMimeData *source)
{
    if (!source || isReadOnly())
        return;

    QString text = source->text();
    if (text.isEmpty())
        return;

    if (d->m_codeAssistant.hasContext())
        d->m_codeAssistant.destroyContext();

    if (d->m_snippetOverlay->isVisible() && (text.contains('\n') || text.contains('\t')))
        d->m_snippetOverlay->accept();

    const bool selectInsertedText = source->property(dropProperty).toBool();
    const TypingSettings &tps = d->m_document->typingSettings();
    MultiTextCursor cursor = multiTextCursor();
    if (!tps.m_autoIndent) {
        cursor.insertText(text, selectInsertedText);
        setMultiTextCursor(cursor);
        return;
    }

    if (source->hasFormat(QLatin1String(kTextBlockMimeType))) {
        text = QString::fromUtf8(source->data(QLatin1String(kTextBlockMimeType)));
        if (text.isEmpty())
            return;
    }

    MappedText mappedText(text, cursor);

    int index = 0;
    cursor.beginEditBlock();
    for (QTextCursor &cursor : cursor) {
        const QString textForCursor = mappedText.textAt(index++);

        cursor.removeSelectedText();

        bool insertAtBeginningOfLine = TabSettings::cursorIsAtBeginningOfLine(cursor);
        int reindentBlockStart = cursor.blockNumber() + (insertAtBeginningOfLine ? 0 : 1);

        bool hasFinalNewline = (textForCursor.endsWith(QLatin1Char('\n'))
                                || textForCursor.endsWith(QChar::ParagraphSeparator)
                                || textForCursor.endsWith(QLatin1Char('\r')));

        if (insertAtBeginningOfLine
            && hasFinalNewline) // since we'll add a final newline, preserve current line's indentation
            cursor.setPosition(cursor.block().position());

        int cursorPosition = cursor.position();
        cursor.insertText(textForCursor);
        const QTextCursor endCursor = cursor;
        QTextCursor startCursor = endCursor;
        startCursor.setPosition(cursorPosition);

        int reindentBlockEnd = cursor.blockNumber() - (hasFinalNewline ? 1 : 0);

        if (!d->m_skipFormatOnPaste
            && (reindentBlockStart < reindentBlockEnd
                || (reindentBlockStart == reindentBlockEnd
                    && (!insertAtBeginningOfLine || hasFinalNewline)))) {
            if (insertAtBeginningOfLine && !hasFinalNewline) {
                QTextCursor unnecessaryWhitespace = cursor;
                unnecessaryWhitespace.setPosition(cursorPosition);
                unnecessaryWhitespace.movePosition(QTextCursor::StartOfBlock,
                                                   QTextCursor::KeepAnchor);
                unnecessaryWhitespace.removeSelectedText();
            }
            QTextCursor c = cursor;
            c.setPosition(cursor.document()->findBlockByNumber(reindentBlockStart).position());
            c.setPosition(cursor.document()->findBlockByNumber(reindentBlockEnd).position(),
                          QTextCursor::KeepAnchor);
            d->m_document->autoReindent(c);
        }

        if (selectInsertedText) {
            cursor.setPosition(startCursor.position());
            cursor.setPosition(endCursor.position(), QTextCursor::KeepAnchor);
        }
    }
    cursor.endEditBlock();
    setMultiTextCursor(cursor);
}


void TextEditorWidget::dragLeaveEvent(QDragLeaveEvent *)
{
    const QRect rect = cursorRect(d->m_dndCursor);
    d->m_dndCursor = QTextCursor();
    if (!rect.isNull())
        viewport()->update(rect);
}


void TextEditorWidget::dragMoveEvent(QDragMoveEvent *e)
{
    const QRect rect = cursorRect(d->m_dndCursor);
    d->m_dndCursor = cursorForPosition(e->pos());
    if (!rect.isNull())
        viewport()->update(rect);
    viewport()->update(cursorRect(d->m_dndCursor));
}


void TextEditorWidget::dropEvent(QDropEvent *e)
{
    const QRect rect = cursorRect(d->m_dndCursor);
    d->m_dndCursor = QTextCursor();
    if (!rect.isNull())
        viewport()->update(rect);
    const QMimeData *mime = e->mimeData();
    if (!canInsertFromMimeData(mime))
        return;
    // Update multi text cursor before inserting data
    MultiTextCursor cursor = multiTextCursor();
    cursor.beginEditBlock();
    const QTextCursor eventCursor = cursorForPosition(e->pos());
    if (e->dropAction() == Qt::MoveAction && e->source() == viewport())
        cursor.removeSelectedText();
    cursor.setCursors({eventCursor});
    setMultiTextCursor(cursor);
    QMimeData *mimeOverwrite = nullptr;
    if (mime && (mime->hasText() || mime->hasHtml())) {
        mimeOverwrite = duplicateMimeData(mime);
        mimeOverwrite->setProperty(dropProperty, true);
        mime = mimeOverwrite;
    }
    insertFromMimeData(mime);
    delete mimeOverwrite;
    cursor.endEditBlock();
    e->acceptProposedAction();
}


QMimeData *TextEditorWidget::duplicateMimeData(const QMimeData *source)
{
    Q_ASSERT(source);

    auto mimeData = new QMimeData;
    mimeData->setText(source->text());
    mimeData->setHtml(source->html());
    if (source->hasFormat(QLatin1String(kTextBlockMimeType))) {
        mimeData->setData(QLatin1String(kTextBlockMimeType),
                          source->data(QLatin1String(kTextBlockMimeType)));
    }

    return mimeData;
}


} // namespace TextEditor

