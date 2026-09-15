// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
//
// v2 split of texteditor.cpp (legacy file kept at ../texteditor.cpp).
// Mouse and events: mouse/wheel/focus/timer/resize events, context menu, tooltips, links, zoom.
//

#include "texteditorwidget_p.h"

namespace TextEditor {

using namespace Internal;

static QFrame *createSeparator(const QString &styleSheet)
{
    QFrame* separator = new QFrame();
    separator->setStyleSheet(styleSheet);
    separator->setFrameShape(QFrame::HLine);
    QSizePolicy sizePolicy = separator->sizePolicy();
    sizePolicy.setHorizontalPolicy(QSizePolicy::MinimumExpanding);
    separator->setSizePolicy(sizePolicy);

    return separator;
}


static QLayout *createSeparatorLayout()
{
    QString styleSheet = "color: gray";

    QFrame* separator1 = createSeparator(styleSheet);
    QFrame* separator2 = createSeparator(styleSheet);
    auto label = new QLabel(TextEditorWidget::tr("Other annotations"));
    label->setStyleSheet(styleSheet);

    auto layout = new QHBoxLayout;
    layout->addWidget(separator1);
    layout->addWidget(label);
    layout->addWidget(separator2);

    return layout;
}


void TextEditorWidgetPrivate::showTextMarksToolTip(const QPoint &pos,
                                                   const TextMarks &marks,
                                                   const TextMark *mainTextMark) const
{
    if (!mainTextMark && marks.isEmpty())
        return; // Nothing to show

    TextMarks allMarks = marks;

    auto layout = new QGridLayout;
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    if (mainTextMark) {
        mainTextMark->addToToolTipLayout(layout);
        if (allMarks.size() > 1)
            layout->addLayout(createSeparatorLayout(), layout->rowCount(), 0, 1, -1);
    }

    Utils::sort(allMarks, [](const TextMark *mark1, const TextMark *mark2) {
        return mark1->priority() > mark2->priority();
    });

    for (const TextMark *mark : std::as_const(allMarks)) {
        if (mark != mainTextMark)
            mark->addToToolTipLayout(layout);
    }
    layout->addWidget(DisplaySettings::createAnnotationSettingsLink(),layout->rowCount(), 0, 1, -1, Qt::AlignRight);
    ToolTip::show(pos, layout, q);
}



void TextEditorWidget::showContextMenu()
{
    QTextCursor tc = textCursor();
    const QPoint cursorPos = mapToGlobal(cursorRect(tc).bottomRight() + QPoint(1,1));
    qGuiApp->postEvent(this, new QContextMenuEvent(QContextMenuEvent::Keyboard, cursorPos));
}



void TextEditorWidget::openLinkUnderCursor()
{
    d->openLinkUnderCursor(alwaysOpenLinksInNextSplit());
}


void TextEditorWidget::openLinkUnderCursorInNextSplit()
{
    d->openLinkUnderCursor(!alwaysOpenLinksInNextSplit());
}


bool TextEditorWidget::event(QEvent *e)
{
    if (!d)
        return QPlainTextEdit::event(e);

    // FIXME: That's far too heavy, and triggers e.g for ChildEvent
    if (e->type() != QEvent::InputMethodQuery){
         d->m_contentsChanged = false;
    }

    switch (e->type()) {
    case QEvent::ShortcutOverride: {
         auto ke = static_cast<QKeyEvent *>(e);
            if (ke->key() == Qt::Key_Escape
                && (d->m_snippetOverlay->isVisible() || multiTextCursor().hasMultipleCursors() || d->m_suggestionBlock.isValid())) {
                e->accept();
            }else if(ke->key()==Qt::Key_Delete){
                break;
            }else {
                // hack copied from QInputControl::isCommonTextEditShortcut
                // Fixes: QTCREATORBUG-22854
                if(ke->modifiers() == Qt::ControlModifier){
                    if(ke->key() == Qt::Key_A){
                        //select all
                        this->selectAll();
                        return true;
                    }else if(ke->key()==Qt::Key_D){
                        MultiTextCursor cursor = multiTextCursor();
                        cursor.beginEditBlock();

                        auto iter = cursor.begin();
                        while(iter!=cursor.end()){
                            //qDebug()<<"start pos"<<(*iter).position();
                            auto text = (*iter).block().text();
                            (*iter).movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
                            (*iter).insertBlock();
                            //(*iter).movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
                            (*iter).insertText(text);
                            d->m_document->autoIndent((*iter));
                            iter++;
                        }
                        cursor.endEditBlock();
                        setMultiTextCursor(cursor);
                        return true;
                    }
                }
                e->setAccepted((ke->modifiers() == Qt::NoModifier || ke->modifiers() == Qt::ShiftModifier || ke->modifiers() == Qt::KeypadModifier)  && (ke->key() < Qt::Key_Escape));
                d->m_maybeFakeTooltipEvent = false;
            }
        return true;
    }
    case QEvent::ApplicationPaletteChange: {
        // slight hack: ignore palette changes
        // at this point the palette has changed already,
        // so undo it by re-setting the palette:
        applyFontSettings();
        return true;
    }
    default:
        break;
    }
    return QPlainTextEdit::event(e);
}


void TextEditorWidget::contextMenuEvent(QContextMenuEvent *e)
{
    //showDefaultContextMenu(e, Id());
    /*QMenu contextMenu;

    contextMenu.exec(QCursor::pos());*/
    QPlainTextEdit::contextMenuEvent(e);
}


void TextEditorWidgetPrivate::openLinkUnderCursor(bool openInNextSplit)
{
    q->findLinkAt(q->textCursor(),
               [openInNextSplit, self = QPointer<TextEditorWidget>(q)](const Link &symbolLink) {
        if (self)
            self->openLink(symbolLink, openInNextSplit);
    }, true, openInNextSplit);
}


QPoint TextEditorWidget::toolTipPosition(const QTextCursor &c) const
{
    const QPoint cursorPos = mapToGlobal(cursorRect(c).bottomRight() + QPoint(1,1));
    return cursorPos + QPoint(d->m_extraArea->width(), HostOsInfo::isWindowsHost() ? -24 : -16);
}


void TextEditorWidget::showTextMarksToolTip(const QPoint &pos,
                                            const TextMarks &marks,
                                            const TextMark *mainTextMark) const
{
    d->showTextMarksToolTip(pos, marks, mainTextMark);
}


void TextEditorWidgetPrivate::processTooltipRequest(const QTextCursor &c)
{
    const QPoint toolTipPoint = q->toolTipPosition(c);
    bool handled = false;
    emit q->tooltipOverrideRequested(q, toolTipPoint, c.position(), &handled);
    if (handled)
        return;

    /*if (m_hoverHandlers.isEmpty()) {
        emit q->tooltipRequested(toolTipPoint, c.position());
        return;
    }*/

    /*const auto callback = [toolTipPoint](TextEditorWidget *widget, BaseHoverHandler *handler, int) {
        handler->showToolTip(widget, toolTipPoint);
    };*/
    //m_hoverHandlerRunner.startChecking(c, callback);
}


bool TextEditorWidgetPrivate::processAnnotaionTooltipRequest(const QTextBlock &block,
                                                             const QPoint &pos) const
{
    TextBlockUserData *blockUserData = TextDocumentLayout::textUserData(block);
    if (!blockUserData)
        return false;

    for (const AnnotationRect &annotationRect : m_annotationRects[block.blockNumber()]) {
        if (!annotationRect.rect.contains(pos))
            continue;
        showTextMarksToolTip(q->mapToGlobal(pos), blockUserData->marks(), annotationRect.mark);
        return true;
    }
    return false;
}


bool TextEditorWidget::viewportEvent(QEvent *event)
{
    d->m_contentsChanged = false;
    if (event->type() == QEvent::ToolTip) {
        if (QApplication::keyboardModifiers() & Qt::ControlModifier
                || (!(QApplication::keyboardModifiers() & Qt::ShiftModifier)
                    && d->m_behaviorSettings.m_constrainHoverTooltips)) {
            // Tooltips should be eaten when either control is pressed (so they don't get in the
            // way of code navigation) or if they are in constrained mode and shift is not pressed.
            return true;
        }
        const QHelpEvent *he = static_cast<QHelpEvent*>(event);
        const QPoint &pos = he->pos();

        RefactorMarker refactorMarker = d->m_refactorOverlay->markerAt(pos);
        if (refactorMarker.isValid() && !refactorMarker.tooltip.isEmpty()) {
            ToolTip::show(he->globalPos(), refactorMarker.tooltip,
                          viewport(), {}, refactorMarker.rect);
            return true;
        }

        QTextCursor tc = cursorForPosition(pos);
        QTextBlock block = tc.block();
        QTextLine line = block.layout()->lineForTextPosition(tc.positionInBlock());
        QTC_CHECK(line.isValid());
        // Only handle tool tip for text cursor if mouse is within the block for the text cursor,
        // and not if the mouse is e.g. in the empty space behind a short line.
        if (line.isValid()) {
            const QRectF blockGeometry = blockBoundingGeometry(block);
            const int width = block == d->m_suggestionBlock ? blockGeometry.width() : line.naturalTextRect().right();

            if (pos.x() <= blockGeometry.left() + width) {
                d->processTooltipRequest(tc);
                return true;
            } else if (d->processAnnotaionTooltipRequest(block, pos)) {
                return true;
            }
            ToolTip::hide();
        }
    }
    return QPlainTextEdit::viewportEvent(event);
}



void TextEditorWidget::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);
    QRect cr = rect();
    d->m_extraArea->setGeometry(
        QStyle::visualRect(layoutDirection(), cr,
                           QRect(cr.left() + frameWidth(), cr.top() + frameWidth(),
                                 extraAreaWidth(), cr.height() - 2 * frameWidth())));
    d->adjustScrollBarRanges();
    d->updateCurrentLineInScrollbar();
}


