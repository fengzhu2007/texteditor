// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
//
// v2 split of texteditor.cpp (legacy file kept at ../texteditor.cpp).
// Core: construction/destruction, document management, printing, editor state, cursor position, misc API.
//

#include "texteditorwidget_p.h"

namespace TextEditor {

using namespace Internal;

TextEditorWidgetPrivate::TextEditorWidgetPrivate(TextEditorWidget *parent)
    : q(parent)
    , m_suggestionBlocker((void *) this, [](void *) {})
    , m_overlay(new TextEditorOverlay(q))
    , m_snippetOverlay(new SnippetOverlay(q))
    , m_searchResultOverlay(new TextEditorOverlay(q))
    , m_selectionHighlightOverlay(new TextEditorOverlay(q))
    , m_refactorOverlay(new RefactorOverlay(q))
    , m_marksVisible(false)
    , m_codeFoldingVisible(false)
    , m_codeFoldingSupported(false)
    , m_revisionsVisible(false)
    , m_lineNumbersVisible(true)
    , m_highlightCurrentLine(true)
    , m_requestMarkEnabled(true)
    , m_lineSeparatorsAllowed(false)
    , m_maybeFakeTooltipEvent(false)
    , m_hoverHandlerRunner(parent, m_hoverHandlers)
    , m_clipboardAssistProvider(new ClipboardAssistProvider)
    , m_autoCompleter(new AutoCompleter)
{
    m_selectionHighlightOverlay->show();
    auto aggregate = new Aggregation::Aggregate;
    m_find = new TextEditorWidgetFind(q);
    connect(m_find, &BaseTextFind::highlightAllRequested,
            this, &TextEditorWidgetPrivate::highlightSearchResultsSlot);
    connect(m_find, &BaseTextFind::findScopeChanged,
            this, &TextEditorWidgetPrivate::setFindScope);
    aggregate->add(m_find);
    aggregate->add(q);

    m_extraArea = new TextEditExtraArea(q);
    m_extraArea->setMouseTracking(true);



    m_extraSelections.reserve(NExtraSelectionKinds);

    connect(&m_codeAssistant, &CodeAssistant::finished,
            q, &TextEditorWidget::assistFinished);

    connect(q, &QPlainTextEdit::blockCountChanged, this, [this]() { slotUpdateExtraAreaWidth(); });

    connect(q, &QPlainTextEdit::modificationChanged,
            m_extraArea, QOverload<>::of(&QWidget::update));

    connect(q, &QPlainTextEdit::cursorPositionChanged,
            q, &TextEditorWidget::slotCursorPositionChanged);

    connect(q, &QPlainTextEdit::cursorPositionChanged,
            this, &TextEditorWidgetPrivate::updateCursorPosition);

    connect(q, &QPlainTextEdit::updateRequest,
            this, &TextEditorWidgetPrivate::slotUpdateRequest);

    connect(q, &QPlainTextEdit::selectionChanged,
            this, &TextEditorWidgetPrivate::slotSelectionChanged);

    m_parenthesesMatchingTimer.setSingleShot(true);
    m_parenthesesMatchingTimer.setInterval(50);
    connect(&m_parenthesesMatchingTimer, &QTimer::timeout,
            this, &TextEditorWidgetPrivate::_q_matchParentheses);

    m_highlightBlocksTimer.setSingleShot(true);
    connect(&m_highlightBlocksTimer, &QTimer::timeout,
            this, &TextEditorWidgetPrivate::_q_highlightBlocks);

    m_scrollBarUpdateTimer.setSingleShot(true);
    connect(&m_scrollBarUpdateTimer, &QTimer::timeout,
            this, &TextEditorWidgetPrivate::highlightSearchResultsInScrollBar);

    m_delayedUpdateTimer.setSingleShot(true);
    connect(&m_delayedUpdateTimer, &QTimer::timeout,
            q->viewport(), QOverload<>::of(&QWidget::update));


    TextEditorSettings *settings = TextEditorSettings::instance();

    // Connect to settings change signals
    connect(settings, &TextEditorSettings::typingSettingsChanged,
            q, &TextEditorWidget::setTypingSettings);
    connect(settings, &TextEditorSettings::storageSettingsChanged,
            q, &TextEditorWidget::setStorageSettings);
    connect(settings, &TextEditorSettings::behaviorSettingsChanged,
            q, &TextEditorWidget::setBehaviorSettings);
    connect(settings, &TextEditorSettings::marginSettingsChanged,
            q, &TextEditorWidget::setMarginSettings);
    connect(settings, &TextEditorSettings::displaySettingsChanged,
            q, &TextEditorWidget::setDisplaySettings);
    connect(settings, &TextEditorSettings::completionSettingsChanged,
            q, &TextEditorWidget::setCompletionSettings);
    connect(settings, &TextEditorSettings::extraEncodingSettingsChanged,
            q, &TextEditorWidget::setExtraEncodingSettings);
}


TextEditorWidgetPrivate::~TextEditorWidgetPrivate()
{
    QTextDocument *doc = m_document->document();
    QTC_CHECK(doc);
    auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
    QTC_CHECK(documentLayout);
    QTC_CHECK(m_document.data());
    documentLayout->disconnect(this);
    documentLayout->disconnect(m_extraArea);
    doc->disconnect(this);
    m_document.data()->disconnect(this);
    q->disconnect(documentLayout);
    q->disconnect(this);
    //delete m_toolBarWidget;
    delete m_highlightScrollBarController;

}


Id TextEditorWidget::SnippetPlaceholderSelection("TextEdit.SnippetPlaceHolderSelection");

Id TextEditorWidget::CurrentLineSelection("TextEdit.CurrentLineSelection");

Id TextEditorWidget::ParenthesesMatchingSelection("TextEdit.ParenthesesMatchingSelection");

Id TextEditorWidget::AutoCompleteSelection("TextEdit.AutoCompleteSelection");

Id TextEditorWidget::CodeWarningsSelection("TextEdit.CodeWarningsSelection");

Id TextEditorWidget::CodeSemanticsSelection("TextEdit.CodeSemanticsSelection");

Id TextEditorWidget::CursorSelection("TextEdit.CursorSelection");

Id TextEditorWidget::UndefinedSymbolSelection("TextEdit.UndefinedSymbolSelection");

Id TextEditorWidget::UnusedSymbolSelection("TextEdit.UnusedSymbolSelection");

Id TextEditorWidget::OtherSelection("TextEdit.OtherSelection");


TextEditorWidget::TextEditorWidget(QWidget *parent)
    : QPlainTextEdit(parent)
{
    // "Needed", as the creation below triggers ChildEvents that are
    // passed to this object's event() which uses 'd'.
    d = nullptr;
    d = new TextEditorWidgetPrivate(this);

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setLayoutDirection(Qt::LeftToRight);
    viewport()->setMouseTracking(true);
    setFrameStyle(QFrame::NoFrame);
}


void TextEditorWidget::setTextDocument(const QSharedPointer<TextDocument> &doc)
{
    d->setDocument(doc);
    d->m_codeAssistant.configure(this);
}


void TextEditorWidgetPrivate::setDocument(const QSharedPointer<TextDocument> &doc)
{
    QSharedPointer<TextDocument> previousDocument = m_document;
    for (const QMetaObject::Connection &connection : m_documentConnections)
        disconnect(connection);
    m_documentConnections.clear();

    m_document = doc;
    q->QPlainTextEdit::setDocument(doc->document());
    previousDocument.clear();
    q->setCursorWidth(2); // Applies to the document layout

    auto documentLayout = qobject_cast<TextDocumentLayout *>(
        m_document->document()->documentLayout());
    QTC_CHECK(documentLayout);

    m_documentConnections << connect(documentLayout,
                                     &QPlainTextDocumentLayout::updateBlock,
                                     this,
                                     &TextEditorWidgetPrivate::slotUpdateBlockNotify);

    m_documentConnections << connect(documentLayout,
                                     &TextDocumentLayout::updateExtraArea,
                                     m_extraArea,
                                     QOverload<>::of(&QWidget::update));

    m_documentConnections << connect(q,
                                     &TextEditorWidget::requestBlockUpdate,
                                     documentLayout,
                                     &QPlainTextDocumentLayout::updateBlock);

    m_documentConnections << connect(documentLayout,
                                     &TextDocumentLayout::updateExtraArea,
                                     this,
                                     &TextEditorWidgetPrivate::scheduleUpdateHighlightScrollBar);

    m_documentConnections << connect(documentLayout,
                                     &TextDocumentLayout::parenthesesChanged,
                                     &m_parenthesesMatchingTimer,
                                     QOverload<>::of(&QTimer::start));

    m_documentConnections << connect(documentLayout,
                                     &QAbstractTextDocumentLayout::documentSizeChanged,
                                     this,
                                     &TextEditorWidgetPrivate::scheduleUpdateHighlightScrollBar);

    m_documentConnections << connect(documentLayout,
                                     &QAbstractTextDocumentLayout::update,
                                     this,
                                     &TextEditorWidgetPrivate::scheduleUpdateHighlightScrollBar);

    m_documentConnections << connect(m_document->document(),
                                     &QTextDocument::contentsChange,
                                     this,
                                     &TextEditorWidgetPrivate::editorContentsChange);

    m_documentConnections << connect(m_document->document(),
                                     &QTextDocument::modificationChanged,
                                     q,
                                     &TextEditorWidget::updateTextCodecLabel);

    m_documentConnections << connect(m_document->document(),
                                     &QTextDocument::modificationChanged,
                                     q,
                                     &TextEditorWidget::updateTextLineEndingLabel);

    m_documentConnections << connect(m_document.data(),
                                     &TextDocument::aboutToReload,
                                     this,
                                     &TextEditorWidgetPrivate::documentAboutToBeReloaded);

    m_documentConnections << connect(m_document.data(),
                                     &TextDocument::reloadFinished,
                                     this,
                                     &TextEditorWidgetPrivate::documentReloadFinished);

    m_documentConnections << connect(m_document.data(),
                                     &TextDocument::tabSettingsChanged,
                                     this,
                                     &TextEditorWidgetPrivate::applyTabSettings);

    m_documentConnections << connect(m_document.data(),
                                     &TextDocument::fontSettingsChanged,
                                     this,
                                     &TextEditorWidgetPrivate::applyFontSettingsDelayed);

    m_documentConnections << connect(m_document.data(),
                                     &TextDocument::markRemoved,
                                     this,
                                     &TextEditorWidgetPrivate::markRemoved);

    m_documentConnections << connect(m_document.data(),
                                     &TextDocument::aboutToOpen,
                                     q,
                                     &TextEditorWidget::aboutToOpen);

    m_documentConnections << connect(m_document.data(),
                                     &TextDocument::openFinishedSuccessfully,
                                     q,
                                     &TextEditorWidget::openFinishedSuccessfully);

    m_documentConnections << connect(TextEditorSettings::instance(),
                                     &TextEditorSettings::fontSettingsChanged,
                                     m_document.data(),
                                     &TextDocument::setFontSettings);

    slotUpdateExtraAreaWidth();

    // Apply current settings
    // the document might already have the same settings as we set here in which case we do not
    // get an update, so we have to trigger updates manually here
    const FontSettings fontSettings = TextEditorSettings::fontSettings();
    if (m_document->fontSettings() == fontSettings)
        applyFontSettingsDelayed();
    else
        m_document->setFontSettings(fontSettings);
    /*const TabSettings tabSettings = TextEditorSettings::codeStyle()->tabSettings();
    if (m_document->tabSettings() == tabSettings)
        applyTabSettings();
    else
        m_document->setTabSettings(tabSettings); // also set through code style ???*/

    q->setTypingSettings(TextEditorSettings::typingSettings());
    q->setStorageSettings(TextEditorSettings::storageSettings());
    q->setBehaviorSettings(TextEditorSettings::behaviorSettings());
    q->setMarginSettings(TextEditorSettings::marginSettings());
    q->setDisplaySettings(TextEditorSettings::displaySettings());
    q->setCompletionSettings(TextEditorSettings::completionSettings());
    q->setExtraEncodingSettings(TextEditorSettings::extraEncodingSettings());
    //q->setCodeStyle(TextEditorSettings::codeStyle(m_tabSettingsId));

    m_blockCount = doc->document()->blockCount();

    // from RESEARCH

    extraAreaSelectionAnchorBlockNumber = -1;
    extraAreaToggleMarkBlockNumber = -1;
    extraAreaHighlightFoldedBlockNumber = -1;
    visibleFoldedBlockNumber = -1;
    suggestedVisibleFoldedBlockNumber = -1;

    if (m_bracketsAnimator)
        m_bracketsAnimator->finish();
    if (m_autocompleteAnimator)
        m_autocompleteAnimator->finish();

    slotUpdateExtraAreaWidth();
    updateHighlights();

    m_moveLineUndoHack = false;

    updateCannotDecodeInfo();

    q->updateTextCodecLabel();
    q->updateTextLineEndingLabel();
    setupFromDefinition(currentDefinition());
}


TextEditorWidget::~TextEditorWidget()
{
    isDestory = true;
    delete d;
    d = nullptr;
}


void TextEditorWidget::print(QPrinter *printer)
{

}


static void printPage(int index, QPainter *painter, const QTextDocument *doc,
                      const QRectF &body, const QRectF &titleBox,
                      const QString &title)
{
    painter->save();

    painter->translate(body.left(), body.top() - (index - 1) * body.height());
    const QRectF view(0, (index - 1) * body.height(), body.width(), body.height());

    QAbstractTextDocumentLayout *layout = doc->documentLayout();
    QAbstractTextDocumentLayout::PaintContext ctx;

    painter->setFont(QFont(doc->defaultFont()));
    const QRectF box = titleBox.translated(0, view.top());
    const int dpix = painter->device()->logicalDpiX();
    const int dpiy = painter->device()->logicalDpiY();
    const int mx = int(5 * dpix / 72.0);
    const int my = int(2 * dpiy / 72.0);
    painter->fillRect(box.adjusted(-mx, -my, mx, my), QColor(210, 210, 210));
    if (!title.isEmpty())
        painter->drawText(box, Qt::AlignCenter, title);
    const QString pageString = QString::number(index);
    painter->drawText(box, Qt::AlignRight, pageString);

    painter->setClipRect(view);
    ctx.clip = view;
    // don't use the system palette text as default text color, on HP/UX
    // for example that's white, and white text on white paper doesn't
    // look that nice
    ctx.palette.setColor(QPalette::Text, Qt::black);

    layout->draw(painter, ctx);

    painter->restore();
}


Q_LOGGING_CATEGORY(printLog, "qtc.editor.print", QtWarningMsg)


void TextEditorWidgetPrivate::print(QPrinter *printer)
{

}



void TextEditorWidgetPrivate::insertSuggestion(std::unique_ptr<TextSuggestion> &&suggestion)
{
    clearCurrentSuggestion();

    if (m_suggestionBlocker.use_count() > 1)
        return;

    auto cursor = q->textCursor();
    cursor.setPosition(suggestion->currentPosition());
    QTextOption option = suggestion->replacementDocument()->defaultTextOption();
    option.setTabStopDistance(charWidth() * m_document->tabSettings().m_tabSize);
    suggestion->replacementDocument()->setDefaultTextOption(option);
    auto options = suggestion->replacementDocument()->defaultTextOption();
    m_suggestionBlock = cursor.block();
    m_document->insertSuggestion(std::move(suggestion));
    forceUpdateScrollbarSize();
}


qreal TextEditorWidgetPrivate::charWidth() const
{
    return QFontMetricsF(q->font()).horizontalAdvance(QLatin1Char('x'));
}



void TextEditorWidgetPrivate::forceUpdateScrollbarSize()
{
    q->resizeEvent(new QResizeEvent(q->size(), q->size()));
}


void TextEditorWidgetPrivate::updateSuggestion()
{
    if (!m_suggestionBlock.isValid())
        return;
    const QTextCursor cursor = m_cursors.mainCursor();
    if (cursor.block() == m_suggestionBlock) {
        TextSuggestion *suggestion = TextDocumentLayout::suggestion(m_suggestionBlock);
        if (QTC_GUARD(suggestion)) {
            const int pos = cursor.position();
            if (pos >= suggestion->currentPosition()) {
                suggestion->setCurrentPosition(pos);
                if (suggestion->filterSuggestions(q)) {
                    TextDocumentLayout::updateSuggestionFormats(
                        m_suggestionBlock, m_document->fontSettings());
                    return;
                }
            }
        }
    }
    clearCurrentSuggestion();
}


void TextEditorWidgetPrivate::clearCurrentSuggestion()
{
    if (TextBlockUserData *userData = TextDocumentLayout::textUserData(m_suggestionBlock)) {
        userData->clearSuggestion();
        m_document->updateLayout();
    }
    m_suggestionBlock = QTextBlock();
}


void TextEditorWidget::selectEncoding()
{
    /*TextDocument *doc = d->m_document.data();
    CodecSelector codecSelector(this, doc);

    switch (codecSelector.exec()) {
    case CodecSelector::Reload: {
        QString errorString;
        if (!doc->reload(&errorString, codecSelector.selectedCodec())) {
            QMessageBox::critical(this, tr("File Error"), errorString);
            break;
        }
        break; }
    case CodecSelector::Save:
        doc->setCodec(codecSelector.selectedCodec());
        EditorManager::saveDocument(textDocument());
        updateTextCodecLabel();
        break;
    case CodecSelector::Cancel:
        break;
    }*/
}


void TextEditorWidget::selectLineEnding(int index)
{
    QTC_CHECK(index >= 0);
    const auto newMode = Utils::TextFileFormat::LineTerminationMode(index);
    if (d->m_document->lineTerminationMode() != newMode) {
        d->m_document->setLineTerminationMode(newMode);
        d->q->document()->setModified(true);
    }
}


void TextEditorWidget::updateTextLineEndingLabel()
{
    //d->m_fileLineEnding->setCurrentIndex(d->m_document->lineTerminationMode());
}


void TextEditorWidget::updateTextCodecLabel()
{
    QString text = QString::fromLatin1(d->m_document->codec()->name());
    //d->m_fileEncodingLabel->setText(text, text);
}


QString TextEditorWidget::msgTextTooLarge(quint64 size)
{
    return tr("The text is too large to be displayed (%1 MB).").
           arg(size >> 20);
}


void TextEditorWidget::insertPlainText(const QString &text)
{
    MultiTextCursor cursor = d->m_cursors;
    cursor.insertText(text);
    setMultiTextCursor(cursor);
}


QString TextEditorWidget::selectedText() const
{
    return d->m_cursors.selectedText();
}


void TextEditorWidgetPrivate::updateCannotDecodeInfo()
{
    q->setReadOnly(m_document->hasDecodingError());
    InfoBar *infoBar = m_document->infoBar();
    Id selectEncodingId(Constants::SELECT_ENCODING);
    if (m_document->hasDecodingError()) {
        if (!infoBar->canInfoBeAdded(selectEncodingId))
            return;
        InfoBarEntry info(selectEncodingId,
            TextEditorWidget::tr("<b>Error:</b> Could not decode \"%1\" with \"%2\"-encoding. Editing not possible.")
                .arg(m_document->displayName(), QString::fromLatin1(m_document->codec()->name())));
        info.addCustomButton(TextEditorWidget::tr("Select Encoding"), [this] { q->selectEncoding(); });
        infoBar->addInfo(info);
    } else {
        infoBar->removeInfo(selectEncodingId);
    }
}

// Skip over shebang to license header (Python, Perl, sh)
// '#!/bin/sh'
// ''
// '###############'


static QTextBlock skipShebang(const QTextBlock &block)
{
    if (!block.isValid() || !block.text().startsWith("#!"))
        return block;
    const QTextBlock nextBlock1 = block.next();
    if (!nextBlock1.isValid() || !nextBlock1.text().isEmpty())
        return block;
    const QTextBlock nextBlock2 = nextBlock1.next();
    return nextBlock2.isValid() && nextBlock2.text().startsWith('#') ? nextBlock2 : block;
}

/*
  Collapses the first comment in a file, if there is only whitespace/shebang line
  above
  */

void TextEditorWidgetPrivate::foldLicenseHeader()
{
    QTextDocument *doc = q->document();
    auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
    QTC_ASSERT(documentLayout, return);
    QTextBlock block = skipShebang(doc->firstBlock());
    while (block.isValid() && block.isVisible()) {
        QString text = block.text();
        if (TextDocumentLayout::canFold(block) && block.next().isVisible()) {
            const QString trimmedText = text.trimmed();
            QStringList commentMarker;
            if (auto highlighter = qobject_cast<Highlighter *>(
                    q->textDocument()->syntaxHighlighter())) {
                const Highlighter::Definition def = highlighter->definition();
                for (const QString &marker :
                     {def.singleLineCommentMarker(), def.multiLineCommentMarker().first}) {
                    if (!marker.isEmpty())
                        commentMarker << marker;
                }
            } else {
                commentMarker = QStringList({"/*", "#"});
            }

            if (Utils::anyOf(commentMarker, [&](const QString &marker) {
                    return trimmedText.startsWith(marker);
                })) {
                TextDocumentLayout::doFoldOrUnfold(block, false);
                moveCursorVisible();
                documentLayout->requestUpdate();
                documentLayout->emitDocumentSizeChanged();
                break;
            }
        }
        if (TabSettings::firstNonSpace(text) < text.size())
            break;
        block = block.next();
    }
}


TextDocument *TextEditorWidget::textDocument() const
{
    return d->m_document.data();
}


void TextEditorWidget::aboutToOpen(const Utils::FilePath &filePath, const Utils::FilePath &realFilePath)
{
    Q_UNUSED(filePath)
    Q_UNUSED(realFilePath)
}


void TextEditorWidget::openFinishedSuccessfully()
{
    d->moveCursor(QTextCursor::Start);
    d->updateCannotDecodeInfo();
    updateTextCodecLabel();
    updateVisualWrapColumn();
}


TextDocumentPtr TextEditorWidget::textDocumentPtr() const
{
    return d->m_document;
}



void TextEditorWidgetPrivate::editorContentsChange(int position, int charsRemoved, int charsAdded)
{
    if (m_bracketsAnimator)
        m_bracketsAnimator->finish();

    m_contentsChanged = true;
    QTextDocument *doc = q->document();
    auto documentLayout = static_cast<TextDocumentLayout*>(doc->documentLayout());
    const QTextBlock posBlock = doc->findBlock(position);

    // Keep the line numbers and the block information for the text marks updated
    if (charsRemoved != 0) {
        documentLayout->updateMarksLineNumber();
        documentLayout->updateMarksBlock(posBlock);
    } else {
        const QTextBlock nextBlock = doc->findBlock(position + charsAdded);
        if (posBlock != nextBlock) {
            documentLayout->updateMarksLineNumber();
            documentLayout->updateMarksBlock(posBlock);
            documentLayout->updateMarksBlock(nextBlock);
        } else {
            documentLayout->updateMarksBlock(posBlock);
        }
    }

    if (m_snippetOverlay->isVisible()) {
        QTextCursor cursor = q->textCursor();
        cursor.setPosition(position);
        snippetCheckCursor(cursor);
    }

    if ((charsAdded != 0 && q->document()->characterAt(position + charsAdded - 1).isPrint()) || charsRemoved != 0)
        m_codeAssistant.notifyChange();

    int newBlockCount = doc->blockCount();
    if (!q->hasFocus() && newBlockCount != m_blockCount) {
        // lines were inserted or removed from outside, keep viewport on same part of text
        if (q->firstVisibleBlock().blockNumber() > posBlock.blockNumber())
            q->verticalScrollBar()->setValue(q->verticalScrollBar()->value() + newBlockCount - m_blockCount);
    }
    m_blockCount = newBlockCount;
    m_scrollBarUpdateTimer.start(500);
    m_visualIndentCache.clear();
}


void TextEditorWidgetPrivate::slotSelectionChanged()
{
    //qDebug()<<"TextEditorWidgetPrivate::slotSelectionChanged"<<&m_selectBlockAnchor<<q->textCursor().hasSelection()<<m_selectBlockAnchor.isNull();
    if (!q->textCursor().hasSelection() && !m_selectBlockAnchor.isNull()){
        m_selectBlockAnchor = QTextCursor();
    }
    if(q->textCursor().hasSelection()==false){
        q->textCursor().clearSelection();
    }

    // Clear any link which might be showing when the selection changes
    clearLink();
    setClipboardSelection();
}


void TextEditorWidget::findUsages()
{
    emit requestUsages(textCursor());
}


void TextEditorWidget::renameSymbolUnderCursor()
{
    emit requestRename(textCursor());
}


void TextEditorWidget::abortAssist()
{
    d->m_codeAssistant.destroyContext();
}


void TextEditorWidget::doSetTextCursor(const QTextCursor &cursor, bool keepMultiSelection)
{
    // workaround for QTextControl bug
    bool selectionChange = cursor.hasSelection() || textCursor().hasSelection();
    QTextCursor c = cursor;
    c.setVisualNavigation(true);
    const MultiTextCursor oldCursor = d->m_cursors;
    if (!keepMultiSelection)
        const_cast<MultiTextCursor &>(d->m_cursors).setCursors({c});
    else
        const_cast<MultiTextCursor &>(d->m_cursors).replaceMainCursor(c);
    d->updateCursorSelections();
    d->resetCursorFlashTimer();
    QPlainTextEdit::doSetTextCursor(c);
    if (oldCursor != d->m_cursors) {
        QRect updateRect = d->cursorUpdateRect(oldCursor);
        if (d->m_highlightCurrentLine)
            updateRect = QRect(0, updateRect.y(), viewport()->rect().width(), updateRect.height());
        updateRect |= d->cursorUpdateRect(d->m_cursors);
        viewport()->update(updateRect);
        emit cursorPositionChanged();
    }
    if (selectionChange)
        d->slotSelectionChanged();
}


void TextEditorWidget::doSetTextCursor(const QTextCursor &cursor)
{
    doSetTextCursor(cursor, false);
}


void TextEditorWidget::gotoLine(int line, int column, bool centerLine, bool animate)
{
    d->m_lastCursorChangeWasInteresting = false; // avoid adding the previous position to history
    const int blockNumber = qMin(line, document()->blockCount()) - 1;
    const QTextBlock &block = document()->findBlockByNumber(blockNumber);
    if (block.isValid()) {
        QTextCursor cursor(block);
        if (column > 0) {
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
        } else {
            int pos = cursor.position();
            while (document()->characterAt(pos).category() == QChar::Separator_Space) {
                ++pos;
            }
            cursor.setPosition(pos);
        }

        const DisplaySettings &ds = d->m_displaySettings;
        if (animate && ds.m_animateNavigationWithinFile) {
            QScrollBar *scrollBar = verticalScrollBar();
            const int start = scrollBar->value();

            ensureBlockIsUnfolded(block);
            setUpdatesEnabled(false);
            setTextCursor(cursor);
            if (centerLine)
                centerCursor();
            else
                ensureCursorVisible();
            const int end = scrollBar->value();
            scrollBar->setValue(start);
            setUpdatesEnabled(true);

            const int delta = end - start;
            // limit the number of steps for the animation otherwise you wont be able to tell
            // the direction of the animantion for large delta values
            const int steps = qMax(-ds.m_animateWithinFileTimeMax,
                                   qMin(ds.m_animateWithinFileTimeMax, delta));
            // limit the duration of the animation to at least 4 pictures on a 60Hz Monitor and
            // at most to the number of absolute steps
            const int durationMinimum = int (4 // number of pictures
                                             * float(1) / 60 // on a 60 Hz Monitor
                                             * 1000); // milliseconds
            const int duration = qMax(durationMinimum, qAbs(steps));

            d->m_navigationAnimation = new QSequentialAnimationGroup(this);
            auto startAnimation = new QPropertyAnimation(verticalScrollBar(), "value");
            startAnimation->setEasingCurve(QEasingCurve::InExpo);
            startAnimation->setStartValue(start);
            startAnimation->setEndValue(start + steps / 2);
            startAnimation->setDuration(duration / 2);
            d->m_navigationAnimation->addAnimation(startAnimation);
            auto endAnimation = new QPropertyAnimation(verticalScrollBar(), "value");
            endAnimation->setEasingCurve(QEasingCurve::OutExpo);
            endAnimation->setStartValue(end - steps / 2);
            endAnimation->setEndValue(end);
            endAnimation->setDuration(duration / 2);
            d->m_navigationAnimation->addAnimation(endAnimation);
            d->m_navigationAnimation->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
            setTextCursor(cursor);
            if (centerLine)
                centerCursor();
            else
                ensureCursorVisible();
        }
    }
    d->saveCurrentCursorPositionForNavigation();
}


int TextEditorWidget::position(TextPositionOperation posOp, int at) const
{
    QTextCursor tc = textCursor();

    if (at != -1)
        tc.setPosition(at);

    if (posOp == CurrentPosition)
        return tc.position();

    switch (posOp) {
    case EndOfLinePosition:
        tc.movePosition(QTextCursor::EndOfLine);
        return tc.position();
    case StartOfLinePosition:
        tc.movePosition(QTextCursor::StartOfLine);
        return tc.position();
    case AnchorPosition:
        if (tc.hasSelection())
            return tc.anchor();
        break;
    case EndOfDocPosition:
        tc.movePosition(QTextCursor::End);
        return tc.position();
    default:
        break;
    }

    return -1;
}


QRect TextEditorWidget::cursorRect(int pos) const
{
    QTextCursor tc = textCursor();
    if (pos >= 0)
        tc.setPosition(pos);
    QRect result = cursorRect(tc);
    result.moveTo(viewport()->mapToGlobal(result.topLeft()));
    return result;
}


void TextEditorWidget::convertPosition(int pos, int *line, int *column) const
{
    Text::convertPosition(document(), pos, line, column);
}


void TextEditorWidgetPrivate::documentAboutToBeReloaded()
{
    //memorize cursor position
    m_tempState = q->saveState();

    // remove extra selections (loads of QTextCursor objects)

    m_extraSelections.clear();
    m_extraSelections.reserve(NExtraSelectionKinds);
    q->QPlainTextEdit::setExtraSelections(QList<QTextEdit::ExtraSelection>());

    // clear all overlays
    m_overlay->clear();
    m_snippetOverlay->clear();
    m_searchResultOverlay->clear();
    m_refactorOverlay->clear();

    // clear search results
    m_searchResults.clear();
    m_selectionResults.clear();
}


void TextEditorWidgetPrivate::documentReloadFinished(bool success)
{
    if (!success)
        return;

    // restore cursor position
    q->restoreState(m_tempState);
    updateCannotDecodeInfo();
}


QByteArray TextEditorWidget::saveState() const
{
    QByteArray state;
    QDataStream stream(&state, QIODevice::WriteOnly);
    stream << 2; // version number
    stream << verticalScrollBar()->value();
    stream << horizontalScrollBar()->value();
    int line, column;
    convertPosition(textCursor().position(), &line, &column);
    stream << line;
    stream << column;

    // store code folding state
    QList<int> foldedBlocks;
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        if (block.userData() && static_cast<TextBlockUserData*>(block.userData())->folded()) {
            int number = block.blockNumber();
            foldedBlocks += number;
        }
        block = block.next();
    }
    stream << foldedBlocks;

