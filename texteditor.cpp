// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

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


using namespace Internal;

namespace Internal {

enum { NExtraSelectionKinds = 12 };

using TransformationMethod = QString(const QString &);
using ListTransformationMethod = void(QStringList &);

static constexpr char dropProperty[] = "dropProp";

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

    TextEditorFactoryPrivate *m_origin = nullptr;
};

class HoverHandlerRunner
{
public:
    using Callback = std::function<void(TextEditorWidget *, BaseHoverHandler *, int)>;

    HoverHandlerRunner(TextEditorWidget *widget, QList<BaseHoverHandler *> &handlers)
        : m_widget(widget)
        , m_handlers(handlers)
    {
    }

    ~HoverHandlerRunner() { abortHandlers(); }

    void startChecking(const QTextCursor &textCursor, const Callback &callback)
    {
        if (m_handlers.empty())
            return;

        // Does the last handler still applies?
        const int documentRevision = textCursor.document()->revision();
        const int position = Text::wordStartCursor(textCursor).position();
        if (m_lastHandlerInfo.applies(documentRevision, position)) {
            callback(m_widget, m_lastHandlerInfo.handler, position);
            return;
        }

        if (isCheckRunning(documentRevision, position))
            return;

        // Update invocation data
        m_documentRevision = documentRevision;
        m_position = position;
        m_callback = callback;

        restart();
    }

    bool isCheckRunning(int documentRevision, int position) const
    {
        return m_currentHandlerIndex >= 0
            && m_documentRevision == documentRevision
            && m_position == position;
    }

    void checkNext()
    {
        QTC_ASSERT(m_currentHandlerIndex >= 0, return);
        QTC_ASSERT(m_currentHandlerIndex < m_handlers.size(), return);
        BaseHoverHandler *currentHandler = m_handlers[m_currentHandlerIndex];

        currentHandler->checkPriority(m_widget, m_position, [this](int priority) {
            onHandlerFinished(m_documentRevision, m_position, priority);
        });
    }

    void onHandlerFinished(int documentRevision, int position, int priority)
    {
        QTC_ASSERT(m_currentHandlerIndex >= 0, return);
        QTC_ASSERT(m_currentHandlerIndex < m_handlers.size(), return);
        QTC_ASSERT(documentRevision == m_documentRevision, return);
        QTC_ASSERT(position == m_position, return);

        BaseHoverHandler *currentHandler = m_handlers[m_currentHandlerIndex];
        if (priority > m_highestHandlerPriority) {
            m_highestHandlerPriority = priority;
            m_bestHandler = currentHandler;
        }

        // There are more, check next
        ++m_currentHandlerIndex;
        if (m_currentHandlerIndex < m_handlers.size()) {
            checkNext();
            return;
        }
        m_currentHandlerIndex = -1;

        // All were queried, run the best
        if (m_bestHandler) {
            m_lastHandlerInfo = LastHandlerInfo(m_bestHandler, m_documentRevision, m_position);
            m_callback(m_widget, m_bestHandler, m_position);
        }
    }

    void handlerRemoved(BaseHoverHandler *handler)
    {
        if (m_lastHandlerInfo.handler == handler)
            m_lastHandlerInfo = LastHandlerInfo();
        if (m_currentHandlerIndex >= 0)
            restart();
    }

    void abortHandlers()
    {
        for (BaseHoverHandler *handler : m_handlers)
            handler->abort();
        m_currentHandlerIndex = -1;
    }

private:
    void restart()
    {
        abortHandlers();

        if (m_handlers.empty())
            return;

        // Re-initialize process data
        m_currentHandlerIndex = 0;
        m_bestHandler = nullptr;
        m_highestHandlerPriority = -1;

        // Start checking
        checkNext();
    }

    TextEditorWidget *m_widget;
    const QList<BaseHoverHandler *> &m_handlers;

    struct LastHandlerInfo {
        LastHandlerInfo() = default;
        LastHandlerInfo(BaseHoverHandler *handler, int documentRevision, int cursorPosition)
            : handler(handler)
            , documentRevision(documentRevision)
            , cursorPosition(cursorPosition)
        {}

        bool applies(int documentRevision, int cursorPosition) const
        {
            return handler
                && documentRevision == this->documentRevision
                && cursorPosition == this->cursorPosition;
        }

        BaseHoverHandler *handler = nullptr;
        int documentRevision = -1;
        int cursorPosition = -1;
    } m_lastHandlerInfo;

    // invocation data
    Callback m_callback;
    int m_position = -1;
    int m_documentRevision = -1;

    // processing data
    int m_currentHandlerIndex = -1;
    int m_highestHandlerPriority = -1;
    BaseHoverHandler *m_bestHandler = nullptr;
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
    void copyLineUpDown(bool up);
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
    void setupScrollBar();
    void highlightSearchResultsInScrollBar();
    void scheduleUpdateHighlightScrollBar();
    void updateHighlightScrollBarNow();
    struct SearchResult {
        int start;
        int length;
    };
    void addSearchResultsToScrollBar(const QVector<SearchResult> &results);
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

public:
    TextEditorWidget *q;
    BaseTextFind *m_find = nullptr;