void TextEditorWidgetPrivate::saveCurrentCursorPositionForNavigation()
{
    m_lastCursorChangeWasInteresting = true;
    m_tempNavigationState = q->saveState();
}


void TextEditorWidget::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == d->autoScrollTimer.timerId()) {
        const QPoint globalPos = QCursor::pos();
        const QPoint pos = d->m_extraArea->mapFromGlobal(globalPos);
        QRect visible = d->m_extraArea->rect();
        verticalScrollBar()->triggerAction( pos.y() < visible.center().y() ?
                                            QAbstractSlider::SliderSingleStepSub
                                            : QAbstractSlider::SliderSingleStepAdd);
        QMouseEvent ev(QEvent::MouseMove, pos, globalPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        extraAreaMouseEvent(&ev);
        int delta = qMax(pos.y() - visible.top(), visible.bottom() - pos.y()) - visible.height();
        if (delta < 7)
            delta = 7;
        int timeout = 4900 / (delta * delta);
        d->autoScrollTimer.start(timeout, this);

    } else if (e->timerId() == d->foldedBlockTimer.timerId()) {
        d->visibleFoldedBlockNumber = d->suggestedVisibleFoldedBlockNumber;
        d->suggestedVisibleFoldedBlockNumber = -1;
        d->foldedBlockTimer.stop();
        viewport()->update();
    } else if (e->timerId() == d->m_cursorFlashTimer.timerId()) {
        d->m_cursorVisible = !d->m_cursorVisible;
        viewport()->update(d->cursorUpdateRect(d->m_cursors));
    }
    QPlainTextEdit::timerEvent(e);
}



