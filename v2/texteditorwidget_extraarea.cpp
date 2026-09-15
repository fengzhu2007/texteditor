// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
//
// v2 split of texteditor.cpp (legacy file kept at ../texteditor.cpp).
// Extra area (margin): line numbers, text marks, code folding, revision markers, extra area mouse handling.
//

#include "texteditorwidget_p.h"

namespace TextEditor {

using namespace Internal;

QRect TextEditorWidgetPrivate::foldBox()
{
    if (m_highlightBlocksInfo.isEmpty() || extraAreaHighlightFoldedBlockNumber < 0)
        return {};

    QTextBlock begin = q->document()->findBlockByNumber(m_highlightBlocksInfo.open.last());

    QTextBlock end = q->document()->findBlockByNumber(m_highlightBlocksInfo.close.first());
    if (!begin.isValid() || !end.isValid())
        return {};
    QRectF br = q->blockBoundingGeometry(begin).translated(q->contentOffset());
    QRectF er = q->blockBoundingGeometry(end).translated(q->contentOffset());


    if (TextEditorSettings::fontSettings().relativeLineSpacing() == 100) {
        return QRect(m_extraArea->width() - foldBoxWidth(q->fontMetrics()),
                     int(br.top()),
                     foldBoxWidth(q->fontMetrics()),
                     int(er.bottom() - br.top()));
    }

    return QRect(m_extraArea->width() - foldBoxWidth(),
                 int(br.top()),
                 foldBoxWidth(),
                 int(er.bottom() - br.top()));
}


QTextBlock TextEditorWidgetPrivate::foldedBlockAt(const QPoint &pos, QRect *box) const
{
    QPointF offset = q->contentOffset();
    QTextBlock block = q->firstVisibleBlock();
    qreal top = q->blockBoundingGeometry(block).translated(offset).top();
    qreal bottom = top + q->blockBoundingRect(block).height();

    int viewportHeight = q->viewport()->height();

    while (block.isValid() && top <= viewportHeight) {
        QTextBlock nextBlock = block.next();
        if (block.isVisible() && bottom >= 0 && q->replacementVisible(block.blockNumber())) {
            if (nextBlock.isValid() && !nextBlock.isVisible()) {
                QTextLayout *layout = block.layout();
                QTextLine line = layout->lineAt(layout->lineCount()-1);
                QRectF lineRect = line.naturalTextRect().translated(offset.x(), top);
                lineRect.adjust(0, 0, -1, -1);

                QString replacement = QLatin1String(" {") + q->foldReplacementText(block)
                        + QLatin1String("}; ");

                QRectF collapseRect(lineRect.right() + 12,
                                    lineRect.top(),
                                    q->fontMetrics().horizontalAdvance(replacement),
                                    lineRect.height());
                if (collapseRect.contains(pos)) {
                    QTextBlock result = block;
                    if (box)
                        *box = collapseRect.toAlignedRect();
                    return result;
                } else {
                    block = nextBlock;
                    while (nextBlock.isValid() && !nextBlock.isVisible()) {
                        block = nextBlock;
                        nextBlock = block.next();
                    }
                }
            }
        }

        block = nextBlock;
        top = bottom;
        bottom = top + q->blockBoundingRect(block).height();
    }
    return QTextBlock();
}


void TextEditorWidgetPrivate::moveCursorVisible(bool ensureVisible)
{
    QTextCursor cursor = q->textCursor();
    if (!cursor.block().isVisible()) {
        cursor.setVisualNavigation(true);
        cursor.movePosition(QTextCursor::Up);
        q->setTextCursor(cursor);
    }
    if (ensureVisible)
        q->ensureCursorVisible();
}


int TextEditorWidget::visibleFoldedBlockNumber() const
{
    return d->visibleFoldedBlockNumber;
}


void TextEditorWidget::drawCollapsedBlockPopup(QPainter &painter,
                                               const QTextBlock &block,
                                               QPointF offset,
                                               const QRect &clip)
{
    if (!block.isValid())
        return;

    int margin = int(block.document()->documentMargin());
    qreal maxWidth = 0;
    qreal blockHeight = 0;
    QTextBlock b = block;

    while (!b.isVisible()) {
        b.setVisible(true); // make sure block bounding rect works
        QRectF r = blockBoundingRect(b).translated(offset);

        QTextLayout *layout = b.layout();
        for (int i = layout->lineCount()-1; i >= 0; --i)
            maxWidth = qMax(maxWidth, layout->lineAt(i).naturalTextWidth() + 2*margin);

        blockHeight += r.height();

        b.setVisible(false); // restore previous state
        b.setLineCount(0); // restore 0 line count for invisible block
        b = b.next();
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(.5, .5);
    QBrush brush = textDocument()->fontSettings().toTextCharFormat(C_TEXT).background();
    const QTextCharFormat ifdefedOutFormat = textDocument()->fontSettings().toTextCharFormat(
        C_DISABLED_CODE);
    if (ifdefedOutFormat.hasProperty(QTextFormat::BackgroundBrush))
        brush = ifdefedOutFormat.background();
    painter.setBrush(brush);
    painter.drawRoundedRect(QRectF(offset.x(),
                                   offset.y(),
                                   maxWidth, blockHeight).adjusted(0, 0, 0, 0), 3, 3);
    painter.restore();

    QTextBlock end = b;
    b = block;
    while (b != end) {
        b.setVisible(true); // make sure block bounding rect works
        QRectF r = blockBoundingRect(b).translated(offset);
        QTextLayout *layout = b.layout();
        QVector<QTextLayout::FormatRange> selections;
        layout->draw(&painter, offset, selections, clip);

        b.setVisible(false); // restore previous state
        b.setLineCount(0); // restore 0 line count for invisible block
        offset.ry() += r.height();
        b = b.next();
    }
}


QWidget *TextEditorWidget::extraArea() const
{
    return d->m_extraArea;
}


int TextEditorWidget::extraAreaWidth(int *markWidthPtr) const
{
    auto documentLayout = qobject_cast<TextDocumentLayout*>(document()->documentLayout());
    if (!documentLayout)
        return 0;

    if (!d->m_marksVisible && documentLayout->hasMarks)
        d->m_marksVisible = true;

    if (!d->m_marksVisible && !d->m_lineNumbersVisible && !d->m_codeFoldingVisible)
        return 0;

    int space = 0;
    const QFontMetrics fm(d->m_extraArea->fontMetrics());

    if (d->m_lineNumbersVisible) {
        QFont fnt = d->m_extraArea->font();
        // this works under the assumption that bold or italic
        // can only make a font wider
        const QTextCharFormat currentLineNumberFormat
            = textDocument()->fontSettings().toTextCharFormat(C_CURRENT_LINE_NUMBER);
        fnt.setBold(currentLineNumberFormat.font().bold());
        fnt.setItalic(currentLineNumberFormat.font().italic());
        const QFontMetrics linefm(fnt);

        space += linefm.horizontalAdvance(QLatin1Char('9')) * lineNumberDigits();
    }
    int markWidth = 0;

    if (d->m_marksVisible) {
        if (TextEditorSettings::fontSettings().relativeLineSpacing() == 100)
            markWidth += fm.lineSpacing() + 2;
        else
            markWidth += TextEditorSettings::fontSettings().lineSpacing() + 2;

//     if (documentLayout->doubleMarkCount)
//         markWidth += fm.lineSpacing() / 3;
        space += markWidth;
    } else {
        space += 2;
    }

    if (markWidthPtr)
        *markWidthPtr = markWidth;

    space += 10;

    if (d->m_codeFoldingVisible) {
        if (TextEditorSettings::fontSettings().relativeLineSpacing() == 100)
            space += foldBoxWidth(fm);
        else
            space += foldBoxWidth();
    }

    if (viewportMargins() != QMargins{isLeftToRight() ? space : 0, 0, isLeftToRight() ? 0 : space, 0})
        d->slotUpdateExtraAreaWidth(space);

    return space;
}


void TextEditorWidgetPrivate::slotUpdateExtraAreaWidth(std::optional<int> width)
{
    if (!width.has_value())
        width = q->extraAreaWidth();
    if (q->isLeftToRight())
        q->setViewportMargins(*width, 0, 0, 0);
    else
        q->setViewportMargins(0, 0, *width, 0);
}


void TextEditorWidgetPrivate::paintLineNumbers(QPainter &painter,
                                               const ExtraAreaPaintEventData &data,
                                               const QRectF &blockBoundingRect) const
{
    if (!m_lineNumbersVisible)
        return;

    const QString &number = q->lineNumber(data.block.blockNumber());
    const bool selected = (
                (data.selectionStart < data.block.position() + data.block.length()
                 && data.selectionEnd > data.block.position())
                || (data.selectionStart == data.selectionEnd && data.selectionEnd == data.block.position())
                );
    if (selected) {
        painter.save();
        QFont f = painter.font();
        f.setBold(data.currentLineNumberFormat.font().bold());
        f.setItalic(data.currentLineNumberFormat.font().italic());
        painter.setFont(f);
        painter.setPen(data.currentLineNumberFormat.foreground().color());
        if (data.currentLineNumberFormat.background() != Qt::NoBrush) {
            painter.fillRect(QRectF(0, blockBoundingRect.top(),
                                   data.extraAreaWidth, blockBoundingRect.height()),
                             data.currentLineNumberFormat.background().color());
        }
    }
    painter.drawText(QRectF(data.markWidth, blockBoundingRect.top(),
                            data.extraAreaWidth - data.markWidth - 4, blockBoundingRect.height()),
                     Qt::AlignRight,
                     number);
    if (selected)
        painter.restore();
}


void TextEditorWidgetPrivate::paintTextMarks(QPainter &painter, const ExtraAreaPaintEventData &data,
                                             const QRectF &blockBoundingRect) const
{
    auto userData = static_cast<TextBlockUserData*>(data.block.userData());
    if (!userData || !m_marksVisible)
        return;
    TextMarks marks = userData->marks();
    QList<QIcon> icons;
    auto end = marks.crend();
    int marksWithIconCount = 0;
    QIcon overrideIcon;
    for (auto it = marks.crbegin(); it != end; ++it) {
        if ((*it)->isVisible()) {
            const QIcon icon = (*it)->icon();
            if (!icon.isNull()) {
                if ((*it)->isLocationMarker()) {
                    overrideIcon = icon;
                } else {
                    if (icons.size() < 3
                            && !Utils::contains(icons, Utils::equal(&QIcon::cacheKey, icon.cacheKey()))) {
                        icons << icon;
                    }
                    ++marksWithIconCount;
                }
            }
        }
    }


    int size = data.lineSpacing - 1;
    int xoffset = 0;
    int yoffset = blockBoundingRect.top();

    painter.save();
    Utils::ExecuteOnDestruction eod([&painter, size, yoffset, xoffset, overrideIcon]() {
        if (!overrideIcon.isNull()) {
            const QRect r(xoffset, yoffset, size, size);
            overrideIcon.paint(&painter, r, Qt::AlignCenter);
        }
        painter.restore();
    });

    if (icons.isEmpty())
        return;

    if (icons.size() == 1) {
        const QRect r(xoffset, yoffset, size, size);
        icons.first().paint(&painter, r, Qt::AlignCenter);
        return;
    }
    size = size / 2;
    for (const QIcon &icon : std::as_const(icons)) {
        const QRect r(xoffset, yoffset, size, size);
        icon.paint(&painter, r, Qt::AlignCenter);
        if (xoffset != 0) {
            yoffset += size;
            xoffset = 0;
        } else {
            xoffset = size;
        }
    }
    QFont font = painter.font();
    font.setPixelSize(size);
    painter.setFont(font);

    const QColor color = data.currentLineNumberFormat.foreground().color();
    if (color.isValid())
        painter.setPen(color);

    const QRect r(size, blockBoundingRect.top() + size, size, size);
    const QString detail = marksWithIconCount > 9 ? QString("+")
                                                  : QString::number(marksWithIconCount);
    painter.drawText(r, Qt::AlignRight, detail);
}


static void drawRectBox(QPainter *painter, const QRect &rect, const QPalette &pal)
{
    painter->save();
    painter->setOpacity(0.5);
    painter->fillRect(rect, pal.brush(QPalette::Highlight));
    painter->restore();
}


void TextEditorWidgetPrivate::paintCodeFolding(QPainter &painter,
                                               const ExtraAreaPaintEventData &data,
                                               const QRectF &blockBoundingRect) const
{
    if (!m_codeFoldingVisible)
        return;

    int extraAreaHighlightFoldBlockNumber = -1;
    int extraAreaHighlightFoldEndBlockNumber = -1;
    if (!m_highlightBlocksInfo.isEmpty()) {
        extraAreaHighlightFoldBlockNumber = m_highlightBlocksInfo.open.last();
        extraAreaHighlightFoldEndBlockNumber = m_highlightBlocksInfo.close.first();
    }

    const QTextBlock &nextBlock = data.block.next();
    TextBlockUserData *nextBlockUserData = TextDocumentLayout::textUserData(nextBlock);

    bool drawBox = nextBlockUserData
            && TextDocumentLayout::foldingIndent(data.block) < nextBlockUserData->foldingIndent();


    const int blockNumber = data.block.blockNumber();
    bool active = blockNumber == extraAreaHighlightFoldBlockNumber;
    bool hovered = blockNumber >= extraAreaHighlightFoldBlockNumber
            && blockNumber <= extraAreaHighlightFoldEndBlockNumber;

    int boxWidth = 0;
    if (TextEditorSettings::fontSettings().relativeLineSpacing() == 100)
        boxWidth = foldBoxWidth(data.fontMetrics);
    else
        boxWidth = foldBoxWidth();

    if (hovered) {
        int itop = qRound(blockBoundingRect.top());
        int ibottom = qRound(blockBoundingRect.bottom());
        QRect box = QRect(data.extraAreaWidth + 1, itop, boxWidth - 2, ibottom - itop);
        drawRectBox(&painter, box, data.palette);
    }

    if (drawBox) {
        bool expanded = nextBlock.isVisible();
        int size = boxWidth/4;
        QRect box(data.extraAreaWidth + size, int(blockBoundingRect.top()) + size,
                  2 * (size) + 1, 2 * (size) + 1);
        drawFoldingMarker(&painter, data.palette, box, expanded, active, hovered);
    }

}


void TextEditorWidgetPrivate::paintRevisionMarker(QPainter &painter,
                                                  const ExtraAreaPaintEventData &data,
                                                  const QRectF &blockBoundingRect) const
{
    if (m_revisionsVisible && data.block.revision() != data.documentLayout->lastSaveRevision) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, false);
        if (data.block.revision() < 0)
            painter.setPen(QPen(Qt::darkGreen, 2));
        else
            painter.setPen(QPen(Qt::red, 2));
        painter.drawLine(data.extraAreaWidth - 1, int(blockBoundingRect.top()),
                         data.extraAreaWidth - 1, int(blockBoundingRect.bottom()) - 1);
        painter.restore();
    }
}


