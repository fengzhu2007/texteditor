// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
#pragma once

//
// Private header of the v2 split TextEditorWidget implementation.
// The public API stays in texteditor.h; the legacy single-file
// implementation remains in ../texteditor.cpp and can be selected
// again in CMake via ADY_TEXTEDITOR_V2=OFF.
//

#include "texteditor.h"
#include "texteditor_p.h"
#include "displaysettings.h"
#include "marginsettings.h"
#include "fontsettings.h"
//#include "texteditoractionhandler.h"

#include "autocompleter.h"
#include "basehoverhandler.h"
#include "behaviorsettings.h"
#include "circularclipboard.h"
#include "circularclipboardassist.h"
#include "completionsettings.h"
#include "extraencodingsettings.h"
#include "highlighter.h"
#include "highlightersettings.h"
#include "icodestylepreferences.h"
#include "refactoroverlay.h"
//#include "snippets/snippet.h"
#include "snippets/snippetoverlay.h"
#include "storagesettings.h"
#include "syntaxhighlighter.h"
#include "tabsettings.h"
#include "textdocument.h"
#include "textdocumentlayout.h"
#include "texteditorconstants.h"
#include "texteditoroverlay.h"
#include "texteditorsettings.h"
//#include "textindenter.h"
#include "typingsettings.h"
#include "codeassist/assistinterface.h"
#include "codeassist/codeassistant.h"
#include "aggregation/aggregate.h"
#include "core/coreconstants.h"
//#include "core/coreconstants.h"

//#include <coreplugin/dialogs/codecselector.h>
#include "core/find/basetextfind.h"
#include "core/find/highlightscrollbarcontroller.h"
#include "core/icore.h"
//#include <coreplugin/manhattanstyle.h>

#include "utils/algorithm.h"
#include "utils/camelcasecursor.h"
#include "utils/executeondestruction.h"
#include "utils/fadingindicator.h"
#include "utils/filesearch.h"
#include "utils/hostosinfo.h"
#include "utils/infobar.h"
#include "utils/mimeutils.h"
#include "utils/multitextcursor.h"
#include "utils/qtcassert.h"
#include "utils/textutils.h"
#include "utils/theme/theme.h"
#include "utils/tooltip/tooltip.h"
#include "utils/uncommentselection.h"

#include "snippets/snippetprovider.h"
#include "displaysettings.h"

#include "utils/theme/theme.h"
#include "utils/theme/theme_p.h"

#include "codeassist/documentcontentcompletion.h"

#include "codeformatter.h"
#include "texteditorenvironment.h"


#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QComboBox>
#include <QDebug>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QDrag>
#include <QRegularExpression>
#include <QSequentialAnimationGroup>
#include <QScreen>
#include <QScrollBar>
#include <QLabel>
#include <QStyle>
#include <QStyleFactory>
#include <QTextBlock>
#include <QTextCodec>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextLayout>
#include <QTime>
#include <QTimeLine>
#include <QTimer>
#include <QToolBar>
#include <QStack>

/*!
    \namespace TextEditor
    \brief The TextEditor namespace contains the base text editor and several classes which
    provide supporting functionality like snippets, highlighting, \l{CodeAssist}{code assist},
    indentation and style, and others.
*/

/*!
    \namespace TextEditor::Internal
    \internal
*/

/*!
    \class TextEditor::BaseTextEditor
    \brief The BaseTextEditor class is base implementation for QPlainTextEdit-based
    text editors. It can use the Kate text highlighting definitions, and some basic
    auto indentation.

    The corresponding document base class is BaseTextDocument, the corresponding
    widget base class is BaseTextEditorWidget.

    It is the default editor for text files used by \QC, if no other editor
    implementation matches the MIME type.
*/

using namespace Core;
using namespace Utils;