void TextEditorWidget::mouseMoveEvent(QMouseEvent *e)
{
    d->requestUpdateLink(e);

    bool onLink = false;
    if (d->m_linkPressed && d->m_currentLink.hasValidTarget()) {
        const int eventCursorPosition = cursorForPosition(e->pos()).position();
        if (eventCursorPosition < d->m_currentLink.linkTextStart
            || eventCursorPosition > d->m_currentLink.linkTextEnd) {
            d->m_linkPressed = false;
        } else {
            onLink = true;
        }
    }

    static std::optional<MultiTextCursor> startMouseMoveCursor;
    if (e->buttons() == Qt::LeftButton && e->modifiers() & Qt::AltModifier) {
        if (!startMouseMoveCursor.has_value()) {
            startMouseMoveCursor = multiTextCursor();
            QTextCursor c = startMouseMoveCursor->takeMainCursor();
            if (!startMouseMoveCursor->hasMultipleCursors()
                && !startMouseMoveCursor->hasSelection()) {
                startMouseMoveCursor.emplace(MultiTextCursor());
            }
            c.setPosition(c.anchor());
            startMouseMoveCursor->addCursor(c);
        }
        MultiTextCursor cursor = *startMouseMoveCursor;
        const QTextCursor anchorCursor = cursor.takeMainCursor();
        const QTextCursor eventCursor = cursorForPosition(e->pos());

        const TabSettings tabSettings = d->m_document->tabSettings();
        int eventColumn = tabSettings.columnAt(eventCursor.block().text(),
                                               eventCursor.positionInBlock());
        if (eventCursor.positionInBlock() == eventCursor.block().length() - 1) {
            eventColumn += int((e->pos().x() - cursorRect(eventCursor).center().x())
                               / QFontMetricsF(font()).horizontalAdvance(' '));
        }

        int anchorColumn = tabSettings.columnAt(anchorCursor.block().text(),
                                                anchorCursor.positionInBlock());
        const TextEditorWidgetPrivate::BlockSelection blockSelection = {eventCursor.blockNumber(),
                                                                        eventColumn,
                                                                        anchorCursor.blockNumber(),
                                                                        anchorColumn};

        cursor.addCursors(d->generateCursorsForBlockSelection(blockSelection));
        if (!cursor.isNull())
            setMultiTextCursor(cursor);
    } else {
        if (startMouseMoveCursor.has_value())
            startMouseMoveCursor.reset();
        if (e->buttons() == Qt::NoButton) {
            const QTextBlock collapsedBlock = d->foldedBlockAt(e->pos());
            const int blockNumber = collapsedBlock.next().blockNumber();
            if (blockNumber < 0) {
                d->clearVisibleFoldedBlock();
            } else if (blockNumber != d->visibleFoldedBlockNumber) {
                d->suggestedVisibleFoldedBlockNumber = blockNumber;
                d->foldedBlockTimer.start(40, this);
            }

            const RefactorMarker refactorMarker = d->m_refactorOverlay->markerAt(e->pos());

            // Update the mouse cursor
            if ((collapsedBlock.isValid() || refactorMarker.isValid())
                && !d->m_mouseOnFoldedMarker) {
                d->m_mouseOnFoldedMarker = true;
                viewport()->setCursor(Qt::PointingHandCursor);
            } else if (!collapsedBlock.isValid() && !refactorMarker.isValid()
                       && d->m_mouseOnFoldedMarker) {
                d->m_mouseOnFoldedMarker = false;
                viewport()->setCursor(Qt::IBeamCursor);
            }
        } else if (!onLink || e->buttons() != Qt::LeftButton
                   || e->modifiers() != Qt::ControlModifier) {
            QPlainTextEdit::mouseMoveEvent(e);
        }
    }

    if (viewport()->cursor().shape() == Qt::BlankCursor)
        viewport()->setCursor(Qt::IBeamCursor);
}


