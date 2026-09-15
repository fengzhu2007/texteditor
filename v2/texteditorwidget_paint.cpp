// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
//
// v2 split of texteditor.cpp (legacy file kept at ../texteditor.cpp).
// Painting: paint event and all paint helpers, TextEditorAnimator, collapsed block popup.
//

#include "texteditorwidget_p.h"

namespace TextEditor {

using namespace Internal;

static QColor blendColors(const QColor &a, const QColor &b, int alpha)
{
    return QColor((a.red()   * (256 - alpha) + b.red()   * alpha) / 256,
                  (a.green() * (256 - alpha) + b.green() * alpha) / 256,
                  (a.blue()  * (256 - alpha) + b.blue()  * alpha) / 256);
}


static QColor calcBlendColor(const QColor &baseColor, int level, int count)
{
    QColor color80;
    QColor color90;

    if (baseColor.value() > 128) {
        const int f90 = 15;
        const int f80 = 30;
        color80.setRgb(qMax(0, baseColor.red() - f80),
                       qMax(0, baseColor.green() - f80),
                       qMax(0, baseColor.blue() - f80));
        color90.setRgb(qMax(0, baseColor.red() - f90),
                       qMax(0, baseColor.green() - f90),
                       qMax(0, baseColor.blue() - f90));
    } else {
        const int f90 = 20;
        const int f80 = 40;
        color80.setRgb(qMin(255, baseColor.red() + f80),
                       qMin(255, baseColor.green() + f80),
                       qMin(255, baseColor.blue() + f80));
        color90.setRgb(qMin(255, baseColor.red() + f90),
                       qMin(255, baseColor.green() + f90),
                       qMin(255, baseColor.blue() + f90));
    }

    if (level == count)
        return baseColor;
    if (level == 0)
        return color80;
    if (level == count - 1)
        return color90;

    const int blendFactor = level * (256 / (count - 2));

    return blendColors(color80, color90, blendFactor);
}


static QTextLayout::FormatRange createBlockCursorCharFormatRange(int pos,
                                                                 const QColor &textColor,
                                                                 const QColor &baseColor)
{
    QTextLayout::FormatRange o;
    o.start = pos;
    o.length = 1;
    o.format.setForeground(baseColor);
    o.format.setBackground(textColor);
    return o;
}


static TextMarks availableMarks(const TextMarks &marks,
                                QRectF &boundingRect,
                                const QFontMetrics &fm,
                                const qreal itemOffset)
{
    TextMarks ret;
    bool first = true;
    for (TextMark *mark : marks) {
        const TextMark::AnnotationRects &rects = mark->annotationRects(
                    boundingRect, fm, first ? 0 : itemOffset, 0);
        if (rects.annotationRect.isEmpty())
            break;
        boundingRect.setLeft(rects.fadeOutRect.right());
        ret.append(mark);
        if (boundingRect.isEmpty())
            break;
        first = false;
    }
    return ret;
}


QRectF TextEditorWidgetPrivate::getLastLineLineRect(const QTextBlock &block)
{
    //const QTextLayout *layout = block.layout();
    QTextLayout *layout = nullptr;
    if (TextSuggestion *suggestion = TextDocumentLayout::suggestion(block))
        layout = suggestion->replacementDocument()->firstBlock().layout();
    else
        layout = block.layout();
    const int lineCount = layout->lineCount();
    if (lineCount < 1)
        return {};
    const QTextLine line = layout->lineAt(lineCount - 1);
    const QPointF contentOffset = q->contentOffset();
    const qreal top = q->blockBoundingGeometry(block).translated(contentOffset).top();
    return line.naturalTextRect().translated(contentOffset.x(), top).adjusted(0, 0, -1, -1);
}


bool TextEditorWidgetPrivate::updateAnnotationBounds(TextBlockUserData *blockUserData,
                                                     TextDocumentLayout *layout,
                                                     bool annotationsVisible)
{
    const bool additionalHeightNeeded = annotationsVisible
            && m_displaySettings.m_annotationAlignment == AnnotationAlignment::BetweenLines;

    int additionalHeight = 0;
    if (additionalHeightNeeded) {
        if (TextEditorSettings::fontSettings().relativeLineSpacing() == 100)
            additionalHeight = q->fontMetrics().lineSpacing();
        else
            TextEditorSettings::fontSettings().lineSpacing();
    }

    if (blockUserData->additionalAnnotationHeight() == additionalHeight)
        return false;
    blockUserData->setAdditionalAnnotationHeight(additionalHeight);
    q->viewport()->update();
    layout->emitDocumentSizeChanged();
    return true;
}


void TextEditorWidgetPrivate::updateLineAnnotation(const PaintEventData &data,
                                                   const PaintEventBlockData &blockData,
                                                   QPainter &painter)
{
    const QList<AnnotationRect> previousRects = m_annotationRects.take(data.block.blockNumber());
    if (!m_displaySettings.m_displayAnnotations)
        return;

    TextBlockUserData *blockUserData = TextDocumentLayout::textUserData(data.block);
    if (!blockUserData)
        return;

    TextMarks marks = Utils::filtered(blockUserData->marks(), [](const TextMark* mark){
        return !mark->lineAnnotation().isEmpty() && mark->isVisible();
    });

    const bool annotationsVisible = !marks.isEmpty();

    if (updateAnnotationBounds(blockUserData, data.documentLayout, annotationsVisible)
            || !annotationsVisible) {
        return;
    }

    const QRectF lineRect = getLastLineLineRect(data.block);
    if (lineRect.isNull())
        return;

    Utils::sort(marks, [](const TextMark* mark1, const TextMark* mark2){
        return mark1->priority() > mark2->priority();
    });

    qreal itemOffset = 0.0;
    if (TextEditorSettings::fontSettings().relativeLineSpacing() == 100)
        itemOffset = q->fontMetrics().lineSpacing();
    else
        itemOffset = blockData.boundingRect.height();


    const qreal initialOffset = m_displaySettings.m_annotationAlignment == AnnotationAlignment::BetweenLines ? itemOffset / 2 : itemOffset * 2;
    const qreal minimalContentWidth = q->fontMetrics().horizontalAdvance('X')
            * m_displaySettings.m_minimalAnnotationContent;
    qreal offset = initialOffset;
    qreal x = 0;
    if (marks.isEmpty())
        return;
    QRectF boundingRect;
    if (m_displaySettings.m_annotationAlignment == AnnotationAlignment::BetweenLines) {
        boundingRect = QRectF(lineRect.bottomLeft(), blockData.boundingRect.bottomRight());
    } else {
        boundingRect = QRectF(lineRect.topLeft().x(), lineRect.topLeft().y(),
                              q->viewport()->width() - lineRect.right(), lineRect.height());
        x = lineRect.right();
        if (m_displaySettings.m_annotationAlignment == AnnotationAlignment::NextToMargin
                && data.rightMargin > lineRect.right() + offset
                && q->viewport()->width() > data.rightMargin + minimalContentWidth) {
            offset = data.rightMargin - lineRect.right();
        } else if (m_displaySettings.m_annotationAlignment != AnnotationAlignment::NextToContent) {
            marks = availableMarks(marks, boundingRect, q->fontMetrics(), itemOffset);
            if (boundingRect.width() > 0)
                offset = qMax(boundingRect.width(), initialOffset);
        }
    }

    QList<AnnotationRect> newRects;
    for (const TextMark *mark : std::as_const(marks)) {
        boundingRect = QRectF(x, boundingRect.top(), q->viewport()->width() - x, boundingRect.height());
        if (boundingRect.isEmpty())
            break;

        mark->paintAnnotation(painter,
                              data.eventRect,
                              &boundingRect,
                              offset,
                              itemOffset / 2,
                              q->contentOffset());

        x = boundingRect.right();
        offset = itemOffset / 2;
        newRects.append({boundingRect, mark});
    }

    if (previousRects != newRects) {
        for (const AnnotationRect &annotationRect : qAsConst(newRects))
            q->viewport()->update(annotationRect.rect.toAlignedRect());
        for (const AnnotationRect &annotationRect : previousRects)
            q->viewport()->update(annotationRect.rect.toAlignedRect());
    }
    m_annotationRects[data.block.blockNumber()] = newRects;
}


QColor blendRightMarginColor(const FontSettings &settings, bool areaColor)
{
    const QColor baseColor = settings.toTextCharFormat(C_TEXT).background().color();
    const QColor col = (baseColor.value() > 128) ? Qt::black : Qt::white;
    return blendColors(baseColor, col, areaColor ? 16 : 32);
}


void TextEditorWidgetPrivate::paintRightMarginArea(PaintEventData &data, QPainter &painter) const
{
    if (m_visibleWrapColumn <= 0)
        return;
    // Don't use QFontMetricsF::averageCharWidth here, due to it returning
    // a fractional size even when this is not supported by the platform.
    data.rightMargin = QFontMetricsF(q->font()).horizontalAdvance(QLatin1Char('x'))
            * (m_visibleWrapColumn + m_visualIndentOffset)
            + data.offset.x() + 4;
    if (m_marginSettings.m_tintMarginArea && data.rightMargin < data.viewportRect.width()) {
        const QRectF behindMargin(data.rightMargin,
                                  data.eventRect.top(),
                                  data.viewportRect.width() - data.rightMargin,
                                  data.eventRect.height());
        painter.fillRect(behindMargin, blendRightMarginColor(m_document->fontSettings(), true));
    }
}


void TextEditorWidgetPrivate::paintRightMarginLine(const PaintEventData &data,
                                                   QPainter &painter) const
{
    if (m_visibleWrapColumn <= 0 || data.rightMargin >= data.viewportRect.width())
        return;

    const QPen pen = painter.pen();
    painter.setPen(blendRightMarginColor(m_document->fontSettings(), false));
    painter.drawLine(QPointF(data.rightMargin, data.eventRect.top()),
                     QPointF(data.rightMargin, data.eventRect.bottom()));
    painter.setPen(pen);
}


void TextEditorWidgetPrivate::paintBlockHighlight(const PaintEventData &data,
                                                  QPainter &painter) const
{
    if (m_highlightBlocksInfo.isEmpty())
        return;

    const QColor baseColor = m_document->fontSettings().toTextCharFormat(C_TEXT).background().color();

    // extra pass for the block highlight

    const int margin = 5;
    QTextBlock block = data.block;
    QPointF offset = data.offset;
    while (block.isValid()) {
        QRectF blockBoundingRect = q->blockBoundingRect(block).translated(offset);

        int n = block.blockNumber();
        int depth = 0;
        const QList<int> open = m_highlightBlocksInfo.open;
        for (const int i : open)
            if (n >= i)
                ++depth;
        const QList<int> close = m_highlightBlocksInfo.close;
        for (const int i : close)
            if (n > i)
                --depth;

        int count = m_highlightBlocksInfo.count();
        if (count) {
            for (int i = 0; i <= depth; ++i) {
                const QColor &blendedColor = calcBlendColor(baseColor, i, count);
                int vi = i > 0 ? m_highlightBlocksInfo.visualIndent.at(i - 1) : 0;
                QRectF oneRect = blockBoundingRect;
                oneRect.setWidth(qMax(data.viewportRect.width(), data.documentWidth));
                oneRect.adjust(vi, 0, 0, 0);
                if (oneRect.left() >= oneRect.right())
                    continue;
                if (data.rightMargin > 0 && oneRect.left() < data.rightMargin
                        && oneRect.right() > data.rightMargin) {
                    QRectF otherRect = blockBoundingRect;
                    otherRect.setLeft(data.rightMargin + 1);
                    otherRect.setRight(oneRect.right());
                    oneRect.setRight(data.rightMargin - 1);
                    painter.fillRect(otherRect, blendedColor);
                }
                painter.fillRect(oneRect, blendedColor);
            }
        }
        offset.ry() += blockBoundingRect.height();

        if (offset.y() > data.viewportRect.height() + margin)
            break;

        block = TextEditor::nextVisibleBlock(block, data.doc);
    }

}


void TextEditorWidgetPrivate::paintSearchResultOverlay(const PaintEventData &data,
                                                       QPainter &painter) const
{
    m_searchResultOverlay->clear();
    if (m_searchExpr.pattern().isEmpty() || !m_searchExpr.isValid())
        return;

    const int margin = 5;
    QTextBlock block = data.block;
    QPointF offset = data.offset;
    while (block.isValid()) {
        QRectF blockBoundingRect = q->blockBoundingRect(block).translated(offset);

        if (blockBoundingRect.bottom() >= data.eventRect.top() - margin
                && blockBoundingRect.top() <= data.eventRect.bottom() + margin) {
            highlightSearchResults(block, data);
        }
        offset.ry() += blockBoundingRect.height();

        if (offset.y() > data.viewportRect.height() + margin)
            break;

        block = TextEditor::nextVisibleBlock(block, data.doc);
    }

    m_searchResultOverlay->fill(&painter,
                                data.searchResultFormat.background().color(),
                                data.eventRect);
}


void TextEditorWidgetPrivate::paintSelectionOverlay(const PaintEventData &data,
                                                    QPainter &painter) const
{
    if (m_cursors.hasMultipleCursors())
        return;
    const QString expr = m_cursors.selectedText();
    if (expr.isEmpty())
        return;

    const int margin = 5;
    QTextBlock block = data.block;
    QPointF offset = data.offset;
    while (block.isValid()) {
        QRectF blockBoundingRect = q->blockBoundingRect(block).translated(offset);

        if (blockBoundingRect.bottom() >= data.eventRect.top() - margin
            && blockBoundingRect.top() <= data.eventRect.bottom() + margin) {
            highlightSelection(block);
        }
        offset.ry() += blockBoundingRect.height();

        if (offset.y() > data.viewportRect.height() + margin)
            break;

        block = TextEditor::nextVisibleBlock(block, data.doc);
    }

    QColor selection = m_document->fontSettings().toTextCharFormat(C_SELECTION).background().color();
    const QColor text = m_document->fontSettings().toTextCharFormat(C_TEXT).background().color();
    selection.setAlphaF(0.4);

    m_selectionHighlightOverlay->fill(&painter, selection, data.eventRect);
}


void TextEditorWidgetPrivate::paintIfDefedOutBlocks(const PaintEventData &data,
                                                    QPainter &painter) const
{
    QTextBlock block = data.block;
    QPointF offset = data.offset;
    while (block.isValid()) {

        QRectF r = q->blockBoundingRect(block).translated(offset);

        if (r.bottom() >= data.eventRect.top() && r.top() <= data.eventRect.bottom()) {
            if (TextDocumentLayout::ifdefedOut(block)) {
                QRectF rr = r;
                rr.setRight(data.viewportRect.width() - offset.x());
                if (data.rightMargin > 0)
                    rr.setRight(qMin(data.rightMargin, rr.right()));
                painter.fillRect(rr, data.ifdefedOutFormat.background());
            }
        }
        offset.ry() += r.height();

        if (offset.y() > data.viewportRect.height())
            break;

        block = TextEditor::nextVisibleBlock(block, data.doc);
    }
}


void TextEditorWidgetPrivate::paintFindScope(const PaintEventData &data, QPainter &painter) const
{
    if (m_findScope.isNull())
        return;
    auto overlay = new TextEditorOverlay(q);
    for (const QTextCursor &c : m_findScope) {
        overlay->addOverlaySelection(c.selectionStart(),
                                     c.selectionEnd(),
                                     data.searchScopeFormat.foreground().color(),
                                     data.searchScopeFormat.background().color(),
                                     TextEditorOverlay::ExpandBegin);
    }
    overlay->setAlpha(false);
    overlay->paint(&painter, data.eventRect);
    delete overlay;
}


void TextEditorWidgetPrivate::paintCurrentLineHighlight(const PaintEventData &data,
                                                        QPainter &painter) const
{
    if (!m_highlightCurrentLine)
        return;

    QList<QTextCursor> cursorsForBlock;
    for (const QTextCursor &c : m_cursors) {
        if (c.block() == data.block)
            cursorsForBlock << c;
    }
    if (cursorsForBlock.isEmpty())
        return;

    const QRectF blockRect = q->blockBoundingRect(data.block).translated(data.offset);
    QColor color = m_document->fontSettings().toTextCharFormat(C_CURRENT_LINE).background().color();
    color.setAlpha(128);
    QSet<int> seenLines;
    for (const QTextCursor &cursor : cursorsForBlock) {
        QTextLine line = data.block.layout()->lineForTextPosition(cursor.positionInBlock());
        if (seenLines.contains(line.lineNumber()))
            continue;
        seenLines << line.lineNumber();
        QRectF lineRect = line.rect();
        lineRect.moveTop(lineRect.top() + blockRect.top());
        lineRect.setLeft(0);
        lineRect.setRight(data.viewportRect.width());
        painter.fillRect(lineRect, color);
    }
}


void TextEditorWidgetPrivate::paintCursorAsBlock(const PaintEventData &data, QPainter &painter,
                                                 PaintEventBlockData &blockData, int cursorPosition) const
{
    const QFontMetricsF fontMetrics(blockData.layout->font());
    int relativePos = cursorPosition - blockData.position;
    bool doSelection = true;
    QTextLine line = blockData.layout->lineForTextPosition(relativePos);
    qreal x = line.cursorToX(relativePos);
    qreal w = 0;
    if (relativePos < line.textLength() - line.textStart()) {
        w = line.cursorToX(relativePos + 1) - x;
        if (data.doc->characterAt(cursorPosition) == QLatin1Char('\t')) {
            doSelection = false;
            qreal space = fontMetrics.horizontalAdvance(QLatin1Char(' '));
            if (w > space) {
                x += w-space;
                w = space;
            }
        }
    } else
        w = fontMetrics.horizontalAdvance(QLatin1Char(' ')); // in sync with QTextLine::draw()

    QRectF lineRect = line.rect();
    lineRect.moveTop(lineRect.top() + blockData.boundingRect.top());
    lineRect.moveLeft(blockData.boundingRect.left() + x);
    lineRect.setWidth(w);
    const QTextCharFormat textFormat = data.fontSettings.toTextCharFormat(C_TEXT);
    painter.fillRect(lineRect, textFormat.foreground());
    if (doSelection) {
        blockData.selections.append(
            createBlockCursorCharFormatRange(relativePos,
                                             textFormat.foreground().color(),
                                             textFormat.background().color()));
    }
}


void TextEditorWidgetPrivate::paintAdditionalVisualWhitespaces(PaintEventData &data,
                                                               QPainter &painter,
                                                               qreal top) const
{
    if (!m_displaySettings.m_visualizeWhitespace)
        return;

    QTextLayout *layout = data.block.layout();
    const bool nextBlockIsValid = data.block.next().isValid();
    int lineCount = layout->lineCount();
    if (lineCount >= 2 || !nextBlockIsValid) {
        painter.save();
        painter.setPen(data.visualWhitespaceFormat.foreground().color());
        for (int i = 0; i < lineCount-1; ++i) { // paint line wrap indicator
            QTextLine line = layout->lineAt(i);
            QRectF lineRect = line.naturalTextRect().translated(data.offset.x(), top);
            QChar visualArrow(ushort(0x21b5));
            painter.drawText(QPointF(lineRect.right(), lineRect.top() + line.ascent()),
                             visualArrow);
        }
        if (!nextBlockIsValid) { // paint EOF symbol
            if (TextSuggestion *suggestion = TextDocumentLayout::suggestion(data.block)) {
                const QTextBlock lastReplacementBlock = suggestion->replacementDocument()->lastBlock();
                for (QTextBlock block = suggestion->replacementDocument()->firstBlock();block != lastReplacementBlock && block.isValid();block = block.next()) {
                    top += suggestion->replacementDocument()->documentLayout()->blockBoundingRect(block).height();
                }
                layout = lastReplacementBlock.layout();
                lineCount = layout->lineCount();
            }

            QTextLine line = layout->lineAt(lineCount-1);
            QRectF lineRect = line.naturalTextRect().translated(data.offset.x(), top);
            int h = 4;
            lineRect.adjust(0, 0, -1, -1);
            QPainterPath path;
            QPointF pos(lineRect.topRight() + QPointF(h+4, line.ascent()));
            path.moveTo(pos);
            path.lineTo(pos + QPointF(-h, -h));
            path.lineTo(pos + QPointF(0, -2*h));
            path.lineTo(pos + QPointF(h, -h));
            path.closeSubpath();
            painter.setBrush(painter.pen().color());
            painter.drawPath(path);
        }
        painter.restore();
    }
}


int TextEditorWidgetPrivate::indentDepthForBlock(const QTextBlock &block)
{
    const TabSettings &tabSettings = m_document->tabSettings();
    const auto blockDepth = [&](const QTextBlock &block) {
        int depth = m_visualIndentCache.value(block.blockNumber(), -1);
        if (depth < 0) {
            const QString text = block.text().mid(m_visualIndentOffset);
            depth = text.simplified().isEmpty() ? -1 : tabSettings.indentationColumn(text);
        }
        return depth;
    };
    const auto ensureCacheSize = [&](const int size) {
        int count = m_visualIndentCache.size();
        if (count < size){
            for(int i=count;i<size;i++){
                m_visualIndentCache.append(-1);
            }
        }
    };
    int depth = blockDepth(block);
    if (depth < 0) // the block was empty and uncached ask the indenter for a visual indentation
        depth = m_document->indenter()->visualIndentFor(block, tabSettings);
    if (depth >= 0) {
        ensureCacheSize(block.blockNumber() + 1);
        m_visualIndentCache[block.blockNumber()] = depth;
    } else {
        // find previous non empty block and get the indent depth of this block
        QTextBlock it = block.previous();
        int prevDepth = -1;
        while (it.isValid()) {
            prevDepth = blockDepth(it);
            if (prevDepth >= 0)
                break;
            it = it.previous();
        }
        const int startBlockNumber = it.isValid() ? it.blockNumber() + 1 : 0;

        // find next non empty block and get the indent depth of this block
        it = block.next();
        int nextDepth = -1;
        while (it.isValid()) {
            nextDepth = blockDepth(it);
            if (nextDepth >= 0)
                break;
            it = it.next();
        }
        const int endBlockNumber = it.isValid() ? it.blockNumber() : m_blockCount;

        // get the depth for the whole range of empty blocks and fill the cache so we do not need to
        // redo this for every paint event
        depth = prevDepth > 0 && nextDepth > 0 ? qMin(prevDepth, nextDepth) : 0;
        ensureCacheSize(endBlockNumber);
        for (int i = startBlockNumber; i < endBlockNumber; ++i)
            m_visualIndentCache[i] = depth;
    }
    return depth;
}


void TextEditorWidgetPrivate::paintIndentDepth(PaintEventData &data,
                                               QPainter &painter,
                                               const PaintEventBlockData &blockData)
{
    if (!m_displaySettings.m_visualizeIndent)
        return;

    const int depth = indentDepthForBlock(data.block);
    if (depth <= 0 || blockData.layout->lineCount() < 1)
        return;

    const TabSettings &tabSettings = m_document->tabSettings();
    const qreal horizontalAdvance = QFontMetricsF(q->font()).horizontalAdvance(' ');
    const qreal fullHorizontalAdvance = horizontalAdvance * tabSettings.m_indentSize;
    painter.save();
    painter.setPen(data.visualWhitespaceFormat.foreground().color());
    //painter.setPen(Qt::lightGray);//to do fixed

    const QTextLine textLine = blockData.layout->lineAt(0);
    const QRectF rect = textLine.naturalTextRect();
    qreal x = textLine.x() + data.offset.x() + qMax(0, q->cursorWidth() - 1)
            + horizontalAdvance * m_visualIndentOffset;
    int paintColumn = 0;
    const QString text = data.block.text().mid(m_visualIndentOffset);
    while (paintColumn < depth) {
        if (x >= 0) {
            int paintPosition = tabSettings.positionAtColumn(text, paintColumn);
            if (q->lineWrapMode() == QPlainTextEdit::WidgetWidth
                && blockData.layout->lineForTextPosition(paintPosition).lineNumber() != 0) {
                break;
            }
            const QPointF top(x, blockData.boundingRect.top());
            const QPointF bottom(x, blockData.boundingRect.top() + rect.height());
            const QLineF line(top, bottom);
            painter.drawLine(line);
        }
        x += fullHorizontalAdvance;
        paintColumn += tabSettings.m_indentSize;
    }
    painter.restore();
}


void TextEditorWidgetPrivate::paintReplacement(PaintEventData &data, QPainter &painter,
                                               qreal top) const
{
    QTextBlock nextBlock = data.block.next();

    if (nextBlock.isValid() && !nextBlock.isVisible() && q->replacementVisible(data.block.blockNumber())) {
        const bool selectThis = (data.textCursor.hasSelection()
                                 && nextBlock.position() >= data.textCursor.selectionStart()
                                 && nextBlock.position() < data.textCursor.selectionEnd());


        const QTextCharFormat selectionFormat = data.fontSettings.toTextCharFormat(C_SELECTION);

        painter.save();
        if (selectThis) {
            painter.setBrush(selectionFormat.background().style() != Qt::NoBrush
                                 ? selectionFormat.background()
                                 : QApplication::palette().brush(QPalette::Highlight));
        } else {
            QColor rc = q->replacementPenColor(data.block.blockNumber());
            if (rc.isValid())
                painter.setPen(rc);
        }

        QTextLayout *layout = data.block.layout();
        QTextLine line = layout->lineAt(layout->lineCount()-1);
        QRectF lineRect = line.naturalTextRect().translated(data.offset.x(), top);
        lineRect.adjust(0, 0, -1, -1);

        QString replacement = q->foldReplacementText(data.block);
        QString rectReplacement = QLatin1String(" {") + replacement + QLatin1String("}; ");

        QRectF collapseRect(lineRect.right() + 12,
                            lineRect.top(),
                            q->fontMetrics().horizontalAdvance(rectReplacement),
                            lineRect.height());
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.translate(.5, .5);
        painter.drawRoundedRect(collapseRect.adjusted(0, 0, 0, -1), 3, 3);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.translate(-.5, -.5);

        if (TextBlockUserData *nextBlockUserData = TextDocumentLayout::textUserData(nextBlock)) {
            if (nextBlockUserData->foldingStartIncluded())
                replacement.prepend(nextBlock.text().trimmed().at(0));
        }

        QTextBlock lastInvisibleBlock = TextEditor::nextVisibleBlock(data.block, data.doc).previous();
        if (!lastInvisibleBlock.isValid())
            lastInvisibleBlock = data.doc->lastBlock();

        if (TextBlockUserData *blockUserData = TextDocumentLayout::textUserData(lastInvisibleBlock)) {
            if (blockUserData->foldingEndIncluded()) {
                QString right = lastInvisibleBlock.text().trimmed();
                if (right.endsWith(QLatin1Char(';'))) {
                    right.chop(1);
                    right = right.trimmed();
                    replacement.append(right.right(right.endsWith('/') ? 2 : 1));
                    replacement.append(QLatin1Char(';'));
                } else {
                    replacement.append(right.right(right.endsWith('/') ? 2 : 1));
                }
            }
        }

        if (selectThis)
            painter.setPen(selectionFormat.foreground().color());
        painter.drawText(collapseRect, Qt::AlignCenter, replacement);
        painter.restore();
    }
}


void TextEditorWidgetPrivate::paintWidgetBackground(const PaintEventData &data,
                                                    QPainter &painter) const
{
    painter.fillRect(data.eventRect, data.fontSettings.toTextCharFormat(C_TEXT).background());
}


void TextEditorWidgetPrivate::paintOverlays(const PaintEventData &data, QPainter &painter) const
{
    // draw the overlays, but only if we do not have a find scope, otherwise the
    // view becomes too noisy.
    if (m_findScope.isNull()) {
        if (m_overlay->isVisible())
            m_overlay->paint(&painter, data.eventRect);

        if (m_snippetOverlay->isVisible())
            m_snippetOverlay->paint(&painter, data.eventRect);

        if (!m_refactorOverlay->isEmpty())
            m_refactorOverlay->paint(&painter, data.eventRect);
    }

    if (!m_searchResultOverlay->isEmpty()) {
        m_searchResultOverlay->paint(&painter, data.eventRect);
        m_searchResultOverlay->clear();
    }
}


void TextEditorWidgetPrivate::paintCursor(const PaintEventData &data, QPainter &painter) const
{
    for (const CursorData &cursor : data.cursors) {
        painter.setPen(cursor.pen);
        cursor.layout->drawCursor(&painter, cursor.offset, cursor.pos, q->cursorWidth());
    }
}


void TextEditorWidgetPrivate::setupBlockLayout(const PaintEventData &data,
                                               QPainter &painter,
                                               PaintEventBlockData &blockData) const
{
    blockData.layout = data.block.layout();

    QTextOption option = blockData.layout->textOption();
    if (data.suppressSyntaxInIfdefedOutBlock && TextDocumentLayout::ifdefedOut(data.block)) {
        option.setFlags(option.flags() | QTextOption::SuppressColors);
        painter.setPen(data.ifdefedOutFormat.foreground().color());
    } else {
        option.setFlags(option.flags() & ~QTextOption::SuppressColors);
        painter.setPen(data.context.palette.text().color());
    }
    blockData.layout->setTextOption(option);
    blockData.layout->setFont(data.doc->defaultFont());
}


void TextEditorWidgetPrivate::setupSelections(const PaintEventData &data,
                                              PaintEventBlockData &blockData) const
{
    QVector<QTextLayout::FormatRange> prioritySelections;
    int deltaPos = -1;
    int delta = 0;
    if (TextSuggestion *suggestion = TextDocumentLayout::suggestion(data.block)) {
        deltaPos = suggestion->currentPosition() - data.block.position();
        const QString trailingText = data.block.text().mid(deltaPos);
        if (!trailingText.isEmpty()) {
            const int trailingIndex = suggestion->replacementDocument()->firstBlock().text().indexOf(trailingText, deltaPos);
            if (trailingIndex >= 0)
                delta = std::max(trailingIndex - deltaPos, 0);
        }
    }


    for (int i = 0; i < data.context.selections.size(); ++i) {
        const QAbstractTextDocumentLayout::Selection &range = data.context.selections.at(i);
        const int selStart = range.cursor.selectionStart() - blockData.position;
        const int selEnd = range.cursor.selectionEnd() - blockData.position;
        if (selStart < blockData.length && selEnd >= 0  && selEnd >= selStart) {
            QTextLayout::FormatRange o;
            o.start = selStart;
            o.length = selEnd - selStart;
            o.format = range.format;
            QTextLayout::FormatRange rest;
            rest.start = -1;
            if (deltaPos >= 0 && delta != 0) {
                if (o.start >= deltaPos) {
                    o.start += delta;
                } else if (o.start + o.length > deltaPos) {
                    // the format range starts before and ends after the position so we need to
                    // split the format into before and after the suggestion format ranges
                    rest.start = deltaPos + delta;
                    rest.length = o.length - (deltaPos - o.start);
                    rest.format = o.format;
                    o.length = deltaPos - o.start;
                }
            }

            o.format = range.format;
            if (data.textCursor.hasSelection() && data.textCursor == range.cursor && data.textCursor.anchor() == range.cursor.anchor()) {
                const QTextCharFormat selectionFormat = data.fontSettings.toTextCharFormat(C_SELECTION);
                if (selectionFormat.background().style() != Qt::NoBrush)
                    o.format.setBackground(selectionFormat.background());
                o.format.setForeground(selectionFormat.foreground());
            }
            if ((data.textCursor.hasSelection() && i == data.context.selections.size() - 1) || (o.format.foreground().style() == Qt::NoBrush
                    && o.format.underlineStyle() != QTextCharFormat::NoUnderline
                    && o.format.background() == Qt::NoBrush)) {
                    if (q->selectionVisible(data.block.blockNumber())) {
                        prioritySelections.append(o);
                        if (rest.start >= 0)
                            prioritySelections.append(rest);
                    }
                } else {
                    blockData.selections.append(o);
                    if (rest.start >= 0)
                        blockData.selections.append(rest);
                }
            }
        }
        blockData.selections.append(prioritySelections);
}


static CursorData generateCursorData(const int cursorPos,
                                     const PaintEventData &data,
                                     const PaintEventBlockData &blockData,
                                     QPainter &painter)
{
    CursorData cursorData;
    cursorData.layout = blockData.layout;
    cursorData.offset = data.offset;
    cursorData.pos = cursorPos;
    cursorData.pen = painter.pen();
    return cursorData;
}


static bool blockContainsCursor(const PaintEventBlockData &blockData, const QTextCursor &cursor)
{
    const int pos = cursor.position();
    return pos >= blockData.position && pos < blockData.position + blockData.length;
}


void TextEditorWidgetPrivate::addCursorsPosition(PaintEventData &data,
                                                 QPainter &painter,
                                                 const PaintEventBlockData &blockData) const
{

    if (!m_dndCursor.isNull()) {
        if (blockContainsCursor(blockData, m_dndCursor)) {
            data.cursors.append(
                generateCursorData(m_dndCursor.positionInBlock(), data, blockData, painter));
        }
    } else {
        for (const QTextCursor &cursor : m_cursors) {
            if (blockContainsCursor(blockData, cursor)) {
                data.cursors.append(
                    generateCursorData(cursor.positionInBlock(), data, blockData, painter));
            }
        }
    }
}


QTextBlock TextEditorWidgetPrivate::nextVisibleBlock(const QTextBlock &block) const
{
    return TextEditor::nextVisibleBlock(block, q->document());
}


void TextEditorWidgetPrivate::cleanupAnnotationCache()
{
    const int firstVisibleBlock = q->firstVisibleBlockNumber();
    const int lastVisibleBlock = q->lastVisibleBlockNumber();
    auto lineIsVisble = [&](int blockNumber){
        auto behindFirstVisibleBlock = [&](){
            return firstVisibleBlock >= 0 && blockNumber >= firstVisibleBlock;
        };
        auto beforeLastVisibleBlock = [&](){
            return lastVisibleBlock < 0 ||  (lastVisibleBlock >= 0 && blockNumber <= lastVisibleBlock);
        };
        return behindFirstVisibleBlock() && beforeLastVisibleBlock();
    };
    auto it = m_annotationRects.begin();
    auto end = m_annotationRects.end();
    while (it != end) {
        if (!lineIsVisble(it.key()))
            it = m_annotationRects.erase(it);
        else
            ++it;
    }
}


void TextEditorWidget::paintEvent(QPaintEvent *e)
{
    PaintEventData data(this, e, contentOffset());
    QTC_ASSERT(data.documentLayout, return);

    QPainter painter(viewport());
    // Set a brush origin so that the WaveUnderline knows where the wave started
    painter.setBrushOrigin(data.offset);

    data.block = firstVisibleBlock();
    data.context = getPaintContext();
    const QTextCharFormat textFormat = textDocument()->fontSettings().toTextCharFormat(C_TEXT);
    //qDebug()<<"textFormat"<<textFormat.font();
    data.context.palette.setBrush(QPalette::Text, textFormat.foreground());
    data.context.palette.setBrush(QPalette::Base, textFormat.background());
    painter.setFont(textFormat.font());
    { // paint background
        d->paintWidgetBackground(data, painter);
        // draw backgrond to the right of the wrap column before everything else
        d->paintRightMarginArea(data, painter);
        // paint a blended background color depending on scope depth
        d->paintBlockHighlight(data, painter);
        // paint background of if defed out blocks in bigger chunks
        d->paintIfDefedOutBlocks(data, painter);
        d->paintRightMarginLine(data, painter);
        // paint find scope on top of ifdefed out blocks and right margin
        d->paintFindScope(data, painter);
        // paint search results on top of the find scope
        d->paintSearchResultOverlay(data, painter);
        // paint selection highlights
        d->paintSelectionOverlay(data, painter);
    }

    while (data.block.isValid()) {
        PaintEventBlockData blockData;
        blockData.boundingRect = blockBoundingRect(data.block).translated(data.offset);
        if (blockData.boundingRect.bottom() >= data.eventRect.top()
                && blockData.boundingRect.top() <= data.eventRect.bottom()) {

            d->setupBlockLayout(data, painter, blockData);
            blockData.position = data.block.position();
            blockData.length = data.block.length();
            d->setupSelections(data, blockData);

            d->paintCurrentLineHighlight(data, painter);
            bool drawCursor = false;
            bool drawCursorAsBlock = false;
            if (d->m_dndCursor.isNull()) {
                drawCursor = d->m_cursorVisible
                             && Utils::anyOf(d->m_cursors, [&](const QTextCursor &cursor) {
                                    return blockContainsCursor(blockData, cursor);
                                });
                drawCursorAsBlock = drawCursor && overwriteMode();
            } else {
                drawCursor = blockContainsCursor(blockData, d->m_dndCursor);
            }

            if (drawCursorAsBlock) {
                for (const QTextCursor &cursor : multiTextCursor()) {
                    if (blockContainsCursor(blockData, cursor))
                        d->paintCursorAsBlock(data, painter, blockData, cursor.position());
                }
            }
            paintBlock(&painter, data.block, data.offset, blockData.selections, data.eventRect);
            if (data.isEditable && !blockData.layout->preeditAreaText().isEmpty()) {
                if (data.context.cursorPosition < -1) {
                    const int cursorPos = blockData.layout->preeditAreaPosition()
                                                      - (data.context.cursorPosition + 2);
                    data.cursors = {generateCursorData(cursorPos, data, blockData, painter)};
                }
            } else if (drawCursor && !drawCursorAsBlock) {
                d->addCursorsPosition(data, painter, blockData);
            }
            d->paintIndentDepth(data, painter, blockData);
            d->paintAdditionalVisualWhitespaces(data, painter, blockData.boundingRect.top());
            d->paintReplacement(data, painter, blockData.boundingRect.top());
            d->updateLineAnnotation(data, blockData, painter);
        }

        data.offset.ry() += blockData.boundingRect.height();

        if (data.offset.y() > data.viewportRect.height())
            break;

        data.block = data.block.next();

        if (!data.block.isVisible()) {
            if (data.block.blockNumber() == d->visibleFoldedBlockNumber) {
                data.visibleCollapsedBlock = data.block;
                data.visibleCollapsedBlockOffset = data.offset;
            }

            // invisible blocks do have zero line count
            data.block = data.doc->findBlockByLineNumber(data.block.firstLineNumber());
        }
    }
    d->cleanupAnnotationCache();

    painter.setPen(data.context.palette.text().color());

    d->updateAnimator(d->m_bracketsAnimator, painter);
    d->updateAnimator(d->m_autocompleteAnimator, painter);

    d->paintOverlays(data, painter);

    // paint diff overlay (for inline diff display)
    paintDiffOverlay(&painter, data.eventRect);

    // draw the cursor last, on top of everything
    d->paintCursor(data, painter);
    // paint a popup with the content of the collapsed block
    drawCollapsedBlockPopup(painter, data.visibleCollapsedBlock,data.visibleCollapsedBlockOffset, data.eventRect);
}


void TextEditorWidget::paintBlock(QPainter *painter,
                                  const QTextBlock &block,
                                  const QPointF &offset,
                                  const QVector<QTextLayout::FormatRange> &selections,
                                  const QRect &clipRect) const
{
    if (TextSuggestion *suggestion = TextDocumentLayout::suggestion(block)) {
        QTextBlock suggestionBlock = suggestion->replacementDocument()->firstBlock();
        //qDebug()<<"suggestionBlock"<<suggestionBlock.text();
        QPointF suggestionOffset = offset;

        suggestionOffset.rx() += document()->documentMargin();
        //qDebug()<<"suggestionOffset"<<suggestionOffset<<selections.length()<<suggestionBlock.blockNumber();
        while (suggestionBlock.isValid()) {
            const QVector<QTextLayout::FormatRange> blockSelections
                = suggestionBlock.blockNumber() == 0 ? selections
                                                      : QVector<QTextLayout::FormatRange>{};
            //qDebug()<<"clipRect"<<clipRect<<blockSelections.size();
            suggestionBlock.layout()->draw(painter, suggestionOffset, blockSelections, clipRect);
            suggestionOffset.ry() += suggestion->replacementDocument()
                                         ->documentLayout()
                                         ->blockBoundingRect(suggestionBlock)
                                         .height();
            suggestionBlock = suggestionBlock.next();
        }
        return;
    }
    block.layout()->draw(painter, offset, selections, clipRect);
}


void TextEditorWidget::paintDiffOverlay(QPainter *painter, const QRect &clip)
{
    Q_UNUSED(painter);
    Q_UNUSED(clip);
    // Default implementation does nothing
    // Subclasses can override this to paint custom diff overlays
}


void TextEditorWidgetPrivate::_q_animateUpdate(const QTextCursor &cursor,
                                               QPointF lastPos, QRectF rect)
{
    q->viewport()->update(QRectF(q->cursorRect(cursor).topLeft() + rect.topLeft(), rect.size()).toAlignedRect());
    if (!lastPos.isNull())
        q->viewport()->update(QRectF(lastPos + rect.topLeft(), rect.size()).toAlignedRect());
}



TextEditorAnimator::TextEditorAnimator(QObject *parent)
    : QObject(parent), m_timeline(256)
{
    m_value = 0;
    m_timeline.setEasingCurve(QEasingCurve::SineCurve);
    connect(&m_timeline, &QTimeLine::valueChanged, this, &TextEditorAnimator::step);
    connect(&m_timeline, &QTimeLine::finished, this, &QObject::deleteLater);
    m_timeline.start();
}


void TextEditorAnimator::init(const QTextCursor &cursor, const QFont &f, const QPalette &pal)
{
    m_cursor = cursor;
    m_font = f;
    m_palette = pal;
    m_text = cursor.selectedText();
    QFontMetrics fm(m_font);
    m_size = QSizeF(fm.horizontalAdvance(m_text), fm.height());
}


void TextEditorAnimator::draw(QPainter *p, const QPointF &pos)
{
    m_lastDrawPos = pos;
    p->setPen(m_palette.text().color());
    QFont f = m_font;
    f.setPointSizeF(f.pointSizeF() * (1.0 + m_value/2));
    QFontMetrics fm(f);
    int width = fm.horizontalAdvance(m_text);
    QRectF r((m_size.width()-width)/2, (m_size.height() - fm.height())/2, width, fm.height());
    r.translate(pos);
    p->fillRect(r, m_palette.base());
    p->setFont(f);
    p->drawText(r, m_text);
}


bool TextEditorAnimator::isRunning() const
{
    return m_timeline.state() == QTimeLine::Running;
}


QRectF TextEditorAnimator::rect() const
{
    QFont f = m_font;
    f.setPointSizeF(f.pointSizeF() * (1.0 + m_value/2));
    QFontMetrics fm(f);
    int width = fm.horizontalAdvance(m_text);
    return QRectF((m_size.width()-width)/2, (m_size.height() - fm.height())/2, width, fm.height());
}


void TextEditorAnimator::step(qreal v)
{
    QRectF before = rect();
    m_value = v;
    QRectF after = rect();
    emit updateRequest(m_cursor, m_lastDrawPos, before.united(after));
}


void TextEditorAnimator::finish()
{
    m_timeline.stop();
    step(0);
    deleteLater();
}


void TextEditorWidgetPrivate::updateAnimator(QPointer<TextEditorAnimator> animator,
                                             QPainter &painter)
{
    if (animator && animator->isRunning())
        animator->draw(&painter, q->cursorRect(animator->cursor()).topLeft());
}


void TextEditorWidgetPrivate::cancelCurrentAnimations()
{
    if (m_autocompleteAnimator)
        m_autocompleteAnimator->finish();
    if (m_bracketsAnimator)
        m_bracketsAnimator->finish();
}


} // namespace TextEditor