void TextEditorWidget::extraAreaPaintEvent(QPaintEvent *e)
{
    ExtraAreaPaintEventData data(this, d);
    QTC_ASSERT(data.documentLayout, return);

    QPainter painter(d->m_extraArea);

    painter.fillRect(e->rect(), data.palette.color(QPalette::Window));

    data.block = firstVisibleBlock();
    QPointF offset = contentOffset();
    QRectF boundingRect = blockBoundingRect(data.block).translated(offset);

    while (data.block.isValid() && boundingRect.top() <= e->rect().bottom()) {
        if (boundingRect.bottom() >= e->rect().top()) {

            painter.setPen(data.palette.color(QPalette::Dark));

            d->paintLineNumbers(painter, data, boundingRect);

            if (d->m_codeFoldingVisible || d->m_marksVisible) {
                painter.save();
                painter.setRenderHint(QPainter::Antialiasing, false);

                d->paintTextMarks(painter, data, boundingRect);
                d->paintCodeFolding(painter, data, boundingRect);

                painter.restore();
            }

            d->paintRevisionMarker(painter, data, boundingRect);
        }

        offset.ry() += boundingRect.height();
        data.block = d->nextVisibleBlock(data.block);
        boundingRect = blockBoundingRect(data.block).translated(offset);
    }
}