namespace TextEditor {

// ---- free helpers shared across implementation files ----

inline int foldBoxWidth(const QFontMetrics &fm)
{
    const int lineSpacing = fm.lineSpacing();
    return lineSpacing + lineSpacing % 2 + 1;
}

inline int foldBoxWidth()
{
    const int lineSpacing = TextEditorSettings::fontSettings().lineSpacing();
    return lineSpacing + lineSpacing % 2 + 1;
}

inline QTextBlock nextVisibleBlock(const QTextBlock &block,
                                   const QTextDocument *doc)
{
    QTextBlock nextVisibleBlock = block.next();
    if (!nextVisibleBlock.isVisible()) {
        // invisible blocks do have zero line count
        nextVisibleBlock = doc->findBlockByLineNumber(nextVisibleBlock.firstLineNumber());
        // paranoia in case our code somewhere did not set the line count
        // of the invisible block to 0
        while (nextVisibleBlock.isValid() && !nextVisibleBlock.isVisible())
            nextVisibleBlock = nextVisibleBlock.next();
    }
    return nextVisibleBlock;
}


namespace Internal {

enum { NExtraSelectionKinds = 12 };


using TransformationMethod = QString(const QString &);

using ListTransformationMethod = void(QStringList &);


class TextEditorAnimator : public QObject
{
    Q_OBJECT


public:
    TextEditorAnimator(QObject *parent);

    void init(const QTextCursor &cursor, const QFont &f, const QPalette &pal);
    inline QTextCursor cursor() const { return m_cursor; }

    void draw(QPainter *p, const QPointF &pos);
    QRectF rect() const;

    inline qreal value() const { return m_value; }
    inline QPointF lastDrawPos() const { return m_lastDrawPos; }

    void finish();

    bool isRunning() const;


signals:
    void updateRequest(const QTextCursor &cursor, QPointF lastPos, QRectF rect);


private:
    void step(qreal v);

    QTimeLine m_timeline;
    qreal m_value;
    QTextCursor m_cursor;
    QPointF m_lastDrawPos;
    QFont m_font;
    QPalette m_palette;
    QString m_text;
    QSizeF m_size;
};


class TextEditExtraArea : public QWidget
{

public:
    TextEditExtraArea(TextEditorWidget *edit)
        : QWidget(edit)
    {
        textEdit = edit;
        setAutoFillBackground(true);
    }


protected:
    QSize sizeHint() const override {
        return {textEdit->extraAreaWidth(), 0};
    }
    void paintEvent(QPaintEvent *event) override {
        textEdit->extraAreaPaintEvent(event);
    }
    void mousePressEvent(QMouseEvent *event) override {
        textEdit->extraAreaMouseEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        textEdit->extraAreaMouseEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        textEdit->extraAreaMouseEvent(event);
    }
    void leaveEvent(QEvent *event) override {
        textEdit->extraAreaLeaveEvent(event);
    }
    void contextMenuEvent(QContextMenuEvent *event) override {
        textEdit->extraAreaContextMenuEvent(event);
    }
    void changeEvent(QEvent *event) override {
        if (event->type() == QEvent::PaletteChange)
            QCoreApplication::sendEvent(textEdit, event);
    }
    void wheelEvent(QWheelEvent *event) override {
        QCoreApplication::sendEvent(textEdit->viewport(), event);
    }


private:
    TextEditorWidget *textEdit;
};


class BaseTextEditorPrivate
{

public:
    BaseTextEditorPrivate() = default;

};


struct CursorData
{
    QTextLayout *layout = nullptr;
    QPointF offset;
    int pos = 0;
    QPen pen;
};


struct PaintEventData
{
    PaintEventData(TextEditorWidget *editor, QPaintEvent *event, QPointF offset)
        : offset(offset)
        , viewportRect(editor->viewport()->rect())
        , eventRect(event->rect())
        , doc(editor->document())
        , documentLayout(qobject_cast<TextDocumentLayout *>(doc->documentLayout()))
        , documentWidth(int(doc->size().width()))
        , textCursor(editor->textCursor())
        , textCursorBlock(textCursor.block())
        , isEditable(!editor->isReadOnly())
        , fontSettings(editor->textDocument()->fontSettings())
        , searchScopeFormat(fontSettings.toTextCharFormat(C_SEARCH_SCOPE))
        , searchResultFormat(fontSettings.toTextCharFormat(C_SEARCH_RESULT))
        , visualWhitespaceFormat(fontSettings.toTextCharFormat(C_VISUAL_WHITESPACE))
        , ifdefedOutFormat(fontSettings.toTextCharFormat(C_DISABLED_CODE))
        , suppressSyntaxInIfdefedOutBlock(ifdefedOutFormat.foreground()
                                          != fontSettings.toTextCharFormat(C_TEXT).foreground())
    { }
    QPointF offset;
    const QRect viewportRect;
    const QRect eventRect;
    qreal rightMargin = -1;
    const QTextDocument *doc;
    TextDocumentLayout *documentLayout;
    const int documentWidth;
    const QTextCursor textCursor;
    const QTextBlock textCursorBlock;
    const bool isEditable;
    const FontSettings fontSettings;
    const QTextCharFormat searchScopeFormat;
    const QTextCharFormat searchResultFormat;
    const QTextCharFormat visualWhitespaceFormat;
    const QTextCharFormat ifdefedOutFormat;
    const bool suppressSyntaxInIfdefedOutBlock;
    QAbstractTextDocumentLayout::PaintContext context;
    QTextBlock visibleCollapsedBlock;
    QPointF visibleCollapsedBlockOffset;
    QTextBlock block;
    QList<CursorData> cursors;
};


struct PaintEventBlockData
{
    QRectF boundingRect;
    QVector<QTextLayout::FormatRange> selections;
    QTextLayout *layout = nullptr;
    int position = 0;
    int length = 0;
};


struct ExtraAreaPaintEventData;


class TextEditorWidgetPrivate : public QObject
{

public:
    TextEditorWidgetPrivate(TextEditorWidget *parent);
    ~TextEditorWidgetPrivate() override;