    stream << firstVisibleBlockNumber();
    stream << lastVisibleBlockNumber();

    return state;
}


void TextEditorWidget::restoreState(const QByteArray &state)
{
    if (state.isEmpty()) {
        if (d->m_displaySettings.m_autoFoldFirstComment)
            d->foldLicenseHeader();
        return;
    }
    int version;
    int vval;
    int hval;
    int lineVal;
    int columnVal;
    QDataStream stream(state);
    stream >> version;
    stream >> vval;
    stream >> hval;
    stream >> lineVal;
    stream >> columnVal;

    if (version >= 1) {
        QList<int> collapsedBlocks;
        stream >> collapsedBlocks;
        QTextDocument *doc = document();
        bool layoutChanged = false;
        for (const int blockNumber : std::as_const(collapsedBlocks)) {
            QTextBlock block = doc->findBlockByNumber(qMax(0, blockNumber));
            if (block.isValid()) {
                TextDocumentLayout::doFoldOrUnfold(block, false);
                layoutChanged = true;
            }
        }
        if (layoutChanged) {
            auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
            QTC_ASSERT(documentLayout, return );
            documentLayout->requestUpdate();
            documentLayout->emitDocumentSizeChanged();
        }
    } else {
        if (d->m_displaySettings.m_autoFoldFirstComment)
            d->foldLicenseHeader();
    }

    d->m_lastCursorChangeWasInteresting = false; // avoid adding last position to history
    // line is 1-based, column is 0-based
    gotoLine(lineVal, columnVal - 1);
    verticalScrollBar()->setValue(vval);
    horizontalScrollBar()->setValue(hval);

    if (version >= 2) {
        int originalFirstBlock, originalLastBlock;
        stream >> originalFirstBlock;
        stream >> originalLastBlock;
        // If current line was visible in the old state, make sure it is visible in the new state.
        // This can happen if the height of the editor changed in the meantime
        const int lineBlock = lineVal - 1; // line is 1-based, blocks are 0-based
        const bool originalCursorVisible = (originalFirstBlock <= lineBlock
                                            && lineBlock <= originalLastBlock);
        const int firstBlock = firstVisibleBlockNumber();
        const int lastBlock = lastVisibleBlockNumber();
        const bool cursorVisible = (firstBlock <= lineBlock && lineBlock <= lastBlock);
        if (originalCursorVisible && !cursorVisible)
            centerCursor();
    }

    d->saveCurrentCursorPositionForNavigation();
}