    //uint m_optionalActionMask = TextEditorActionHandler::None;
    bool m_contentsChanged = false;
    bool m_lastCursorChangeWasInteresting = false;

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
    QVector<SearchResult> m_searchResults;
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

TextEditorWidgetPrivate::TextEditorWidgetPrivate(TextEditorWidget *parent)
    : q(parent)
    , m_overlay(new TextEditorOverlay(q))
    , m_snippetOverlay(new SnippetOverlay(q))
    , m_searchResultOverlay(new TextEditorOverlay(q))
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
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

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

    layout->addWidget(DisplaySettings::createAnnotationSettingsLink(),
                      layout->rowCount(), 0, 1, -1, Qt::AlignRight);
    ToolTip::show(pos, layout, q);
}

} // namespace Internal



void TextEditorEnvironment::init(){
    Utils::Theme *theme = new Utils::Theme(Core::Constants::DEFAULT_DARK_THEME);
    //:/resource/themes/flat-light.creatortheme
    //:/resource/themes/flat-dark.creatortheme
    QSettings themeSettings(QString::fromUtf8(":/resource/themes/flat-light.creatortheme"), QSettings::IniFormat);
    theme->readSettings(themeSettings);

    Utils::setCreatorTheme(theme);
    //d->settings = new TextEditor::TextEditorSettings();
    //d->action_manager = new Core::ActionManager(this);
    TextEditor::SnippetProvider::registerGroup(TextEditor::Constants::TEXT_SNIPPET_GROUP_ID,QObject::tr("Text", "SnippetProvider"));
}
void TextEditorEnvironment::destory(){
    //delete d->settings;
    Utils::setCreatorTheme(nullptr);
}

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
Id TextEditorWidget::ObjCSelection("TextEdit.ObjCSelection");
Id TextEditorWidget::DebuggerExceptionSelection("TextEdit.DebuggerExceptionSelection");
Id TextEditorWidget::FakeVimSelection("TextEdit.FakeVimSelection");

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
    delete d;
    d = nullptr;
}

void TextEditorWidget::print(QPrinter *printer)
{

}

static int foldBoxWidth(const QFontMetrics &fm)
{
    const int lineSpacing = fm.lineSpacing();
    return lineSpacing + lineSpacing % 2 + 1;
}