    void updateLineSelectionColor();

    void print(QPrinter *printer);

    void maybeSelectLine();
    void duplicateSelection(bool comment);
    void updateCannotDecodeInfo();
    void collectToCircularClipboard();
    void setClipboardSelection();

    void setDocument(const QSharedPointer<TextDocument> &doc);
    void handleHomeKey(bool anchor, bool block);
    void handleBackspaceKey();
    void moveLineUpDown(bool up);
    void addSelectionNextFindMatch();
    void addCursorsToLineEnds();
    void saveCurrentCursorPositionForNavigation();
    void updateHighlights();
    void updateCurrentLineInScrollbar();
    void updateCurrentLineHighlight();
    int indentDepthForBlock(const QTextBlock &block);

    void drawFoldingMarker(QPainter *painter, const QPalette &pal,
                           const QRect &rect,
                           bool expanded,
                           bool active,
                           bool hovered) const;
    bool updateAnnotationBounds(TextBlockUserData *blockUserData, TextDocumentLayout *layout,
                                bool annotationsVisible);
    void updateLineAnnotation(const PaintEventData &data, const PaintEventBlockData &blockData,
                              QPainter &painter);
    void paintRightMarginArea(PaintEventData &data, QPainter &painter) const;
    void paintRightMarginLine(const PaintEventData &data, QPainter &painter) const;
    void paintBlockHighlight(const PaintEventData &data, QPainter &painter) const;
    void paintSearchResultOverlay(const PaintEventData &data, QPainter &painter) const;
    void paintSelectionOverlay(const PaintEventData &data, QPainter &painter) const;

    void paintIfDefedOutBlocks(const PaintEventData &data, QPainter &painter) const;
    void paintFindScope(const PaintEventData &data, QPainter &painter) const;
    void paintCurrentLineHighlight(const PaintEventData &data, QPainter &painter) const;
    void paintCursorAsBlock(const PaintEventData &data, QPainter &painter,
                            PaintEventBlockData &blockData, int cursorPosition) const;
    void paintAdditionalVisualWhitespaces(PaintEventData &data, QPainter &painter, qreal top) const;
    void paintIndentDepth(PaintEventData &data, QPainter &painter, const PaintEventBlockData &blockData);
    void paintReplacement(PaintEventData &data, QPainter &painter, qreal top) const;
    void paintWidgetBackground(const PaintEventData &data, QPainter &painter) const;
    void paintOverlays(const PaintEventData &data, QPainter &painter) const;
    void paintCursor(const PaintEventData &data, QPainter &painter) const;

    void setupBlockLayout(const PaintEventData &data, QPainter &painter,
                          PaintEventBlockData &blockData) const;
    void setupSelections(const PaintEventData &data, PaintEventBlockData &blockData) const;
    void addCursorsPosition(PaintEventData &data,
                            QPainter &painter,
                            const PaintEventBlockData &blockData) const;
    QTextBlock nextVisibleBlock(const QTextBlock &block) const;
    void cleanupAnnotationCache();

    // extra area paint methods
    void paintLineNumbers(QPainter &painter, const ExtraAreaPaintEventData &data,
                          const QRectF &blockBoundingRect) const;
    void paintTextMarks(QPainter &painter, const ExtraAreaPaintEventData &data,
                        const QRectF &blockBoundingRect) const;
    void paintCodeFolding(QPainter &painter, const ExtraAreaPaintEventData &data,
                          const QRectF &blockBoundingRect) const;
    void paintRevisionMarker(QPainter &painter, const ExtraAreaPaintEventData &data,
                             const QRectF &blockBoundingRect) const;