void TextEditorWidgetPrivate::reconfigure()
{
    m_document->setMimeType(
        Utils::mimeTypeForFile(m_document->filePath(),
                               MimeMatchMode::MatchDefaultAndRemote).name());
    q->configureGenericHighlighter();
}


void TextEditorWidgetPrivate::updateSyntaxInfoBar(const Highlighter::Definitions &definitions,
                                                  const QString &fileName)
{
    Id missing(Constants::INFO_MISSING_SYNTAX_DEFINITION);
    Id multiple(Constants::INFO_MULTIPLE_SYNTAX_DEFINITIONS);
    InfoBar *infoBar = m_document->infoBar();

    if (definitions.isEmpty() && infoBar->canInfoBeAdded(missing)
        && !TextEditorSettings::highlighterSettings().isIgnoredFilePattern(fileName)) {
        InfoBarEntry info(missing,
                          QObject::tr("A highlight definition was not found for this file. "
                                             "Would you like to download additional highlight definition files?"),
                          InfoBarEntry::GlobalSuppression::Enabled);
        info.addCustomButton(QObject::tr("Download Definitions"), [missing, this]() {
            m_document->infoBar()->removeInfo(missing);
            Highlighter::downloadDefinitions();
        });

        infoBar->removeInfo(multiple);
        infoBar->addInfo(info);
    } else if (definitions.size() > 1) {
        InfoBarEntry info(multiple,
                          QObject::tr("More than one highlight definition was found for this file. "
                                             "Which one should be used to highlight this file?"));
        info.setComboInfo(Utils::transform(definitions, &Highlighter::Definition::name),
                          [this](const InfoBarEntry::ComboInfo &info) {
            this->configureGenericHighlighter(Highlighter::definitionForName(info.displayText));
        });

        info.addCustomButton(QObject::tr("Remember My Choice"), [multiple, this]() {
            m_document->infoBar()->removeInfo(multiple);
            rememberCurrentSyntaxDefinition();
        });

        infoBar->removeInfo(missing);
        infoBar->addInfo(info);
    } else {
        infoBar->removeInfo(multiple);
        infoBar->removeInfo(missing);
    }
}