void TextEditorWidgetPrivate::drawFoldingMarker(QPainter *painter, const QPalette &pal,
                                       const QRect &rect,
                                       bool expanded,
                                       bool active,
                                       bool hovered) const
{
   /* QStyle *s = q->style();
    if (auto ms = qobject_cast<ManhattanStyle*>(s))
        s = ms->baseStyle();

    QStyleOptionViewItem opt;
    opt.rect = rect;
    opt.state = QStyle::State_Active | QStyle::State_Item | QStyle::State_Children;
    if (expanded)
        opt.state |= QStyle::State_Open;
    if (active)
        opt.state |= QStyle::State_MouseOver | QStyle::State_Enabled | QStyle::State_Selected;
    if (hovered)
        opt.palette.setBrush(QPalette::Window, pal.highlight());

    const char *className = s->metaObject()->className();

    // Do not use the windows folding marker since we cannot style them and the default hover color
    // is a blue which does not guarantee an high contrast on all themes.
    static QPointer<QStyle> fusionStyleOverwrite = nullptr;
    if (!qstrcmp(className, "QWindowsVistaStyle")) {
        if (fusionStyleOverwrite.isNull())
            fusionStyleOverwrite = QStyleFactory::create("fusion");
        if (!fusionStyleOverwrite.isNull()) {
            s = fusionStyleOverwrite.data();
            className = s->metaObject()->className();
        }
    }

    if (!qstrcmp(className, "OxygenStyle")) {
        const QStyle::PrimitiveElement direction = expanded ? QStyle::PE_IndicatorArrowDown
                                                            : QStyle::PE_IndicatorArrowRight;
        StyleHelper::drawArrow(direction, painter, &opt);
    } else {
         // QGtkStyle needs a small correction to draw the marker in the right place
        if (!qstrcmp(className, "QGtkStyle"))
           opt.rect.translate(-2, 0);
        else if (!qstrcmp(className, "QMacStyle"))
            opt.rect.translate(-2, 0);
        else if (!qstrcmp(className, "QFusionStyle"))
            opt.rect.translate(0, -1);

        s->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, painter, q);
    }*/
}