static int foldBoxWidth()
{
    const int lineSpacing = TextEditorSettings::fontSettings().lineSpacing();
    return lineSpacing + lineSpacing % 2 + 1;
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

Core::BaseTextFind* TextEditorWidget::finder(){
    return d->m_find;
}

void TextEditorWidget::findText(const QString& text,int flags,bool hightlight){
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
    d->m_find->findStep(text,findFlags);
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

void TextEditorWidget::setVisualIndentOffset(int offset)
{
    d->m_visualIndentOffset = qMax(0, offset);
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

void TextEditorWidget::showContextMenu()
{
    QTextCursor tc = textCursor();
    const QPoint cursorPos = mapToGlobal(cursorRect(tc).bottomRight() + QPoint(1,1));
    qGuiApp->postEvent(this, new QContextMenuEvent(QContextMenuEvent::Keyboard, cursorPos));
}

void TextEditorWidget::copyLineUp()
{
    d->copyLineUpDown(true);
}

void TextEditorWidget::copyLineDown()
{
    d->copyLineUpDown(false);
}

// @todo: Potential reuse of some code around the following functions...
void TextEditorWidgetPrivate::copyLineUpDown(bool up)
{
    if (q->multiTextCursor().hasMultipleCursors())
        return;
    QTextCursor cursor = q->textCursor();
    QTextCursor move = cursor;
    move.beginEditBlock();

    bool hasSelection = cursor.hasSelection();

    if (hasSelection) {
        move.setPosition(cursor.selectionStart());
        move.movePosition(QTextCursor::StartOfBlock);
        move.setPosition(cursor.selectionEnd(), QTextCursor::KeepAnchor);
        move.movePosition(move.atBlockStart() ? QTextCursor::Left: QTextCursor::EndOfBlock,
                          QTextCursor::KeepAnchor);
    } else {
        move.movePosition(QTextCursor::StartOfBlock);
        move.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    }

    QString text = move.selectedText();

    if (up) {
        move.setPosition(cursor.selectionStart());
        move.movePosition(QTextCursor::StartOfBlock);
        move.insertBlock();
        move.movePosition(QTextCursor::Left);
    } else {
        move.movePosition(QTextCursor::EndOfBlock);
        if (move.atBlockStart()) {
            move.movePosition(QTextCursor::NextBlock);
            move.insertBlock();
            move.movePosition(QTextCursor::Left);
        } else {
            move.insertBlock();
        }
    }

    int start = move.position();
    move.clearSelection();
    move.insertText(text);
    int end = move.position();

    move.setPosition(start);
    move.setPosition(end, QTextCursor::KeepAnchor);

    m_document->autoIndent(move);
    move.endEditBlock();

    q->setTextCursor(move);
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
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
        c.insertBlock();
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

void TextEditorWidget::openLinkUnderCursor()
{
    d->openLinkUnderCursor(alwaysOpenLinksInNextSplit());
}

void TextEditorWidget::openLinkUnderCursorInNextSplit()
{
    d->openLinkUnderCursor(!alwaysOpenLinksInNextSplit());
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
    //qDebug()<<"TextEditorWidget::keyPressEvent:"<<e;
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
                return;
            }
            d->handleBackspaceKey();
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
        if (!inOverwriteMode) {
            const bool skipChar = d->m_skipAutoCompletedText
                    && !d->m_autoCompleteHighlightPos.isEmpty()
                    && cursor == d->m_autoCompleteHighlightPos.last();
            autoText = autoCompleter()->autoComplete(cursor, eventText, skipChar);
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
            cursor.setPosition(autoText.length() == 1 ? cursor.position() : cursor.anchor());

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
         if(ke->modifiers()==Qt::ControlModifier){
            int key = ke->key();
            if(key==Qt::Key_A){
                this->selectAll();
            }else if(key==Qt::Key_C){
                this->copy();
            }else if(key==Qt::Key_X){
                this->cut();
            }else if(key==Qt::Key_V){
                this->paste();
            }else if(key==Qt::Key_Y){
                this->redo();
            }else if(key==Qt::Key_Z){
                this->undo();
            }
         }else{
            if (ke->key() == Qt::Key_Escape
                && (d->m_snippetOverlay->isVisible() || multiTextCursor().hasMultipleCursors())) {
                e->accept();
            }else if(ke->key()==Qt::Key_Delete){
                break;
            }else {
                // hack copied from QInputControl::isCommonTextEditShortcut
                // Fixes: QTCREATORBUG-22854
                e->setAccepted((ke->modifiers() == Qt::NoModifier || ke->modifiers() == Qt::ShiftModifier
                                || ke->modifiers() == Qt::KeypadModifier)
                               && (ke->key() < Qt::Key_Escape));
                d->m_maybeFakeTooltipEvent = false;
            }
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
    //qDebug()<<"TextEditorWidget::event6:"<<e;

    return QPlainTextEdit::event(e);
}

void TextEditorWidget::contextMenuEvent(QContextMenuEvent *e)
{
    //showDefaultContextMenu(e, Id());
    /*QMenu contextMenu;

    contextMenu.exec(QCursor::pos());*/
    QPlainTextEdit::contextMenuEvent(e);
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

void TextEditorWidget::setParenthesesMatchingEnabled(bool b)
{
    d->m_parenthesesMatchingEnabled = b;
}

bool TextEditorWidget::isParenthesesMatchingEnabled() const
{
    return d->m_parenthesesMatchingEnabled;
}

void TextEditorWidget::setHighlightCurrentLine(bool b)
{
    d->m_highlightCurrentLine = b;
    d->updateCurrentLineHighlight();
}

bool TextEditorWidget::highlightCurrentLine() const
{
    return d->m_highlightCurrentLine;
}

void TextEditorWidget::setLineNumbersVisible(bool b)
{
    d->m_lineNumbersVisible = b;
    d->slotUpdateExtraAreaWidth();
}

bool TextEditorWidget::lineNumbersVisible() const
{
    return d->m_lineNumbersVisible;
}

void TextEditorWidget::setAlwaysOpenLinksInNextSplit(bool b)
{
    d->m_displaySettings.m_openLinksInNextSplit = b;
}

bool TextEditorWidget::alwaysOpenLinksInNextSplit() const
{
    return d->m_displaySettings.m_openLinksInNextSplit;
}

void TextEditorWidget::setMarksVisible(bool b)
{
    d->m_marksVisible = b;
    d->slotUpdateExtraAreaWidth();
}

bool TextEditorWidget::marksVisible() const
{
    return d->m_marksVisible;
}

void TextEditorWidget::setRequestMarkEnabled(bool b)
{
    d->m_requestMarkEnabled = b;
}

bool TextEditorWidget::requestMarkEnabled() const
{
    return d->m_requestMarkEnabled;
}

void TextEditorWidget::setLineSeparatorsAllowed(bool b)
{
    d->m_lineSeparatorsAllowed = b;
}

bool TextEditorWidget::lineSeparatorsAllowed() const
{
    return d->m_lineSeparatorsAllowed;
}

void TextEditorWidgetPrivate::updateCodeFoldingVisible()
{
    const bool visible = m_codeFoldingSupported && m_displaySettings.m_displayFoldingMarkers;
    if (m_codeFoldingVisible != visible) {
        m_codeFoldingVisible = visible;
        slotUpdateExtraAreaWidth();
    }
}

void TextEditorWidgetPrivate::updateFileLineEndingVisible()
{
    //m_fileLineEndingAction->setVisible(m_displaySettings.m_displayFileLineEnding && !q->isReadOnly());
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

void TextEditorWidgetPrivate::configureGenericHighlighter(
    const KSyntaxHighlighting::Definition &definition)
{
    auto highlighter = new Highlighter();
    m_document->setSyntaxHighlighter(highlighter);

    if (definition.isValid()) {
        highlighter->setDefinition(definition);
        setupFromDefinition(definition);
    } else {
        q->setCodeFoldingSupported(false);
    }

    m_document->setFontSettings(TextEditorSettings::fontSettings());
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

void TextEditorWidgetPrivate::openLinkUnderCursor(bool openInNextSplit)
{
    q->findLinkAt(q->textCursor(),
               [openInNextSplit, self = QPointer<TextEditorWidget>(q)](const Link &symbolLink) {
        if (self)
            self->openLink(symbolLink, openInNextSplit);
    }, true, openInNextSplit);
}

bool TextEditorWidget::codeFoldingVisible() const
{
    return d->m_codeFoldingVisible;
}

/**
 * Sets whether code folding is supported by the syntax highlighter. When not
 * supported (the default), this makes sure the code folding is not shown.
 *
 * Needs to be called before calling setCodeFoldingVisible.
 */
void TextEditorWidget::setCodeFoldingSupported(bool b)
{
    d->m_codeFoldingSupported = b;
    d->updateCodeFoldingVisible();
}

bool TextEditorWidget::codeFoldingSupported() const
{
    return d->m_codeFoldingSupported;
}

void TextEditorWidget::setMouseNavigationEnabled(bool b)
{
    d->m_behaviorSettings.m_mouseNavigation = b;
}

bool TextEditorWidget::mouseNavigationEnabled() const
{
    return d->m_behaviorSettings.m_mouseNavigation;
}

void TextEditorWidget::setMouseHidingEnabled(bool b)
{
    d->m_behaviorSettings.m_mouseHiding = b;
}

bool TextEditorWidget::mouseHidingEnabled() const
{
    return Utils::HostOsInfo::isMacHost() ? false : d->m_behaviorSettings.m_mouseHiding;
}

void TextEditorWidget::setScrollWheelZoomingEnabled(bool b)
{
    d->m_behaviorSettings.m_scrollWheelZooming = b;
}

bool TextEditorWidget::scrollWheelZoomingEnabled() const
{
    return d->m_behaviorSettings.m_scrollWheelZooming;
}

void TextEditorWidget::setConstrainTooltips(bool b)
{
    d->m_behaviorSettings.m_constrainHoverTooltips = b;
}

bool TextEditorWidget::constrainTooltips() const
{
    return d->m_behaviorSettings.m_constrainHoverTooltips;
}

void TextEditorWidget::setCamelCaseNavigationEnabled(bool b)
{
    d->m_behaviorSettings.m_camelCaseNavigation = b;
}

bool TextEditorWidget::camelCaseNavigationEnabled() const
{
    return d->m_behaviorSettings.m_camelCaseNavigation;
}

void TextEditorWidget::setRevisionsVisible(bool b)
{
    d->m_revisionsVisible = b;
    d->slotUpdateExtraAreaWidth();
}

bool TextEditorWidget::revisionsVisible() const
{
    return d->m_revisionsVisible;
}

void TextEditorWidget::setVisibleWrapColumn(int column)
{
    d->m_visibleWrapColumn = column;
    viewport()->update();
}

int TextEditorWidget::visibleWrapColumn() const
{
    return d->m_visibleWrapColumn;
}

void TextEditorWidget::setAutoCompleter(AutoCompleter *autoCompleter)
{
    d->m_autoCompleter.reset(autoCompleter);
}

AutoCompleter *TextEditorWidget::autoCompleter() const
{
    return d->m_autoCompleter.data();
}

//
// TextEditorWidgetPrivate
//

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

    if (m_hoverHandlers.isEmpty()) {
        emit q->tooltipRequested(toolTipPoint, c.position());
        return;
    }

    const auto callback = [toolTipPoint](TextEditorWidget *widget, BaseHoverHandler *handler, int) {
        handler->showToolTip(widget, toolTipPoint);
    };
    m_hoverHandlerRunner.startChecking(c, callback);
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
            if (pos.x() <= blockBoundingGeometry(block).left() + line.naturalTextRect().right()) {
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
}

void TextEditorWidgetPrivate::moveCursor(QTextCursor::MoveOperation operation,
                                         QTextCursor::MoveMode mode)
{
    MultiTextCursor cursor = m_cursors;
    cursor.movePosition(operation, mode);
    q->setMultiTextCursor(cursor);
}

QRect TextEditorWidgetPrivate::cursorUpdateRect(const MultiTextCursor &cursor)
{
    QRect result(0, 0, 0, 0);
    for (const QTextCursor &c : cursor)
        result |= q->cursorRect(c);
    return result;
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
    const QTextLayout *layout = block.layout();
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

static QTextBlock nextVisibleBlock(const QTextBlock &block,
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
    for (int i = 0; i < data.context.selections.size(); ++i) {
        const QAbstractTextDocumentLayout::Selection &range = data.context.selections.at(i);
        const int selStart = range.cursor.selectionStart() - blockData.position;
        const int selEnd = range.cursor.selectionEnd() - blockData.position;
        if (selStart < blockData.length && selEnd >= 0
            && selEnd >= selStart) {
            QTextLayout::FormatRange o;
            o.start = selStart;
            o.length = selEnd - selStart;
            o.format = range.format;
            if (data.textCursor.hasSelection() && data.textCursor == range.cursor
                && data.textCursor.anchor() == range.cursor.anchor()) {
                const QTextCharFormat selectionFormat = data.fontSettings.toTextCharFormat(C_SELECTION);
                if (selectionFormat.background().style() != Qt::NoBrush)
                    o.format.setBackground(selectionFormat.background());
                o.format.setForeground(selectionFormat.foreground());
            }
            if ((data.textCursor.hasSelection() && i == data.context.selections.size() - 1)
                || (o.format.foreground().style() == Qt::NoBrush
                && o.format.underlineStyle() != QTextCharFormat::NoUnderline
                && o.format.background() == Qt::NoBrush)) {
                if (q->selectionVisible(data.block.blockNumber()))
                    prioritySelections.append(o);
            } else {
                blockData.selections.append(o);
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
    data.context.palette.setBrush(QPalette::Text, textFormat.foreground());
    data.context.palette.setBrush(QPalette::Base, textFormat.background());

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
            if (data.isEditable && data.context.cursorPosition < -1
                && !blockData.layout->preeditAreaText().isEmpty()) {
                const int cursorPos = blockData.layout->preeditAreaPosition()
                                           - (data.context.cursorPosition + 2);
                data.cursors.append(generateCursorData(cursorPos, data, blockData, painter));
            }
            if (drawCursor && !drawCursorAsBlock)
                d->addCursorsPosition(data, painter, blockData);
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

    // draw the cursor last, on top of everything
    d->paintCursor(data, painter);
    // paint a popup with the content of the collapsed block
    drawCollapsedBlockPopup(painter, data.visibleCollapsedBlock,
                            data.visibleCollapsedBlockOffset, data.eventRect);
}

void TextEditorWidget::paintBlock(QPainter *painter,
                                  const QTextBlock &block,
                                  const QPointF &offset,
                                  const QVector<QTextLayout::FormatRange> &selections,
                                  const QRect &clipRect) const
{
    block.layout()->draw(painter, offset, selections, clipRect);
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

    space += 4;

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

struct Internal::ExtraAreaPaintEventData
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

void TextEditorWidgetPrivate::saveCurrentCursorPositionForNavigation()
{
    m_lastCursorChangeWasInteresting = true;
    m_tempNavigationState = q->saveState();
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
    /*const QTextCursor plainTextEditCursor = textCursor();
    const QTextCursor multiMainCursor = multiTextCursor().mainCursor();
    if (multiMainCursor.position() != plainTextEditCursor.position()
        || multiMainCursor.anchor() != plainTextEditCursor.anchor()) {
        doSetTextCursor(plainTextEditCursor, true);
    }*/
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

void TextEditorWidgetPrivate::setClipboardSelection()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (m_cursors.hasSelection() && clipboard->supportsSelection())
        clipboard->setMimeData(q->createMimeDataFromSelection(), QClipboard::Selection);
}

void TextEditorWidget::leaveEvent(QEvent *e)
{
    // Clear link emulation when the mouse leaves the editor
    d->clearLink();
    QPlainTextEdit::leaveEvent(e);
}

void TextEditorWidget::keyReleaseEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Control) {
        d->clearLink();
    } else if (e->key() == Qt::Key_Shift
             && d->m_behaviorSettings.m_constrainHoverTooltips
             && ToolTip::isVisible()) {
        ToolTip::hide();
    } else if (e->key() == Qt::Key_Alt
               && d->m_maybeFakeTooltipEvent) {
        d->m_maybeFakeTooltipEvent = false;
        d->processTooltipRequest(textCursor());
    }

    QPlainTextEdit::keyReleaseEvent(e);
}

void TextEditorWidget::dragEnterEvent(QDragEnterEvent *e)
{
    // If the drag event contains URLs, we don't want to insert them as text
    if (e->mimeData()->hasUrls()) {
        e->ignore();
        return;
    }

    QPlainTextEdit::dragEnterEvent(e);
}

static void appendMenuActionsFromContext(QMenu *menu, Id menuContextId)
{
    /*ActionContainer *mcontext = ActionManager::actionContainer(menuContextId);
    if(mcontext==nullptr)
        return;
    QMenu *contextMenu = mcontext->menu();

    const QList<QAction *> actions = contextMenu->actions();
    for (QAction *action : actions)
        menu->addAction(action);*/
}



#ifdef WITH_TESTS
void TextEditorWidget::processTooltipRequest(const QTextCursor &c)
{
    d->processTooltipRequest(c);
}
#endif

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

void TextEditorWidget::ensureCursorVisible()
{
    ensureBlockIsUnfolded(textCursor().block());
    QPlainTextEdit::ensureCursorVisible();
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

void TextEditorWidget::setLanguageSettingsId(Id settingsId)
{
    d->m_tabSettingsId = settingsId;
    //setCodeStyle(TextEditorSettings::codeStyle(settingsId));
}

Id TextEditorWidget::languageSettingsId() const
{
    return d->m_tabSettingsId;
}

void TextEditorWidget::setCodeStyle(ICodeStylePreferences *preferences)
{
    TextDocument *document = d->m_document.data();
    // Not fully initialized yet... wait for TextEditorWidgetPrivate::setupDocumentSignals
    if (!document)
        return;
    document->indenter()->setCodeStylePreferences(preferences);
    if (d->m_codeStylePreferences) {
        disconnect(d->m_codeStylePreferences, &ICodeStylePreferences::currentTabSettingsChanged,
                   document, &TextDocument::setTabSettings);
        disconnect(d->m_codeStylePreferences, &ICodeStylePreferences::currentValueChanged,
                   this, &TextEditorWidget::slotCodeStyleSettingsChanged);
    }
    d->m_codeStylePreferences = preferences;
    if (d->m_codeStylePreferences) {
        connect(d->m_codeStylePreferences, &ICodeStylePreferences::currentTabSettingsChanged,
                document, &TextDocument::setTabSettings);
        connect(d->m_codeStylePreferences, &ICodeStylePreferences::currentValueChanged,
                this, &TextEditorWidget::slotCodeStyleSettingsChanged);
        document->setTabSettings(d->m_codeStylePreferences->currentTabSettings());
        slotCodeStyleSettingsChanged(d->m_codeStylePreferences->currentValue());
    }
}

void TextEditorWidget::slotCodeStyleSettingsChanged(const QVariant &)
{

}

const DisplaySettings &TextEditorWidget::displaySettings() const
{
    return d->m_displaySettings;
}

const MarginSettings &TextEditorWidget::marginSettings() const
{
    return d->m_marginSettings;
}

const BehaviorSettings &TextEditorWidget::behaviorSettings() const
{
    return d->m_behaviorSettings;
}

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

void TextEditorWidgetPrivate::setFindScope(const Utils::MultiTextCursor &scope)
{
    if (m_findScope != scope) {
        m_findScope = scope;
        q->viewport()->update();
        highlightSearchResultsInScrollBar();
    }
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
    d->m_hoverHandlerRunner.abortHandlers();
    if (viewport()->cursor().shape() == Qt::BlankCursor)
        viewport()->setCursor(Qt::IBeamCursor);
    d->m_cursorFlashTimer.stop();
    if (d->m_cursorVisible) {
        d->m_cursorVisible = false;
        viewport()->update(d->cursorUpdateRect(d->m_cursors));
    }
    d->updateHighlights();
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

    /*const QTextDocument::FindFlags findFlags = textDocumentFlagsForFindFlags(m_findFlags);

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
    }*/
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
    cursor.beginEditBlock();
    d->m_document->autoFormat(cursor);
    cursor.endEditBlock();
}

void TextEditorWidget::encourageApply()
{
    if (!d->m_snippetOverlay->isVisible() || d->m_snippetOverlay->isEmpty())
        return;
    d->m_snippetOverlay->updateEquivalentSelections(textCursor());
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
    }
    if(d->m_hightlighted==false){
        QTimer::singleShot(0,[this]{
            this->configureGenericHighlighter();
            //this->setFocus();
            //this->gotoLine(10);

        });
        d->m_hightlighted = true;
    }

}

void TextEditorWidgetPrivate::applyFontSettingsDelayed()
{
    m_fontSettingsNeedsApply = true;
    if (q->isVisible())
        q->triggerPendingUpdates();
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

void TextEditorWidget::triggerPendingUpdates()
{
    if (d->m_fontSettingsNeedsApply)
        applyFontSettings();
    textDocument()->triggerPendingUpdates();
}

void TextEditorWidget::applyFontSettings()
{
    d->m_fontSettingsNeedsApply = false;
    const FontSettings &fs = textDocument()->fontSettings();
    const QTextCharFormat textFormat = fs.toTextCharFormat(C_TEXT);
    const QTextCharFormat lineNumberFormat = fs.toTextCharFormat(C_LINE_NUMBER);
    QFont font(textFormat.font());

    if (font != this->font()) {
        setFont(font);
        d->updateTabStops(); // update tab stops, they depend on the font
    } else if (font != document()->defaultFont()) {
        // When the editor already have the correct font configured it wont generate a font change
        // signal. In turn the default font of the document wont get updated so we need to do that
        // manually here
        document()->setDefaultFont(font);
    }

    // Line numbers
    QPalette ep;
    ep.setColor(QPalette::Dark, lineNumberFormat.foreground().color());
    ep.setColor(QPalette::Window, lineNumberFormat.background().style() != Qt::NoBrush ?
                lineNumberFormat.background().color() : textFormat.background().color());
    if (ep != d->m_extraArea->palette()) {
        d->m_extraArea->setPalette(ep);
        d->slotUpdateExtraAreaWidth();   // Adjust to new font width
    }

    d->updateHighlights();
}

void TextEditorWidget::setDisplaySettings(const DisplaySettings &ds)
{
    const TextEditor::FontSettings &fs = TextEditorSettings::fontSettings();
    if (fs.relativeLineSpacing() == 100)
        setLineWrapMode(ds.m_textWrapping ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    else
        setLineWrapMode(QPlainTextEdit::NoWrap);

    QTC_ASSERT((fs.relativeLineSpacing() == 100) || (fs.relativeLineSpacing() != 100
        && lineWrapMode() == QPlainTextEdit::NoWrap), setLineWrapMode(QPlainTextEdit::NoWrap));

    setLineNumbersVisible(ds.m_displayLineNumbers);
    setHighlightCurrentLine(ds.m_highlightCurrentLine);
    setRevisionsVisible(ds.m_markTextChanges);
    setCenterOnScroll(ds.m_centerCursorOnScroll);
    setParenthesesMatchingEnabled(ds.m_highlightMatchingParentheses);
    //d->m_fileEncodingLabelAction->setVisible(ds.m_displayFileEncoding);

    const QTextOption::Flags currentOptionFlags = document()->defaultTextOption().flags();
    QTextOption::Flags optionFlags = currentOptionFlags;
    optionFlags.setFlag(QTextOption::AddSpaceForLineAndParagraphSeparators);
    optionFlags.setFlag(QTextOption::ShowTabsAndSpaces, ds.m_visualizeWhitespace);
    if (optionFlags != currentOptionFlags) {
        if (SyntaxHighlighter *highlighter = textDocument()->syntaxHighlighter())
            highlighter->rehighlight();
        QTextOption option = document()->defaultTextOption();
        option.setFlags(optionFlags);
        document()->setDefaultTextOption(option);
    }

    d->m_displaySettings = ds;
    if (!ds.m_highlightBlocks) {
        d->extraAreaHighlightFoldedBlockNumber = -1;
        d->m_highlightBlocksInfo = TextEditorPrivateHighlightBlocks();
    }

    d->updateCodeFoldingVisible();
    d->updateFileLineEndingVisible();
    d->updateHighlights();
    d->setupScrollBar();
    viewport()->update();
    extraArea()->update();
}

void TextEditorWidget::setMarginSettings(const MarginSettings &ms)
{
    d->m_marginSettings = ms;
    updateVisualWrapColumn();

    viewport()->update();
    extraArea()->update();
}

void TextEditorWidget::setBehaviorSettings(const BehaviorSettings &bs)
{
    d->m_behaviorSettings = bs;
}

void TextEditorWidget::setTypingSettings(const TypingSettings &typingSettings)
{
    d->m_document->setTypingSettings(typingSettings);
}

void TextEditorWidget::setStorageSettings(const StorageSettings &storageSettings)
{
    d->m_document->setStorageSettings(storageSettings);
}

void TextEditorWidget::setCompletionSettings(const CompletionSettings &completionSettings)
{
    d->m_autoCompleter->setAutoInsertBracketsEnabled(completionSettings.m_autoInsertBrackets);
    d->m_autoCompleter->setSurroundWithBracketsEnabled(completionSettings.m_surroundingAutoBrackets);
    d->m_autoCompleter->setAutoInsertQuotesEnabled(completionSettings.m_autoInsertQuotes);
    d->m_autoCompleter->setSurroundWithQuotesEnabled(completionSettings.m_surroundingAutoQuotes);
    d->m_autoCompleter->setOverwriteClosingCharsEnabled(completionSettings.m_overwriteClosingChars);
    d->m_animateAutoComplete = completionSettings.m_animateAutoComplete;
    d->m_highlightAutoComplete = completionSettings.m_highlightAutoComplete;
    d->m_skipAutoCompletedText = completionSettings.m_skipAutoCompletedText;
    d->m_removeAutoCompletedText = completionSettings.m_autoRemove;
}

void TextEditorWidget::setExtraEncodingSettings(const ExtraEncodingSettings &extraEncodingSettings)
{
    d->m_document->setExtraEncodingSettings(extraEncodingSettings);
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

void TextEditorWidget::setReadOnly(bool b)
{
    QPlainTextEdit::setReadOnly(b);
    emit readOnlyChanged();
    if (b)
        setTextInteractionFlags(textInteractionFlags() | Qt::TextSelectableByKeyboard);
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

void TextEditorWidget::switchUtf8bom()
{
    textDocument()->switchUtf8Bom();
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

void TextEditorWidget::setupFallBackEditor(Id id)
{
    TextDocumentPtr doc(new TextDocument(id));
    doc->setFontSettings(TextEditorSettings::fontSettings());
    setTextDocument(doc);
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



void TextEditorWidgetPrivate::updateCursorPosition()
{
    //m_contextHelpItem = HelpItem();
    if (!q->textCursor().block().isVisible())
        q->ensureCursorVisible();
}

/*void BaseTextEditor::contextHelp(const HelpCallback &callback) const
{
    editorWidget()->contextHelpItem(callback);
}

void BaseTextEditor::setContextHelp(const HelpItem &item)
{
    IEditor::setContextHelp(item);
    editorWidget()->setContextHelpItem(item);
}

void TextEditorWidget::contextHelpItem(const IContext::HelpCallback &callback)
{
    if (!d->m_contextHelpItem.isEmpty()) {
        callback(d->m_contextHelpItem);
        return;
    }
    const QString fallbackWordUnderCursor = Text::wordUnderCursor(textCursor());
    if (d->m_hoverHandlers.isEmpty()) {
        callback(fallbackWordUnderCursor);
        return;
    }

    const auto hoverHandlerCallback = [fallbackWordUnderCursor, callback](
            TextEditorWidget *widget, BaseHoverHandler *handler, int position) {
        handler->contextHelpId(widget, position,
                               [fallbackWordUnderCursor, callback](const HelpItem &item) {
            if (item.isEmpty())
                callback(fallbackWordUnderCursor);
            else
                callback(item);
        });

    };
    d->m_hoverHandlerRunner.startChecking(textCursor(), hoverHandlerCallback);
}

void TextEditorWidget::setContextHelpItem(const HelpItem &item)
{
    d->m_contextHelpItem = item;
}*/

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

bool TextEditorWidget::inFindScope(const QTextCursor &cursor) const
{
    return d->m_find->inScope(cursor);
    //return false;
}

void TextEditorWidget::updateVisualWrapColumn()
{
    auto calcMargin = [this] {
        const auto &ms = d->m_marginSettings;

        if (!ms.m_showMargin) {
            return 0;
        }
        if (ms.m_useIndenter) {
            if (auto margin = d->m_document->indenter()->margin()) {
                return *margin;
            }
        }
        return ms.m_marginColumn;
    };
    setVisibleWrapColumn(calcMargin());
}

void TextEditorWidgetPrivate::updateTabStops()
{
    // Although the tab stop is stored as qreal the API from QPlainTextEdit only allows it
    // to be set as an int. A work around is to access directly the QTextOption.
    qreal charWidth = QFontMetricsF(q->font()).horizontalAdvance(QLatin1Char(' '));
    QTextOption option = q->document()->defaultTextOption();
    option.setTabStopDistance(charWidth * m_document->tabSettings().m_tabSize);
    q->document()->setDefaultTextOption(option);
}

void TextEditorWidgetPrivate::applyTabSettings()
{
    updateTabStops();
    m_autoCompleter->setTabSettings(m_document->tabSettings());
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

void TextEditorWidget::inSnippetMode(bool *active)
{
    *active = d->m_snippetOverlay->isVisible();
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

QString TextEditorWidget::foldReplacementText(const QTextBlock &) const
{
    return QLatin1String("...");
}


QChar TextEditorWidget::characterAt(int pos) const
{
    return textDocument()->characterAt(pos);
}

QString TextEditorWidget::textAt(int from, int to) const
{
    return textDocument()->textAt(from, to);
}

void TextEditorWidget::configureGenericHighlighter()
{
    Highlighter::Definitions definitions = Highlighter::definitionsForDocument(textDocument());
    d->configureGenericHighlighter(definitions.isEmpty() ? Highlighter::Definition() : definitions.first());
    d->updateSyntaxInfoBar(definitions, textDocument()->filePath().fileName());
    //update
    if(!definitions.isEmpty()){
        //DocumentContentCompletionProvider keywords functions
    }
}

/*void TextEditorWidget::configureGenericHighlighter(const Utils::MimeType &mimeType)
{
    Highlighter::Definitions definitions = Highlighter::definitionsForMimeType(mimeType.name());
    d->configureGenericHighlighter(definitions.isEmpty() ? Highlighter::Definition()
                                                         : definitions.first());
    d->removeSyntaxInfoBar();
}*/

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

HighlightScrollBarController *TextEditorWidget::highlightScrollBarController() const
{
    return d->m_highlightScrollBarController;
}

// The remnants of PlainTextEditor.
void TextEditorWidget::setupGenericHighlighter()
{
    setLineSeparatorsAllowed(true);

    connect(textDocument(), &IDocument::filePathChanged,
            d, &TextEditorWidgetPrivate::reconfigure);
}




namespace Internal {



} /// namespace Internal


} // namespace TextEditor

QT_BEGIN_NAMESPACE

size_t qHash(const QColor &color)
{
    return color.rgba();
}

QT_END_NAMESPACE

#include "texteditor.moc"