    void toggleBlockVisible(const QTextBlock &block);
    QRect foldBox();

    QTextBlock foldedBlockAt(const QPoint &pos, QRect *box = nullptr) const;

    bool isMouseNavigationEvent(QMouseEvent *e) const;
    void requestUpdateLink(QMouseEvent *e);
    void updateLink();
    void showLink(const Utils::Link &);
    void clearLink();

    void universalHelper(); // test function for development

    bool cursorMoveKeyEvent(QKeyEvent *e);

    void processTooltipRequest(const QTextCursor &c);
    bool processAnnotaionTooltipRequest(const QTextBlock &block, const QPoint &pos) const;
    void showTextMarksToolTip(const QPoint &pos,
                              const TextMarks &marks,
                              const TextMark *mainTextMark = nullptr) const;

    void transformSelection(TransformationMethod method);

    void transformSelectedLines(ListTransformationMethod method);

    void slotUpdateExtraAreaWidth(std::optional<int> width = {});
    void slotUpdateRequest(const QRect &r, int dy);
    void slotUpdateBlockNotify(const QTextBlock &);
    void updateTabStops();
    void applyTabSettings();
    void applyFontSettingsDelayed();
    void markRemoved(TextMark *mark);

    void editorContentsChange(int position, int charsRemoved, int charsAdded);
    void documentAboutToBeReloaded();
    void documentReloadFinished(bool success);
    void highlightSearchResultsSlot(const QString &txt, FindFlags findFlags);
    void searchResultsReady(int beginIndex, int endIndex);
    void searchFinished();
    void selectionResultsReady(int beginIndex, int endIndex);
    void selectionFinished();
    void setupScrollBar();
    void highlightSearchResultsInScrollBar();
    void highlightSelectionResultsInScrollBar();
    void scheduleUpdateHighlightScrollBar();

    void updateHighlightScrollBarNow();
    struct SearchResult {
        int start;
        int length;
    };
    void addSearchResultsToScrollBar(const QVector<SearchResult> &results);
    void addSelectionHighlightToScrollBar(const QVector<SearchResult> &selections);
    void adjustScrollBarRanges();

    void setFindScope(const MultiTextCursor &scope);

    void updateCursorPosition();

    // parentheses matcher
    void _q_matchParentheses();
    void _q_highlightBlocks();
    void autocompleterHighlight(const QTextCursor &cursor = QTextCursor());
    void updateAnimator(QPointer<TextEditorAnimator> animator, QPainter &painter);
    void cancelCurrentAnimations();
    void slotSelectionChanged();
    void _q_animateUpdate(const QTextCursor &cursor, QPointF lastPos, QRectF rect);
    void updateCodeFoldingVisible();
    void updateFileLineEndingVisible();

    void reconfigure();
    void updateSyntaxInfoBar(const Highlighter::Definitions &definitions, const QString &fileName);
    void removeSyntaxInfoBar();
    void configureGenericHighlighter(const KSyntaxHighlighting::Definition &definition);
    void setupFromDefinition(const KSyntaxHighlighting::Definition &definition);
    KSyntaxHighlighting::Definition currentDefinition();
    void rememberCurrentSyntaxDefinition();
    void openLinkUnderCursor(bool openInNextSplit);

    qreal charWidth() const;

    void forceUpdateScrollbarSize();


public:
    TextEditorWidget *q;
    BaseTextFind *m_find = nullptr;

    //uint m_optionalActionMask = TextEditorActionHandler::None;
    bool m_contentsChanged = false;
    bool m_lastCursorChangeWasInteresting = false;
    std::shared_ptr<void> m_suggestionBlocker;

    QSharedPointer<TextDocument> m_document;
    QList<QMetaObject::Connection> m_documentConnections;
    QByteArray m_tempState;
    QByteArray m_tempNavigationState;

    bool m_parenthesesMatchingEnabled = false;
    QTimer m_parenthesesMatchingTimer;

    QWidget *m_extraArea = nullptr;

    Id m_tabSettingsId;
    ICodeStylePreferences *m_codeStylePreferences = nullptr;
    DisplaySettings m_displaySettings;
    bool m_annotationsrRight = true;
    MarginSettings m_marginSettings;
    // apply when making visible the first time, for the split case
    bool m_fontSettingsNeedsApply = true;
    bool m_wasNotYetShown = true;
    BehaviorSettings m_behaviorSettings;