void TextEditorWidgetPrivate::slotUpdateRequest(const QRect &r, int dy)
{
    if (dy) {
        m_extraArea->scroll(0, dy);
    } else if (r.width() > 4) { // wider than cursor width, not just cursor blinking
        m_extraArea->update(0, r.y(), m_extraArea->width(), r.height());
        if (!m_searchExpr.pattern().isEmpty()) {
            const int m = m_searchResultOverlay->dropShadowWidth();
            q->viewport()->update(r.adjusted(-m, -m, m, m));
        }
    }

    if (r.contains(q->viewport()->rect()))
        slotUpdateExtraAreaWidth();
}


void TextEditorWidgetPrivate::clearVisibleFoldedBlock()
{
    if (suggestedVisibleFoldedBlockNumber) {
        suggestedVisibleFoldedBlockNumber = -1;
        foldedBlockTimer.stop();
    }
    if (visibleFoldedBlockNumber >= 0) {
        visibleFoldedBlockNumber = -1;
        q->viewport()->update();
    }
}


void TextEditorWidget::extraAreaLeaveEvent(QEvent *)
{
    d->extraAreaPreviousMarkTooltipRequestedLine = -1;
    ToolTip::hide();

    // fake missing mouse move event from Qt
    QMouseEvent me(QEvent::MouseMove, QPoint(-1, -1), Qt::NoButton, {}, {});
    extraAreaMouseEvent(&me);
}