static bool handleForwardBackwardMouseButtons(QMouseEvent *e)
{
    if (e->button() == Qt::XButton1) {
        //EditorManager::goBackInNavigationHistory();
        return true;
    }
    if (e->button() == Qt::XButton2) {
        //EditorManager::goForwardInNavigationHistory();
        return true;
    }

    return false;
}


void TextEditorWidget::mousePressEvent(QMouseEvent *e)
{
    ICore::restartTrimmer();

    if (e->button() == Qt::LeftButton) {
        MultiTextCursor multiCursor = multiTextCursor();
        const QTextCursor &cursor = cursorForPosition(e->pos());
        if (e->modifiers() & Qt::AltModifier && !(e->modifiers() & Qt::ControlModifier)) {
            if (e->modifiers() & Qt::ShiftModifier) {
                QTextCursor c = multiCursor.mainCursor();
                c.setPosition(cursor.position(), QTextCursor::KeepAnchor);
                multiCursor.replaceMainCursor(c);
            } else {
                multiCursor.addCursor(cursor);
            }
            setMultiTextCursor(multiCursor);
            return;
        } else {
            if (multiCursor.hasMultipleCursors())
                setMultiTextCursor(MultiTextCursor({cursor}));

            QTextBlock foldedBlock = d->foldedBlockAt(e->pos());
            if (foldedBlock.isValid()) {
                d->toggleBlockVisible(foldedBlock);
                viewport()->setCursor(Qt::IBeamCursor);
            }

            RefactorMarker refactorMarker = d->m_refactorOverlay->markerAt(e->pos());
            if (refactorMarker.isValid()) {
                if (refactorMarker.callback)
                    refactorMarker.callback(this);
            } else {
                d->m_linkPressed = d->isMouseNavigationEvent(e);
            }
        }
    } else if (e->button() == Qt::RightButton) {
        int eventCursorPosition = cursorForPosition(e->pos()).position();
        if (eventCursorPosition < textCursor().selectionStart()
                || eventCursorPosition > textCursor().selectionEnd()) {
            setTextCursor(cursorForPosition(e->pos()));
        }
    }

    if (HostOsInfo::isLinuxHost() && handleForwardBackwardMouseButtons(e))
        return;

    QPlainTextEdit::mousePressEvent(e);
}