void TextEditorWidgetPrivate::removeSyntaxInfoBar()
{
    InfoBar *infoBar = m_document->infoBar();
    infoBar->removeInfo(Constants::INFO_MISSING_SYNTAX_DEFINITION);
    infoBar->removeInfo(Constants::INFO_MULTIPLE_SYNTAX_DEFINITIONS);
}


void TextEditorWidgetPrivate::configureGenericHighlighter(const KSyntaxHighlighting::Definition &definition)
{
    if (definition.isValid()) {
        auto loader = q->createLanguageLoader(definition.name(), m_document->document());
        if(loader!=nullptr){
            auto indenter = loader->indenter();
            if(indenter!=nullptr){
                m_document->setIndenter(indenter);
            }
            auto autoCompleter = loader->autoCompleter();
            if(autoCompleter!=nullptr){
                q->setAutoCompleter(autoCompleter);
            }
            auto codeFormatter = loader->codeFormatter();
            if(codeFormatter!=nullptr){
                m_document->setFormatter(codeFormatter);
                auto provider = m_document->completionAssistProvider();
                if(provider!=nullptr){
                    provider->setCodeFormatter(codeFormatter);
                }
            }
            auto highlighter = loader->hightlighter();
            if(highlighter!=nullptr){
                m_document->setSyntaxHighlighter(highlighter);
            }else{
                auto highlighter = new Highlighter();
                m_document->setSyntaxHighlighter(highlighter);
                highlighter->setDefinition(definition);
            }
            setupFromDefinition(definition);
            delete loader;//destory loader
        }else{
            q->setCodeFoldingSupported(false);
        }
    } else {
        q->setCodeFoldingSupported(false);
    }
    m_document->setFontSettings(TextEditorSettings::fontSettings());
}