void TextEditorWidget::extraAreaContextMenuEvent(QContextMenuEvent *e)
{
    if (d->m_marksVisible) {
        QTextCursor cursor = cursorForPosition(QPoint(0, e->pos().y()));
        auto contextMenu = new QMenu(this);
        emit markContextMenuRequested(this, cursor.blockNumber() + 1, contextMenu);
        if (!contextMenu->isEmpty())
            contextMenu->exec(e->globalPos());
        delete contextMenu;
        e->accept();
    }
}


void TextEditorWidget::updateFoldingHighlight(const QPoint &pos)
{
    if (!d->m_codeFoldingVisible)
        return;

    QTextCursor cursor = cursorForPosition(QPoint(0, pos.y()));

    // Update which folder marker is highlighted
    const int highlightBlockNumber = d->extraAreaHighlightFoldedBlockNumber;
    d->extraAreaHighlightFoldedBlockNumber = -1;

    int boxWidth = 0;
    if (TextEditorSettings::fontSettings().relativeLineSpacing() == 100)
        boxWidth = foldBoxWidth(fontMetrics());
    else
        boxWidth = foldBoxWidth();

    if (pos.x() > extraArea()->width() - boxWidth) {
        d->extraAreaHighlightFoldedBlockNumber = cursor.blockNumber();
    } else if (d->m_displaySettings.m_highlightBlocks) {
        QTextCursor cursor = textCursor();
        d->extraAreaHighlightFoldedBlockNumber = cursor.blockNumber();
    }

    if (highlightBlockNumber != d->extraAreaHighlightFoldedBlockNumber)
        d->m_highlightBlocksTimer.start(d->m_highlightBlocksInfo.isEmpty() ? 120 : 0);
}