void TextEditorWidget::mouseReleaseEvent(QMouseEvent *e)
{

    const Qt::MouseButton button = e->button();
    if (d->m_linkPressed && d->isMouseNavigationEvent(e) && button == Qt::LeftButton) {
        //EditorManager::addCurrentPositionToNavigationHistory();
        bool inNextSplit = ((e->modifiers() & Qt::AltModifier) && !alwaysOpenLinksInNextSplit())
                || (alwaysOpenLinksInNextSplit() && !(e->modifiers() & Qt::AltModifier));

        findLinkAt(textCursor(),
                   [inNextSplit, self = QPointer<TextEditorWidget>(this)](const Link &symbolLink) {
            if (self && self->openLink(symbolLink, inNextSplit))
                self->d->clearLink();
        }, true, inNextSplit);
    } else if (button == Qt::MiddleButton
               && !isReadOnly()
               && QGuiApplication::clipboard()->supportsSelection()) {
        if (!(e->modifiers() & Qt::AltModifier))
            doSetTextCursor(cursorForPosition(e->pos()));
        if (const QMimeData *md = QGuiApplication::clipboard()->mimeData(QClipboard::Selection))
            insertFromMimeData(md);
        e->accept();
        return;
    }

    if (!HostOsInfo::isLinuxHost() && handleForwardBackwardMouseButtons(e))
        return;

    QPlainTextEdit::mouseReleaseEvent(e);

    d->setClipboardSelection();
    const QTextCursor plainTextEditCursor = textCursor();
    const QTextCursor multiMainCursor = multiTextCursor().mainCursor();
    if (multiMainCursor.position() != plainTextEditCursor.position()
        || multiMainCursor.anchor() != plainTextEditCursor.anchor()) {
        doSetTextCursor(plainTextEditCursor, true);
    }
}


void TextEditorWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        QTextCursor cursor = textCursor();
        const int position = cursor.position();
        if (TextBlockUserData::findPreviousOpenParenthesis(&cursor, false, true)) {
            if (position - cursor.position() == 1 && selectBlockUp())
                return;
        }
    }

    QTextCursor eventCursor = cursorForPosition(QPoint(e->pos().x(), e->pos().y()));
    const int eventDocumentPosition = eventCursor.position();

    QPlainTextEdit::mouseDoubleClickEvent(e);

    // QPlainTextEdit::mouseDoubleClickEvent just selects the word under the text cursor. If the
    // event is triggered on a position that is inbetween two whitespaces this event selects the
    // previous word or nothing if the whitespaces are at the block start. Replace this behavior
    // with selecting the whitespaces starting from the previous word end to the next word.
    const QChar character = characterAt(eventDocumentPosition);
    const QChar prevCharacter = characterAt(eventDocumentPosition - 1);
    if (character.isSpace() && prevCharacter.isSpace()) {
        if (prevCharacter != QChar::ParagraphSeparator) {
            eventCursor.movePosition(QTextCursor::PreviousWord);
            eventCursor.movePosition(QTextCursor::EndOfWord);
        } else if (character == QChar::ParagraphSeparator) {
            return; // no special handling for empty lines
        }
        eventCursor.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor);
        MultiTextCursor cursor = multiTextCursor();
        cursor.replaceMainCursor(eventCursor);
        setMultiTextCursor(cursor);
    }


}



