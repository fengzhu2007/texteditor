// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
//
// v2 split of texteditor.cpp (legacy file kept at ../texteditor.cpp).
// Find: find/replace, search result highlighting, scrollbar highlights.
//

#include "texteditorwidget_p.h"

namespace TextEditor {

using namespace Internal;

QFutureWatcher<FileSearchResultList> *TextEditorWidgetFind::m_selectWatcher = nullptr;


void TextEditorWidgetFind::selectAll(const QString &txt, FindFlags findFlags)
{
    if (txt.isEmpty())
        return;

    cancelCurrentSelectAll();

    m_selectWatcher = new QFutureWatcher<FileSearchResultList>();
    connect(m_selectWatcher, &QFutureWatcher<Utils::FileSearchResultList>::finished,
            this, [this] {
                const QFuture<FileSearchResultList> future = m_selectWatcher->future();
                m_selectWatcher->deleteLater();
                m_selectWatcher = nullptr;
                if (future.resultCount() <= 0)
                    return;
                const FileSearchResultList &results = future.result();
                const QTextCursor c(m_editor->document());
                auto cursorForResult = [c](const FileSearchResult &r) {
                    return Utils::Text::selectAt(c, r.lineNumber, r.matchStart + 1, r.matchLength);
                };
                QList<QTextCursor> cursors = Utils::transform(results, cursorForResult);
                cursors = Utils::filtered(cursors, [this](const QTextCursor &c) {
                    return m_editor->inFindScope(c);
                });
                m_editor->setMultiTextCursor(MultiTextCursor(cursors));
                m_editor->setFocus();
            });

    const FilePath &fileName = m_editor->textDocument()->filePath();
    QMap<FilePath, QString> fileToContentsMap;
    fileToContentsMap[fileName] = m_editor->textDocument()->plainText();

    FileListIterator *it = new FileListIterator({fileName},
                                                {const_cast<QTextCodec *>(
                                                    m_editor->textDocument()->codec())});
    const QTextDocument::FindFlags findFlags2 = textDocumentFlagsForFindFlags(findFlags);

    if (findFlags & FindRegularExpression)
        m_selectWatcher->setFuture(findInFilesRegExp(txt, it, findFlags2, fileToContentsMap));
    else
        m_selectWatcher->setFuture(findInFiles(txt, it, findFlags2, fileToContentsMap));
}


void TextEditorWidgetFind::cancelCurrentSelectAll()
{
    if (m_selectWatcher) {
        m_selectWatcher->disconnect();
        m_selectWatcher->cancel();
        m_selectWatcher->deleteLater();
        m_selectWatcher = nullptr;
    }
}


void TextEditorWidgetPrivate::setupScrollBar()
{
    if (m_displaySettings.m_scrollBarHighlights) {
        if (!m_highlightScrollBarController)
            m_highlightScrollBarController = new HighlightScrollBarController();

        m_highlightScrollBarController->setScrollArea(q);
        highlightSearchResultsInScrollBar();
        scheduleUpdateHighlightScrollBar();
    } else if (m_highlightScrollBarController) {
        delete m_highlightScrollBarController;
        m_highlightScrollBarController = nullptr;
    }

}


Core::BaseTextFind* TextEditorWidget::finder(){
    return d->m_find;
}


bool TextEditorWidget::findText(const QString& text,int flags,bool hightlight){
    Core::FindFlags findFlags;
    if((flags & FindBackward)>0){
        findFlags.setFlag(FindBackward,true);
    }
    if((flags & FindCaseSensitively)>0){
        findFlags.setFlag(FindCaseSensitively,true);
    }
    if((flags & FindWholeWords)>0){
        findFlags.setFlag(FindWholeWords,true);
    }
    if((flags & FindRegularExpression)>0){
        findFlags.setFlag(FindRegularExpression,true);
    }
    if((flags & FindPreserveCase)>0){
        findFlags.setFlag(FindPreserveCase,true);
    }
    if(hightlight)
        d->m_find->highlightAllRequested(text,findFlags);
    return d->m_find->findStep(text,findFlags)==Core::IFindSupport::Found;
}


void TextEditorWidget::replaceText(const QString& before,const QString& after,int flags,bool hightlight){
    Core::FindFlags findFlags;
    if((flags & FindBackward)>0){
        findFlags.setFlag(FindBackward,true);
    }
    if((flags & FindCaseSensitively)>0){
        findFlags.setFlag(FindCaseSensitively,true);
    }
    if((flags & FindWholeWords)>0){
        findFlags.setFlag(FindWholeWords,true);
    }
    if((flags & FindRegularExpression)>0){
        findFlags.setFlag(FindRegularExpression,true);
    }
    if((flags & FindPreserveCase)>0){
        findFlags.setFlag(FindPreserveCase,true);
    }
    if(hightlight)
        d->m_find->highlightAllRequested(before,findFlags);
    d->m_find->replaceStep(before,after,findFlags);
}


int TextEditorWidget::replaceAll(const QString& before,const QString& after,int flags){
    Core::FindFlags findFlags;
    if((flags & FindBackward)>0){
        findFlags.setFlag(FindBackward,true);
    }
    if((flags & FindCaseSensitively)>0){
        findFlags.setFlag(FindCaseSensitively,true);
    }
    if((flags & FindWholeWords)>0){
        findFlags.setFlag(FindWholeWords,true);
    }
    if((flags & FindRegularExpression)>0){
        findFlags.setFlag(FindRegularExpression,true);
    }
    if((flags & FindPreserveCase)>0){
        findFlags.setFlag(FindPreserveCase,true);
    }
    return d->m_find->replaceAll(before,after,findFlags);
}


void TextEditorWidget::clearHighlights(){
    d->m_find->clearHighlights();
}


void TextEditorWidgetPrivate::highlightSearchResults(const QTextBlock &block, const PaintEventData &data) const
{
    if (m_searchExpr.pattern().isEmpty())
        return;

    int blockPosition = block.position();

    QTextCursor cursor = q->textCursor();
    QString text = block.text();
    text.replace(QChar::Nbsp, QLatin1Char(' '));
    int idx = -1;
    int l = 0;

    const int left = data.viewportRect.left() - int(data.offset.x());
    const int right = data.viewportRect.right() - int(data.offset.x());
    const int top = data.viewportRect.top() - int(data.offset.y());
    const int bottom = data.viewportRect.bottom() - int(data.offset.y());
    const QColor &searchResultColor = m_document->fontSettings()
            .toTextCharFormat(C_SEARCH_RESULT).background().color().darker(120);

    while (idx < text.length()) {
        const QRegularExpressionMatch match = m_searchExpr.match(text, idx + l + 1);
        if (!match.hasMatch())
            break;
        idx = match.capturedStart();
        l = match.capturedLength();
        if (l == 0)
            break;
        if ((m_findFlags & FindWholeWords)
            && ((idx && text.at(idx-1).isLetterOrNumber())
                || (idx + l < text.length() && text.at(idx + l).isLetterOrNumber())))
            continue;

        const int start = blockPosition + idx;
        const int end = start + l;
        QTextCursor result = cursor;
        result.setPosition(start);
        result.setPosition(end, QTextCursor::KeepAnchor);
        if (!q->inFindScope(result))
            continue;

        // check if the result is inside the visibale area for long blocks
        const QTextLine &startLine = block.layout()->lineForTextPosition(idx);
        const QTextLine &endLine = block.layout()->lineForTextPosition(idx + l);

        if (startLine.isValid() && endLine.isValid()
                && startLine.lineNumber() == endLine.lineNumber()) {
            const int lineY = int(endLine.y() + q->blockBoundingGeometry(block).y());
            if (startLine.cursorToX(idx) > right) { // result is behind the visible area
                if (endLine.lineNumber() >= block.lineCount() - 1)
                    break; // this is the last line in the block, nothing more to add

                // skip to the start of the next line
                idx = block.layout()->lineAt(endLine.lineNumber() + 1).textStart();
                l = 0;
                continue;
            } else if (endLine.cursorToX(idx + l, QTextLine::Trailing) < left) { // result is in front of the visible area skip it
                continue;
            } else if (lineY + endLine.height() < top) {
                if (endLine.lineNumber() >= block.lineCount() - 1)
                    break; // this is the last line in the block, nothing more to add
                // before visible area, skip to the start of the next line
                idx = block.layout()->lineAt(endLine.lineNumber() + 1).textStart();
                l = 0;
                continue;
            } else if (lineY > bottom) {
                break; // under the visible area, nothing more to add
            }
        }

        const uint flag = (idx == cursor.selectionStart() - blockPosition
                           && idx + l == cursor.selectionEnd() - blockPosition) ?
                    TextEditorOverlay::DropShadow : 0;
        m_searchResultOverlay->addOverlaySelection(start, end, searchResultColor, QColor(), flag);
    }
}



void TextEditorWidgetPrivate::highlightSelection(const QTextBlock &block) const
{
    if (!m_displaySettings.m_highlightSelection || m_cursors.hasMultipleCursors())
        return;
    const QString selection = m_cursors.selectedText();
    if (selection.trimmed().isEmpty())
        return;

    const int blockPosition = block.position();

    QString text = block.text();
    text.replace(QChar::Nbsp, QLatin1Char(' '));
    const int l = selection.length();

    for (int idx = text.indexOf(selection, 0, Qt::CaseInsensitive);
         idx >= 0;
         idx = text.indexOf(selection, idx + 1, Qt::CaseInsensitive)) {
        const int start = blockPosition + idx;
        const int end = start + l;
        if (!Utils::contains(m_selectionHighlightOverlay->selections(),
                             [&](const OverlaySelection &selection) {
                                 return selection.m_cursor_begin.position() == start
                                        && selection.m_cursor_end.position() == end;
                             })) {
            m_selectionHighlightOverlay->addOverlaySelection(start, end, {}, {});
        }
    }
}


void TextEditorWidgetPrivate::updateCurrentLineInScrollbar()
{
    if (m_highlightCurrentLine && m_highlightScrollBarController) {
        m_highlightScrollBarController->removeHighlights(Constants::SCROLL_BAR_CURRENT_LINE);
        for (const QTextCursor &tc : m_cursors) {
            if (QTextLayout *layout = tc.block().layout()) {
                const int pos = tc.block().firstLineNumber() +
                        layout->lineForTextPosition(tc.positionInBlock()).lineNumber();
                m_highlightScrollBarController->addHighlight({Constants::SCROLL_BAR_CURRENT_LINE, pos,
                                                              Theme::TextEditor_CurrentLine_ScrollBarColor,
                                                              Highlight::HighestPriority});
            }
        }
    }
}


void TextEditorWidgetPrivate::highlightSearchResultsSlot(const QString &txt, FindFlags findFlags)
{
    const QString pattern = (findFlags & FindRegularExpression) ? txt
                                                                : QRegularExpression::escape(txt);
    const QRegularExpression::PatternOptions options
        = (findFlags & FindCaseSensitively) ? QRegularExpression::NoPatternOption
                                            : QRegularExpression::CaseInsensitiveOption;
    if (m_searchExpr.pattern() == pattern && m_searchExpr.patternOptions() == options)
        return;
    m_searchExpr.setPattern(pattern);
    m_searchExpr.setPatternOptions(options);
    m_findText = txt;
    m_findFlags = findFlags;

    m_delayedUpdateTimer.start(50);

    if (m_highlightScrollBarController)
        m_scrollBarUpdateTimer.start(50);
}


void TextEditorWidgetPrivate::searchResultsReady(int beginIndex, int endIndex)
{
    QVector<SearchResult> results;
    for (int index = beginIndex; index < endIndex; ++index) {
        const FileSearchResultList resultList = m_searchWatcher->resultAt(index);
        for (FileSearchResult result : resultList) {
            const QTextBlock &block = q->document()->findBlockByNumber(result.lineNumber - 1);
            const int matchStart = block.position() + result.matchStart;
            QTextCursor cursor(block);
            cursor.setPosition(matchStart);
            cursor.setPosition(matchStart + result.matchLength, QTextCursor::KeepAnchor);
            if (!q->inFindScope(cursor))
                continue;
            results << SearchResult{matchStart, result.matchLength};
        }
    }
    m_searchResults << results;
    addSearchResultsToScrollBar(results);
}


void TextEditorWidgetPrivate::searchFinished()
{
    delete m_searchWatcher;
    m_searchWatcher = nullptr;
}



void TextEditorWidgetPrivate::selectionResultsReady(int beginIndex, int endIndex)
{
    QVector<SearchResult> results;
    for (int index = beginIndex; index < endIndex; ++index) {
        const FileSearchResultList resultList = m_selectionHighlightFuture->resultAt(index);
        for (FileSearchResult result : resultList) {
            const QTextBlock &block = q->document()->findBlockByNumber(result.lineNumber - 1);
            const int matchStart = block.position() + result.matchStart;
            QTextCursor cursor(block);
            cursor.setPosition(matchStart);
            cursor.setPosition(matchStart + result.matchLength, QTextCursor::KeepAnchor);
            if (!q->inFindScope(cursor))
                continue;
            results << SearchResult{matchStart, result.matchLength};
        }
    }
    m_selectionResults << results;
    addSelectionHighlightToScrollBar(results);
}


void TextEditorWidgetPrivate::selectionFinished()
{
    delete m_selectionHighlightFuture;
    m_selectionHighlightFuture = nullptr;
}




void TextEditorWidgetPrivate::adjustScrollBarRanges()
{
    if (!m_highlightScrollBarController)
        return;
    const double lineSpacing = TextEditorSettings::fontSettings().lineSpacing();
    if (lineSpacing == 0)
        return;

    m_highlightScrollBarController->setLineHeight(lineSpacing);
    m_highlightScrollBarController->setVisibleRange(q->viewport()->rect().height());
    m_highlightScrollBarController->setMargin(q->textDocument()->document()->documentMargin());
}


void TextEditorWidgetPrivate::highlightSearchResultsInScrollBar()
{
    if (!m_highlightScrollBarController)
        return;
    m_highlightScrollBarController->removeHighlights(Constants::SCROLL_BAR_SEARCH_RESULT);
    m_searchResults.clear();

    if (m_searchWatcher) {
        m_searchWatcher->disconnect();
        m_searchWatcher->cancel();
        m_searchWatcher->deleteLater();
        m_searchWatcher = nullptr;
    }

    const QString &txt = m_findText;
    if (txt.isEmpty())
        return;

    adjustScrollBarRanges();

    m_searchWatcher = new QFutureWatcher<FileSearchResultList>();
    connect(m_searchWatcher, &QFutureWatcher<FileSearchResultList>::resultsReadyAt,
            this, &TextEditorWidgetPrivate::searchResultsReady);
    connect(m_searchWatcher, &QFutureWatcher<FileSearchResultList>::finished,
            this, &TextEditorWidgetPrivate::searchFinished);
    m_searchWatcher->setPendingResultsLimit(10);

    const QTextDocument::FindFlags findFlags = textDocumentFlagsForFindFlags(m_findFlags);

    const FilePath &fileName = m_document->filePath();
    FileListIterator *it =
            new FileListIterator({fileName} , {const_cast<QTextCodec *>(m_document->codec())});
    QMap<FilePath, QString> fileToContentsMap;
    fileToContentsMap[fileName] = m_document->plainText();

    if (m_findFlags & FindRegularExpression)
        m_searchWatcher->setFuture(findInFilesRegExp(txt, it, findFlags, fileToContentsMap));
    else
        m_searchWatcher->setFuture(findInFiles(txt, it, findFlags, fileToContentsMap));
}


void TextEditorWidgetPrivate::highlightSelectionResultsInScrollBar(){
    if (!m_highlightScrollBarController)
        return;
    m_highlightScrollBarController->removeHighlights(Constants::SCROLL_BAR_SELECTION);
    m_selectionResults.clear();

    if (m_selectionHighlightFuture) {
        m_selectionHighlightFuture->disconnect();
        m_selectionHighlightFuture->cancel();
        m_selectionHighlightFuture->deleteLater();
        m_selectionHighlightFuture = nullptr;
    }

    const QString &txt = m_cursors.selectedText();
    if (txt.isEmpty())
        return;

    adjustScrollBarRanges();

    m_selectionHighlightFuture = new QFutureWatcher<FileSearchResultList>();
    connect(m_selectionHighlightFuture, &QFutureWatcher<FileSearchResultList>::resultsReadyAt,
            this, &TextEditorWidgetPrivate::selectionResultsReady);
    connect(m_selectionHighlightFuture, &QFutureWatcher<FileSearchResultList>::finished,
            this, &TextEditorWidgetPrivate::selectionFinished);
    m_selectionHighlightFuture->setPendingResultsLimit(10);

    const QTextDocument::FindFlags findFlags = textDocumentFlagsForFindFlags(m_findFlags);

    const FilePath &fileName = m_document->filePath();
    FileListIterator *it =
            new FileListIterator({fileName} , {const_cast<QTextCodec *>(m_document->codec())});
    QMap<FilePath, QString> fileToContentsMap;
    fileToContentsMap[fileName] = m_document->plainText();
    m_selectionHighlightFuture->setFuture(findInFiles(txt, it, findFlags, fileToContentsMap));
}


void TextEditorWidgetPrivate::scheduleUpdateHighlightScrollBar()
{
    if (m_scrollBarUpdateScheduled)
        return;

    m_scrollBarUpdateScheduled = true;
    QMetaObject::invokeMethod(this, &TextEditorWidgetPrivate::updateHighlightScrollBarNow,
                              Qt::QueuedConnection);
}


Highlight::Priority textMarkPrioToScrollBarPrio(const TextMark::Priority &prio)
{
    switch (prio) {
    case TextMark::LowPriority:
        return Highlight::LowPriority;
    case TextMark::NormalPriority:
        return Highlight::NormalPriority;
    case TextMark::HighPriority:
        return Highlight::HighPriority;
    default:
        return Highlight::NormalPriority;
    }
}


void TextEditorWidgetPrivate::addSearchResultsToScrollBar(const QVector<SearchResult> &results)
{
    if (!m_highlightScrollBarController)
        return;
    for (SearchResult result : results) {
        const QTextBlock &block = q->document()->findBlock(result.start);
        if (block.isValid() && block.isVisible()) {
            const int firstLine = block.layout()->lineForTextPosition(result.start - block.position()).lineNumber();
            const int lastLine = block.layout()->lineForTextPosition(result.start - block.position() + result.length).lineNumber();
            for (int line = firstLine; line <= lastLine; ++line) {
                m_highlightScrollBarController->addHighlight(
                    {Constants::SCROLL_BAR_SEARCH_RESULT, block.firstLineNumber() + line,
                            Theme::TextEditor_SearchResult_ScrollBarColor, Highlight::HighPriority});
            }
        }
    }
}


void TextEditorWidgetPrivate::addSelectionHighlightToScrollBar(
    const QVector<SearchResult> &selections)
{
    if (!m_highlightScrollBarController)
        return;
    for (const SearchResult &result : selections) {
        const QTextBlock &block = q->document()->findBlock(result.start);
        if (block.isValid() && block.isVisible()) {
            if (q->lineWrapMode() == QPlainTextEdit::WidgetWidth) {
                const int firstLine = block.layout()->lineForTextPosition(result.start - block.position()).lineNumber();
                const int lastLine = block.layout()->lineForTextPosition(result.start - block.position() + result.length).lineNumber();
                for (int line = firstLine; line <= lastLine; ++line) {
                    m_highlightScrollBarController->addHighlight(
                        {Constants::SCROLL_BAR_SELECTION, block.firstLineNumber() + line,
                         Theme::TextEditor_Selection_ScrollBarColor, Highlight::NormalPriority});
                }
            } else {
                m_highlightScrollBarController->addHighlight(
                    {Constants::SCROLL_BAR_SELECTION,
                     block.blockNumber(),
                     Theme::TextEditor_Selection_ScrollBarColor,
                     Highlight::NormalPriority});
            }
        }
    }
}


Highlight markToHighlight(TextMark *mark, int lineNumber)
{
    return Highlight(mark->category(),
                     lineNumber,
                     mark->color().value_or(Utils::Theme::TextColorNormal),
                     textMarkPrioToScrollBarPrio(mark->priority()));
}


void TextEditorWidgetPrivate::updateHighlightScrollBarNow()
{
    m_scrollBarUpdateScheduled = false;
    if (!m_highlightScrollBarController)
        return;

    m_highlightScrollBarController->removeAllHighlights();

    updateCurrentLineInScrollbar();

    // update search results
    addSearchResultsToScrollBar(m_searchResults);

    // update search selection
    addSelectionHighlightToScrollBar(m_selectionResults);

    // update text marks
    const TextMarks marks = m_document->marks();
    for (TextMark *mark : marks) {
        if (!mark->isVisible() || !mark->color().has_value())
            continue;
        const QTextBlock &block = q->document()->findBlockByNumber(mark->lineNumber() - 1);
        if (block.isVisible())
            m_highlightScrollBarController->addHighlight(markToHighlight(mark, block.firstLineNumber()));
    }
}


void TextEditorWidgetPrivate::setFindScope(const Utils::MultiTextCursor &scope)
{
    if (m_findScope != scope) {
        m_findScope = scope;
        q->viewport()->update();
        highlightSearchResultsInScrollBar();
    }
}


bool TextEditorWidget::inFindScope(const QTextCursor &cursor) const
{
    return d->m_find->inScope(cursor);
    //return false;
}


HighlightScrollBarController *TextEditorWidget::highlightScrollBarController() const
{
    return d->m_highlightScrollBarController;
}


} // namespace TextEditor