void TextEditorWidget::extraAreaMouseEvent(QMouseEvent *e)
{
    QTextCursor cursor = cursorForPosition(QPoint(0, e->pos().y()));

    int markWidth = 0;
    extraAreaWidth(&markWidth);
    const bool inMarkArea = e->pos().x() <= markWidth && e->pos().x() >= 0;

    if (d->m_codeFoldingVisible
            && e->type() == QEvent::MouseMove && e->buttons() == 0) { // mouse tracking
        updateFoldingHighlight(e->pos());
    }

    // Set whether the mouse cursor is a hand or normal arrow
    if (e->type() == QEvent::MouseMove) {
        if (inMarkArea) {
            int line = cursor.blockNumber() + 1;
            if (d->extraAreaPreviousMarkTooltipRequestedLine != line) {
                if (auto data = static_cast<TextBlockUserData *>(cursor.block().userData())) {
                    if (data->marks().isEmpty())
                        ToolTip::hide();
                    else
                        d->showTextMarksToolTip(mapToGlobal(e->pos()), data->marks());
                }
            }
            d->extraAreaPreviousMarkTooltipRequestedLine = line;
        }

        if (!d->m_markDragging && e->buttons() & Qt::LeftButton && !d->m_markDragStart.isNull()) {
            int dist = (e->pos() - d->m_markDragStart).manhattanLength();
            if (dist > QApplication::startDragDistance()) {
                d->m_markDragging = true;
                const int height = fontMetrics().lineSpacing() - 1;
                d->m_markDragCursor = QCursor(d->m_dragMark->icon().pixmap({height, height}));
                d->m_dragMark->setVisible(false);
                QGuiApplication::setOverrideCursor(d->m_markDragCursor);
            }
        }

        if (d->m_markDragging) {
            QGuiApplication::changeOverrideCursor(inMarkArea ? d->m_markDragCursor
                                                             : QCursor(Qt::ForbiddenCursor));
        } else if (inMarkArea != (d->m_extraArea->cursor().shape() == Qt::PointingHandCursor)) {
            d->m_extraArea->setCursor(inMarkArea ? Qt::PointingHandCursor : Qt::ArrowCursor);
        }
    }

    if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonDblClick) {
        if (e->button() == Qt::LeftButton) {
            int boxWidth = 0;
            if (TextEditorSettings::fontSettings().relativeLineSpacing() == 100)
                boxWidth = foldBoxWidth(fontMetrics());
            else
                boxWidth = foldBoxWidth();
            if (d->m_codeFoldingVisible && e->pos().x() > extraArea()->width() - boxWidth) {
                if (!cursor.block().next().isVisible()) {
                    d->toggleBlockVisible(cursor.block());
                    d->moveCursorVisible(false);
                } else if (d->foldBox().contains(e->pos())) {
                    cursor.setPosition(
                            document()->findBlockByNumber(d->m_highlightBlocksInfo.open.last()).position()
                            );
                    QTextBlock c = cursor.block();
                    d->toggleBlockVisible(c);
                    d->moveCursorVisible(false);
                }
            } else if (d->m_lineNumbersVisible && !inMarkArea) {
                QTextCursor selection = cursor;
                selection.setVisualNavigation(true);
                d->extraAreaSelectionAnchorBlockNumber = selection.blockNumber();
                selection.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                selection.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                setTextCursor(selection);
            } else {
                d->extraAreaToggleMarkBlockNumber = cursor.blockNumber();
                d->m_markDragging = false;
                QTextBlock block = cursor.document()->findBlockByNumber(d->extraAreaToggleMarkBlockNumber);
                if (auto data = static_cast<TextBlockUserData *>(block.userData())) {
                    TextMarks marks = data->marks();
                    for (int i = marks.size(); --i >= 0; ) {
                        TextMark *mark = marks.at(i);
                        if (mark->isDraggable()) {
                            d->m_markDragStart = e->pos();
                            d->m_dragMark = mark;
                            break;
                        }
                    }
                }
            }
        }
    } else if (d->extraAreaSelectionAnchorBlockNumber >= 0) {
        QTextCursor selection = cursor;
        selection.setVisualNavigation(true);
        if (e->type() == QEvent::MouseMove) {
            QTextBlock anchorBlock = document()->findBlockByNumber(d->extraAreaSelectionAnchorBlockNumber);
            selection.setPosition(anchorBlock.position());
            if (cursor.blockNumber() < d->extraAreaSelectionAnchorBlockNumber) {
                selection.movePosition(QTextCursor::EndOfBlock);
                selection.movePosition(QTextCursor::Right);
            }
            selection.setPosition(cursor.block().position(), QTextCursor::KeepAnchor);
            if (cursor.blockNumber() >= d->extraAreaSelectionAnchorBlockNumber) {
                selection.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                selection.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            }

            if (e->pos().y() >= 0 && e->pos().y() <= d->m_extraArea->height())
                d->autoScrollTimer.stop();
            else if (!d->autoScrollTimer.isActive())
                d->autoScrollTimer.start(100, this);

        } else {
            d->autoScrollTimer.stop();
            d->extraAreaSelectionAnchorBlockNumber = -1;
            return;
        }
        setTextCursor(selection);
    } else if (d->extraAreaToggleMarkBlockNumber >= 0 && d->m_marksVisible && d->m_requestMarkEnabled) {
        if (e->type() == QEvent::MouseButtonRelease && e->button() == Qt::LeftButton) {
            int n = d->extraAreaToggleMarkBlockNumber;
            d->extraAreaToggleMarkBlockNumber = -1;
            const bool sameLine = cursor.blockNumber() == n;
            const bool wasDragging = d->m_markDragging;
            TextMark *dragMark = d->m_dragMark;
            d->m_dragMark = nullptr;
            d->m_markDragging = false;
            d->m_markDragStart = QPoint();
            if (dragMark)
                dragMark->setVisible(true);
            QGuiApplication::restoreOverrideCursor();
            if (wasDragging && dragMark) {
                dragMark->dragToLine(cursor.blockNumber() + 1);
                return;
            } else if (sameLine) {
                QTextBlock block = cursor.document()->findBlockByNumber(n);
                if (auto data = static_cast<TextBlockUserData *>(block.userData())) {
                    TextMarks marks = data->marks();
                    for (int i = marks.size(); --i >= 0; ) {
                        TextMark *mark = marks.at(i);
                        if (mark->isClickable()) {
                            mark->clicked();
                            return;
                        }
                    }
                }
            }
            int line = n + 1;
            TextMarkRequestKind kind;
            if (QApplication::keyboardModifiers() & Qt::ShiftModifier)
                kind = BookmarkRequest;
            else
                kind = BreakpointRequest;

            emit markRequested(this, line, kind);
        }
    }
}