#ifdef WITH_TESTS
void TextEditorWidget::processTooltipRequest(const QTextCursor &c)
{
    d->processTooltipRequest(c);
}
#endif


void TextEditorWidget::wheelEvent(QWheelEvent *e)
{
    d->clearVisibleFoldedBlock();
    if (e->modifiers() & Qt::ControlModifier) {
        if (!scrollWheelZoomingEnabled()) {
            // When the setting is disabled globally,
            // we have to skip calling QPlainTextEdit::wheelEvent()
            // that changes zoom in it.
            return;
        }

        const int deltaY = e->angleDelta().y();
        if (deltaY != 0)
            zoomF(deltaY / 120.f);
        return;
    }
    QPlainTextEdit::wheelEvent(e);
}


static void showZoomIndicator(QWidget *editor, const int newZoom)
{
    Utils::FadingIndicator::showText(editor,
                                     QCoreApplication::translate("TextEditor::TextEditorWidget",
                                                                 "Zoom: %1%").arg(newZoom),
                                     Utils::FadingIndicator::SmallText);
}


void TextEditorWidget::zoomF(float delta)
{
    d->clearVisibleFoldedBlock();
    float step = 10.f * delta;
    // Ensure we always zoom a minimal step in-case the resolution is more than 16x
    if (step > 0 && step < 1)
        step = 1;
    else if (step < 0 && step > -1)
        step = -1;

    const int newZoom = TextEditorSettings::increaseFontZoom(int(step));
    showZoomIndicator(this, newZoom);
}


void TextEditorWidget::zoomReset()
{
    TextEditorSettings::resetFontZoom();
    showZoomIndicator(this, 100);
}


void TextEditorWidget::findLinkAt(const QTextCursor &cursor,
                                  const Utils::LinkHandler &callback,
                                  bool resolveTarget,
                                  bool inNextSplit)
{
    emit requestLinkAt(cursor, callback, resolveTarget, inNextSplit);
}


bool TextEditorWidget::openLink(const Utils::Link &link, bool inNextSplit)
{
#ifdef WITH_TESTS
    struct Signaller { ~Signaller() { emit EditorManager::instance()->linkOpened(); } } s;
#endif

    if (!link.hasValidTarget())
        return false;

    if (!inNextSplit && textDocument()->filePath() == link.targetFilePath) {
        //EditorManager::addCurrentPositionToNavigationHistory();
        gotoLine(link.targetLine, link.targetColumn, true, true);
        setFocus();
        return true;
    }
    /*EditorManager::OpenEditorFlags flags;
    if (inNextSplit)
        flags |= EditorManager::OpenInOtherSplit;

    return EditorManager::openEditorAt(link, Id(), flags);*/
    return false;
}


bool TextEditorWidgetPrivate::isMouseNavigationEvent(QMouseEvent *e) const
{
    return q->mouseNavigationEnabled() && e->modifiers() & Qt::ControlModifier
           && !(e->modifiers() & Qt::ShiftModifier);
}


void TextEditorWidgetPrivate::requestUpdateLink(QMouseEvent *e)
{
    if (!isMouseNavigationEvent(e))
        return;
    // Link emulation behaviour for 'go to definition'
    const QTextCursor cursor = q->cursorForPosition(e->pos());

    // Avoid updating the link we already found
    if (cursor.position() >= m_currentLink.linkTextStart
        && cursor.position() <= m_currentLink.linkTextEnd)
        return;

    // Check that the mouse was actually on the text somewhere
    bool onText = q->cursorRect(cursor).right() >= e->x();
    if (!onText) {
        QTextCursor nextPos = cursor;
        nextPos.movePosition(QTextCursor::Right);
        onText = q->cursorRect(nextPos).right() >= e->x();
    }

    if (onText) {
        m_pendingLinkUpdate = cursor;
        QMetaObject::invokeMethod(this, &TextEditorWidgetPrivate::updateLink, Qt::QueuedConnection);
        return;
    }

    clearLink();
}