LanguageLoader *TextEditorWidget::createLanguageLoader(const QString &languageName, QTextDocument *doc)
{
    auto *env = TextEditorEnvironment::getIntance();
    if (env)
        return env->loader(languageName, doc);
    return nullptr;
}


bool TextEditorWidget::applySyntaxHighlighter(const QString &definitionName)
{
    auto definition = Highlighter::definitionForName(definitionName);
    if (!definition.isValid())
        return false;

    auto *highlighter = new Highlighter();
    highlighter->setDefinition(definition);
    d->m_document->setSyntaxHighlighter(highlighter);
    d->setupFromDefinition(definition);
    d->m_document->setFontSettings(TextEditorSettings::fontSettings());
    return true;
}


void TextEditorWidgetPrivate::setupFromDefinition(const KSyntaxHighlighting::Definition &definition)
{
    if (!definition.isValid())
        return;
    m_commentDefinition.singleLine = definition.singleLineCommentMarker();
    m_commentDefinition.multiLineStart = definition.multiLineCommentMarker().first;
    m_commentDefinition.multiLineEnd = definition.multiLineCommentMarker().second;
    q->setCodeFoldingSupported(true);
}


KSyntaxHighlighting::Definition TextEditorWidgetPrivate::currentDefinition()
{
    if (auto highlighter = qobject_cast<Highlighter *>(m_document->syntaxHighlighter()))
        return highlighter->definition();
    return {};
}