    int extraAreaSelectionAnchorBlockNumber = -1;
    int extraAreaToggleMarkBlockNumber = -1;
    int extraAreaHighlightFoldedBlockNumber = -1;
    int extraAreaPreviousMarkTooltipRequestedLine = -1;

    TextEditorOverlay *m_overlay = nullptr;
    SnippetOverlay *m_snippetOverlay = nullptr;
    TextEditorOverlay *m_searchResultOverlay = nullptr;
    TextEditorOverlay *m_selectionHighlightOverlay = nullptr;
    bool snippetCheckCursor(const QTextCursor &cursor);
    void snippetTabOrBacktab(bool forward);

    struct AnnotationRect
    {
        QRectF rect;
        const TextMark *mark;
        friend bool operator==(const AnnotationRect &a, const AnnotationRect &b)
        { return a.mark == b.mark && a.rect == b.rect; }
    };
    QMap<int, QList<AnnotationRect>> m_annotationRects;
    QRectF getLastLineLineRect(const QTextBlock &block);

    RefactorOverlay *m_refactorOverlay = nullptr;
    //HelpItem m_contextHelpItem;

    QBasicTimer foldedBlockTimer;
    int visibleFoldedBlockNumber = -1;
    int suggestedVisibleFoldedBlockNumber = -1;
    void clearVisibleFoldedBlock();
    bool m_mouseOnFoldedMarker = false;
    void foldLicenseHeader();

    QBasicTimer autoScrollTimer;
    uint m_marksVisible : 1;
    uint m_codeFoldingVisible : 1;
    uint m_codeFoldingSupported : 1;
    uint m_revisionsVisible : 1;
    uint m_lineNumbersVisible : 1;
    uint m_highlightCurrentLine : 1;
    uint m_requestMarkEnabled : 1;
    uint m_lineSeparatorsAllowed : 1;
    uint m_maybeFakeTooltipEvent : 1;
    int m_visibleWrapColumn = 0;

    Utils::Link m_currentLink;
    bool m_linkPressed = false;
    QTextCursor m_pendingLinkUpdate;
    QTextCursor m_lastLinkUpdate;

    QRegularExpression m_searchExpr;
    QString m_findText;
    FindFlags m_findFlags;
    void highlightSearchResults(const QTextBlock &block, const PaintEventData &data) const;
    void highlightSelection(const QTextBlock &block) const;
    QTimer m_delayedUpdateTimer;

    void setExtraSelections(Utils::Id kind, const QList<QTextEdit::ExtraSelection> &selections);
    QHash<Utils::Id, QList<QTextEdit::ExtraSelection>> m_extraSelections;

    void startCursorFlashTimer();
    void resetCursorFlashTimer();
    QBasicTimer m_cursorFlashTimer;
    bool m_cursorVisible = false;
    bool m_moveLineUndoHack = false;
    void updateCursorSelections();
    void moveCursor(QTextCursor::MoveOperation operation,
                    QTextCursor::MoveMode mode = QTextCursor::MoveAnchor);
    QRect cursorUpdateRect(const MultiTextCursor &cursor);

    Utils::MultiTextCursor m_findScope;

    QTextCursor m_selectBlockAnchor;

    void moveCursorVisible(bool ensureVisible = true);

    int visualIndent(const QTextBlock &block) const;
    TextEditorPrivateHighlightBlocks m_highlightBlocksInfo;
    QTimer m_highlightBlocksTimer;

    CodeAssistant m_codeAssistant;

    QList<BaseHoverHandler *> m_hoverHandlers; // Not owned
    HoverHandlerRunner m_hoverHandlerRunner;

    QPointer<QSequentialAnimationGroup> m_navigationAnimation;

    QPointer<TextEditorAnimator> m_bracketsAnimator;

    // Animation and highlighting of auto completed text
    QPointer<TextEditorAnimator> m_autocompleteAnimator;
    bool m_animateAutoComplete = true;
    bool m_highlightAutoComplete = true;
    bool m_skipAutoCompletedText = true;
    bool m_skipFormatOnPaste = false;
    bool m_removeAutoCompletedText = true;
    bool m_keepAutoCompletionHighlight = false;
    QList<QTextCursor> m_autoCompleteHighlightPos;
    void updateAutoCompleteHighlight();

    QList<int> m_cursorBlockNumbers;
    int m_blockCount = 0;