void TextEditorWidget::ensureBlockIsUnfolded(QTextBlock block)
{
    if (!block.isVisible()) {
        auto documentLayout = qobject_cast<TextDocumentLayout*>(document()->documentLayout());
        QTC_ASSERT(documentLayout, return);

        // Open all parent folds of current line.
        int indent = TextDocumentLayout::foldingIndent(block);
        block = block.previous();
        while (block.isValid()) {
            const int indent2 = TextDocumentLayout::foldingIndent(block);
            if (TextDocumentLayout::canFold(block) && indent2 < indent) {
                TextDocumentLayout::doFoldOrUnfold(block, /* unfold = */ true);
                if (block.isVisible())
                    break;
                indent = indent2;
            }
            block = block.previous();
        }

        documentLayout->requestUpdate();
        documentLayout->emitDocumentSizeChanged();
    }
}


void TextEditorWidgetPrivate::toggleBlockVisible(const QTextBlock &block)
{
    auto documentLayout = qobject_cast<TextDocumentLayout*>(q->document()->documentLayout());
    QTC_ASSERT(documentLayout, return);

    TextDocumentLayout::doFoldOrUnfold(block, TextDocumentLayout::isFolded(block));
    documentLayout->requestUpdate();
    documentLayout->emitDocumentSizeChanged();
}