void TextEditorWidgetPrivate::rememberCurrentSyntaxDefinition()
{
    const Highlighter::Definition &definition = currentDefinition();
    if (definition.isValid())
        Highlighter::rememberDefinitionForDocument(definition, m_document.data());
}


void TextEditorWidgetPrivate::slotUpdateBlockNotify(const QTextBlock &block)
{
    static bool blockRecursion = false;
    if (blockRecursion)
        return;
    blockRecursion = true;
    if (m_overlay->isVisible()) {
        /* an overlay might draw outside the block bounderies, force
           complete viewport update */
        q->viewport()->update();
    } else {
        if (block.previous().isValid() && block.userState() != block.previous().userState()) {
        /* The syntax highlighting state changes. This opens up for
           the possibility that the paragraph has braces that support
           code folding. In this case, do the save thing and also
           update the previous block, which might contain a fold
           box which now is invalid.*/
            emit q->requestBlockUpdate(block.previous());
        }

        for (const QTextCursor &scope : m_findScope) {
            QSet<int> updatedBlocks;
            const bool blockContainsFindScope = block.position() < scope.selectionEnd()
                                                && block.position() + block.length()
                                                       >= scope.selectionStart();
            if (blockContainsFindScope) {
                QTextBlock b = block.document()->findBlock(scope.selectionStart());
                do {
                    if (!updatedBlocks.contains(b.blockNumber())) {
                        updatedBlocks << b.blockNumber();
                        emit q->requestBlockUpdate(b);
                    }
                    b = b.next();
                } while (b.isValid() && b.position() < scope.selectionEnd());
            }
        }
    }
    blockRecursion = false;
}