void TextEditorWidgetPrivate::updateLink()
{
    if (m_pendingLinkUpdate.isNull())
        return;
    if (m_pendingLinkUpdate == m_lastLinkUpdate)
        return;

    m_lastLinkUpdate = m_pendingLinkUpdate;
    q->findLinkAt(m_pendingLinkUpdate,
                  [parent = QPointer<TextEditorWidget>(q), this](const Link &link) {
        if (!parent)
            return;

        if (link.hasValidLinkText())
            showLink(link);
        else
            clearLink();
    }, false);
}


void TextEditorWidgetPrivate::showLink(const Utils::Link &link)
{
    if (m_currentLink == link)
        return;

    QTextEdit::ExtraSelection sel;
    sel.cursor = q->textCursor();
    sel.cursor.setPosition(link.linkTextStart);
    sel.cursor.setPosition(link.linkTextEnd, QTextCursor::KeepAnchor);
    sel.format = m_document->fontSettings().toTextCharFormat(C_LINK);
    sel.format.setFontUnderline(true);
    q->setExtraSelections(TextEditorWidget::OtherSelection, QList<QTextEdit::ExtraSelection>() << sel);
    q->viewport()->setCursor(Qt::PointingHandCursor);
    m_currentLink = link;
}


void TextEditorWidgetPrivate::clearLink()
{
    m_pendingLinkUpdate = QTextCursor();
    m_lastLinkUpdate = QTextCursor();
    if (!m_currentLink.hasValidLinkText())
        return;

    q->setExtraSelections(TextEditorWidget::OtherSelection, QList<QTextEdit::ExtraSelection>());
    q->viewport()->setCursor(Qt::IBeamCursor);
    m_currentLink = Utils::Link();
}


void TextEditorWidget::changeEvent(QEvent *e)
{
    QPlainTextEdit::changeEvent(e);
    if (e->type() == QEvent::ApplicationFontChange
        || e->type() == QEvent::FontChange) {
        if (d->m_extraArea) {
            QFont f = d->m_extraArea->font();
            f.setPointSizeF(font().pointSizeF());
            d->m_extraArea->setFont(f);
            d->slotUpdateExtraAreaWidth();
            d->m_extraArea->update();
        }
    } else if (e->type() == QEvent::PaletteChange) {
        applyFontSettings();
    }
}


void TextEditorWidget::focusInEvent(QFocusEvent *e)
{
    QPlainTextEdit::focusInEvent(e);
    d->startCursorFlashTimer();
    d->updateHighlights();
}


void TextEditorWidget::focusOutEvent(QFocusEvent *e)
{
    QPlainTextEdit::focusOutEvent(e);
    //d->m_hoverHandlerRunner.abortHandlers();
    if (viewport()->cursor().shape() == Qt::BlankCursor)
        viewport()->setCursor(Qt::IBeamCursor);
    d->m_cursorFlashTimer.stop();
    if (d->m_cursorVisible) {
        d->m_cursorVisible = false;
        viewport()->update(d->cursorUpdateRect(d->m_cursors));
    }
    d->updateHighlights();
}


void TextEditorWidget::showEvent(QShowEvent* e)
{
    triggerPendingUpdates();
    // QPlainTextEdit::showEvent scrolls to make the cursor visible on first show
    // which we don't want, since we restore previous states when
    // opening editors, and when splitting/duplicating.
    // So restore the previous state after that.
    QByteArray state;
    if (d->m_wasNotYetShown)
        state = saveState();
    QPlainTextEdit::showEvent(e);
    if (d->m_wasNotYetShown) {
        restoreState(state);
        d->m_wasNotYetShown = false;

        QTimer::singleShot(0,[this]{
            if(isDestory){
                return ;
            }
            configureGenericHighlighter();
        });
    }


}


} // namespace TextEditor