void TextEditorWidgetPrivate::markRemoved(TextMark *mark)
{
    if (m_dragMark == mark) {
        m_dragMark = nullptr;
        m_markDragging = false;
        m_markDragStart = QPoint();
        QGuiApplication::restoreOverrideCursor();
    }

    auto it = m_annotationRects.find(mark->lineNumber() - 1);
    if (it == m_annotationRects.end())
        return;

    Utils::erase(it.value(), [mark](AnnotationRect rect) {
        return rect.mark == mark;
    });
}


void TextEditorWidget::fold()
{
    QTextDocument *doc = document();
    auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);
    QTextBlock block = textCursor().block();
    if (!(TextDocumentLayout::canFold(block) && block.next().isVisible())) {
        // find the closest previous block which can fold
        int indent = TextDocumentLayout::foldingIndent(block);
        while (block.isValid() && (TextDocumentLayout::foldingIndent(block) >= indent || !block.isVisible()))
            block = block.previous();
    }
    if (block.isValid()) {
        TextDocumentLayout::doFoldOrUnfold(block, false);
        d->moveCursorVisible();
        documentLayout->requestUpdate();
        documentLayout->emitDocumentSizeChanged();
    }
}


void TextEditorWidget::unfold()
{
    QTextDocument *doc = document();
    auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);
    QTextBlock block = textCursor().block();
    while (block.isValid() && !block.isVisible())
        block = block.previous();
    TextDocumentLayout::doFoldOrUnfold(block, true);
    d->moveCursorVisible();
    documentLayout->requestUpdate();
    documentLayout->emitDocumentSizeChanged();
}


void TextEditorWidget::unfoldAll()
{
    QTextDocument *doc = document();
    auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);

    QTextBlock block = doc->firstBlock();
    bool makeVisible = true;
    while (block.isValid()) {
        if (block.isVisible() && TextDocumentLayout::canFold(block) && block.next().isVisible()) {
            makeVisible = false;
            break;
        }
        block = block.next();
    }

    block = doc->firstBlock();

    while (block.isValid()) {
        if (TextDocumentLayout::canFold(block))
            TextDocumentLayout::doFoldOrUnfold(block, makeVisible);
        block = block.next();
    }

    d->moveCursorVisible();
    documentLayout->requestUpdate();
    documentLayout->emitDocumentSizeChanged();
    centerCursor();
}


QString TextEditorWidget::lineNumber(int blockNumber) const
{
    return QString::number(blockNumber + 1);
}


int TextEditorWidget::lineNumberDigits() const
{
    int digits = 2;
    int max = qMax(1, blockCount());
    while (max >= 100) {
        max /= 10;
        ++digits;
    }
    return digits;
}


bool TextEditorWidget::selectionVisible(int blockNumber) const
{
    Q_UNUSED(blockNumber)
    return true;
}


bool TextEditorWidget::replacementVisible(int blockNumber) const
{
    Q_UNUSED(blockNumber)
    return true;
}


QColor TextEditorWidget::replacementPenColor(int blockNumber) const
{
    Q_UNUSED(blockNumber)
    return {};
}


QString TextEditorWidget::foldReplacementText(const QTextBlock &) const
{
    return QLatin1String("...");
}



} // namespace TextEditor