void TextEditorWidget::ensureCursorVisible()
{
    ensureBlockIsUnfolded(textCursor().block());
    QPlainTextEdit::ensureCursorVisible();
}


MultiTextCursor TextEditorWidget::multiTextCursor() const
{
    return d->m_cursors;
}


void TextEditorWidget::setMultiTextCursor(const Utils::MultiTextCursor &cursor)
{
    const MultiTextCursor oldCursor = d->m_cursors;
    const_cast<MultiTextCursor &>(d->m_cursors) = cursor;
    if (oldCursor == d->m_cursors)
        return;
    doSetTextCursor(d->m_cursors.mainCursor(), /*keepMultiSelection*/ true);
    QRect updateRect = d->cursorUpdateRect(oldCursor);
    if (d->m_highlightCurrentLine)
        updateRect = QRect(0, updateRect.y(), viewport()->rect().width(), updateRect.height());
    updateRect |= d->cursorUpdateRect(d->m_cursors);
    viewport()->update(updateRect);
    emit cursorPositionChanged();
}


QRegion TextEditorWidget::translatedLineRegion(int lineStart, int lineEnd) const
{
    QRegion region;
    for (int i = lineStart ; i <= lineEnd; i++) {
        QTextBlock block = document()->findBlockByNumber(i);
        QPoint topLeft = blockBoundingGeometry(block).translated(contentOffset()).topLeft().toPoint();

        if (block.isValid()) {
            QTextLayout *layout = block.layout();

            for (int i = 0; i < layout->lineCount();i++) {
                QTextLine line = layout->lineAt(i);
                region += line.naturalTextRect().translated(topLeft).toRect();
            }
        }
    }
    return region;
}