    QPoint m_markDragStart;
    bool m_markDragging = false;
    QCursor m_markDragCursor;
    TextMark* m_dragMark = nullptr;
    QTextCursor m_dndCursor;

    QScopedPointer<ClipboardAssistProvider> m_clipboardAssistProvider;

    QScopedPointer<AutoCompleter> m_autoCompleter;
    CommentDefinition m_commentDefinition;

    QFutureWatcher<FileSearchResultList> *m_searchWatcher = nullptr;
    QFutureWatcher<FileSearchResultList> *m_selectionHighlightFuture = nullptr;
    QVector<SearchResult> m_searchResults;
    QVector<SearchResult> m_selectionResults;
    QTimer m_scrollBarUpdateTimer;
    HighlightScrollBarController *m_highlightScrollBarController = nullptr;
    bool m_scrollBarUpdateScheduled = false;

    const MultiTextCursor m_cursors;
    struct BlockSelection
    {
        int blockNumber = -1;
        int column = -1;
        int anchorBlockNumber = -1;
        int anchorColumn = -1;
    };
    QList<BlockSelection> m_blockSelections;
    QList<QTextCursor> generateCursorsForBlockSelection(const BlockSelection &blockSelection);
    void initBlockSelection();
    void clearBlockSelection();
    void handleMoveBlockSelection(QTextCursor::MoveOperation op);

    class UndoCursor
    {
    public:
        int position = 0;
        int anchor = 0;
    };
    using UndoMultiCursor = QList<UndoCursor>;
    QStack<UndoMultiCursor> m_undoCursorStack;
    QList<int> m_visualIndentCache;
    int m_visualIndentOffset = 0;
    bool m_hightlighted = false;


    void insertSuggestion(std::unique_ptr<TextSuggestion> &&suggestion);
    void updateSuggestion();
    void clearCurrentSuggestion();
    QTextBlock m_suggestionBlock;


};


class TextEditorWidgetFind : public BaseTextFind
{

public:
    TextEditorWidgetFind(TextEditorWidget *editor)
        : BaseTextFind(editor)
        , m_editor(editor)
    {
        setMultiTextCursorProvider([editor]() { return editor->multiTextCursor(); });
    }

    ~TextEditorWidgetFind() override { cancelCurrentSelectAll(); }

    bool supportsSelectAll() const override { return true; }
    void selectAll(const QString &txt, FindFlags findFlags) override;

    static void cancelCurrentSelectAll();


private:
    TextEditorWidget * const m_editor;
    static QFutureWatcher<FileSearchResultList> *m_selectWatcher;
};


struct ExtraAreaPaintEventData
{
    ExtraAreaPaintEventData(const TextEditorWidget *editor, TextEditorWidgetPrivate *d)
        : doc(editor->document())
        , documentLayout(qobject_cast<TextDocumentLayout*>(doc->documentLayout()))
        , selectionStart(editor->textCursor().selectionStart())
        , selectionEnd(editor->textCursor().selectionEnd())
        , fontMetrics(d->m_extraArea->font())
        , lineSpacing(fontMetrics.lineSpacing())
        , markWidth(d->m_marksVisible ? lineSpacing : 0)
        , collapseColumnWidth(d->m_codeFoldingVisible ? foldBoxWidth(fontMetrics) : 0)
        , extraAreaWidth(d->m_extraArea->width() - collapseColumnWidth)
        , currentLineNumberFormat(
              editor->textDocument()->fontSettings().toTextCharFormat(C_CURRENT_LINE_NUMBER))
        , palette(d->m_extraArea->palette())
    {
        if (TextEditorSettings::fontSettings().relativeLineSpacing() != 100) {
            lineSpacing = TextEditorSettings::fontSettings().lineSpacing();
            collapseColumnWidth = d->m_codeFoldingVisible ? foldBoxWidth() : 0;
            markWidth = d->m_marksVisible ? lineSpacing : 0;
        }
        palette.setCurrentColorGroup(QPalette::Active);
    }
    QTextBlock block;
    const QTextDocument *doc;
    const TextDocumentLayout *documentLayout;
    const int selectionStart;
    const int selectionEnd;
    const QFontMetrics fontMetrics;
    int lineSpacing;
    int markWidth;
    int collapseColumnWidth;
    const int extraAreaWidth;
    const QTextCharFormat currentLineNumberFormat;
    QPalette palette;
};


} // namespace Internal

} // namespace TextEditor