void TextEditorWidget::triggerPendingUpdates()
{
    if (d->m_fontSettingsNeedsApply)
        applyFontSettings();
    textDocument()->triggerPendingUpdates();
}


void TextEditorWidget::setReadOnly(bool b)
{
    QPlainTextEdit::setReadOnly(b);
    emit readOnlyChanged();
    if (b)
        setTextInteractionFlags(textInteractionFlags() | Qt::TextSelectableByKeyboard);
}


void TextEditorWidget::switchUtf8bom()
{
    textDocument()->switchUtf8Bom();
}


void TextEditorWidget::setupFallBackEditor(Id id)
{
    TextDocumentPtr doc(new TextDocument(id));
    doc->setFontSettings(TextEditorSettings::fontSettings());
    setTextDocument(doc);
}





void TextEditorWidget::remove(int length)
{
    QTextCursor tc = textCursor();
    tc.setPosition(tc.position() + length, QTextCursor::KeepAnchor);
    tc.removeSelectedText();
}



void TextEditorWidget::replace(int length, const QString &string)
{
    QTextCursor tc = textCursor();
    tc.setPosition(tc.position() + length, QTextCursor::KeepAnchor);
    tc.insertText(string);
}



void TextEditorWidget::setCursorPosition(int pos)
{
    QTextCursor tc = textCursor();
    tc.setPosition(pos);
    setTextCursor(tc);
}




RefactorMarkers TextEditorWidget::refactorMarkers() const
{
    return d->m_refactorOverlay->markers();
}


void TextEditorWidget::setRefactorMarkers(const RefactorMarkers &markers)
{
    const QList<RefactorMarker> oldMarkers = d->m_refactorOverlay->markers();
    for (const RefactorMarker &marker : oldMarkers)
        emit requestBlockUpdate(marker.cursor.block());
    d->m_refactorOverlay->setMarkers(markers);
    for (const RefactorMarker &marker : markers)
        emit requestBlockUpdate(marker.cursor.block());
}


void TextEditorWidget::addHoverHandler(BaseHoverHandler *handler)
{
    if (!d->m_hoverHandlers.contains(handler))
        d->m_hoverHandlers.append(handler);
}


int TextEditorWidget::columnCount() const
{
    QFontMetricsF fm(font());
    return int(viewport()->rect().width() / fm.horizontalAdvance(QLatin1Char('x')));
}


int TextEditorWidget::rowCount() const
{
    int height = viewport()->rect().height();
    int lineCount = 0;
    QTextBlock block = firstVisibleBlock();
    while (block.isValid()) {
        height -= blockBoundingRect(block).height();
        if (height < 0) {
            const int blockLineCount = block.layout()->lineCount();
            for (int i = 0; i < blockLineCount; ++i) {
                ++lineCount;
                const QTextLine line = block.layout()->lineAt(i);
                height += line.rect().height();
                if (height >= 0)
                    break;
            }
            return lineCount;
        }
        lineCount += block.layout()->lineCount();
        block = block.next();
    }
    return lineCount;
}

/**
  Helper function to transform a selected text. If nothing is selected at the moment
  the word under the cursor is used.
  The type of the transformation is determined by the function pointer given.

  @param method     pointer to the QString function to use for the transformation

  @see uppercaseSelection, lowercaseSelection
*/

void TextEditorWidget::invokeAssist(AssistKind kind, IAssistProvider *provider)
{
    if (multiTextCursor().hasMultipleCursors())
        return;

    if (kind == QuickFix && d->m_snippetOverlay->isVisible())
        d->m_snippetOverlay->accept();

    bool previousMode = overwriteMode();
    setOverwriteMode(false);
    ensureCursorVisible();
    d->m_codeAssistant.invoke(kind, provider);
    setOverwriteMode(previousMode);
}


AssistInterface *TextEditorWidget::createAssistInterface(AssistKind kind,
                                                         AssistReason reason) const
{
    Q_UNUSED(kind)
    return new AssistInterface(textCursor(), d->m_document->filePath(), reason);
}


QChar TextEditorWidget::characterAt(int pos) const
{
    return textDocument()->characterAt(pos);
}


QString TextEditorWidget::textAt(int from, int to) const
{
    return textDocument()->textAt(from, to);
}


void TextEditorWidget::insertSuggestion(std::unique_ptr<TextSuggestion> &&suggestion){
    d->insertSuggestion(std::move(suggestion));
}


void TextEditorWidget::clearSuggestion(){
    d->clearCurrentSuggestion();
}


TextSuggestion *TextEditorWidget::currentSuggestion() const{
    if (d->m_suggestionBlock.isValid())
            return TextDocumentLayout::suggestion(d->m_suggestionBlock);
        return nullptr;
}


bool TextEditorWidget::suggestionVisible() const{
    return currentSuggestion();
}


bool TextEditorWidget::suggestionsBlocked() const{
    return d->m_suggestionBlocker.use_count() > 1;
}



TextEditorWidget::SuggestionBlocker TextEditorWidget::blockSuggestions(){
    if (!suggestionsBlocked())
            clearSuggestion();
        return d->m_suggestionBlocker;
}


// The remnants of PlainTextEditor.

} // namespace TextEditor

QT_BEGIN_NAMESPACE

size_t qHash(const QColor &color)
{
    return color.rgba();
}

QT_END_NAMESPACE

