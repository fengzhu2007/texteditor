// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"

//#include "blockrange.h"
#include "codeassist/assistenums.h"
#include "indenter.h"
#include "refactoroverlay.h"
#include "snippets/snippetparser.h"



//#include "core/ieditor.h"
//#include "core/ieditorfactory.h"
#include "core/find/textfindconstants.h"


#include "utils/link.h"
#include "utils/multitextcursor.h"
//#include "utils/mimeutils.h"

#include <QPlainTextEdit>
#include <QSharedPointer>
#include <functional>

QT_BEGIN_NAMESPACE
class QToolBar;
class QPrinter;
class QMenu;
class QPainter;
class QPoint;
class QRect;
class QTextBlock;
QT_END_NAMESPACE

namespace Core {
class HighlightScrollBarController;
class BaseTextFind;
}

namespace TextEditor {
class TextDocument;
class TextMark;
class RefactorOverlay;
class SyntaxHighlighter;
class AssistInterface;
class IAssistProvider;
class ICodeStylePreferences;
class CompletionAssistProvider;
using RefactorMarkers = QList<RefactorMarker>;
using TextMarks = QList<TextMark *>;

namespace Internal {
class BaseTextEditorPrivate;
class TextEditorWidgetPrivate;
class TextEditorOverlay;
}

class AutoCompleter;
//class BaseTextEditor;
class TextEditorWidget;

class BehaviorSettings;
class CompletionSettings;
class DisplaySettings;
class ExtraEncodingSettings;
class FontSettings;
class MarginSettings;
class StorageSettings;
class TypingSettings;


enum TextMarkRequestKind
{
    BreakpointRequest,
    BookmarkRequest,
    TaskMarkRequest
};

class TEXTEDITOR_EXPORT TextEditorEnvironment{
public:
    static void init();
    static void destory();
};

class TEXTEDITOR_EXPORT TextEditorWidget : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit TextEditorWidget(QWidget *parent = nullptr);
    ~TextEditorWidget() override;

    void setTextDocument(const QSharedPointer<TextDocument> &doc);
    TextDocument *textDocument() const;
    QSharedPointer<TextDocument> textDocumentPtr() const;

    virtual void aboutToOpen(const Utils::FilePath &filePath, const Utils::FilePath &realFilePath);
    virtual void openFinishedSuccessfully();
    // IEditor
    QByteArray saveState() const;
    virtual void restoreState(const QByteArray &state);
    void gotoLine(int line, int column = 0, bool centerLine = true, bool animate = false);
    int position(TextPositionOperation posOp = CurrentPosition,
         int at = -1) const;
    void convertPosition(int pos, int *line, int *column) const;
    using QPlainTextEdit::cursorRect;
    QRect cursorRect(int pos) const;
    void setCursorPosition(int pos);

    void print(QPrinter *);



    void setAutoCompleter(AutoCompleter *autoCompleter);
    AutoCompleter *autoCompleter() const;

    // Works only in conjunction with a syntax highlighter that puts
    // parentheses into text block user data
    void setParenthesesMatchingEnabled(bool b);
    bool isParenthesesMatchingEnabled() const;

    void setHighlightCurrentLine(bool b);
    bool highlightCurrentLine() const;

    void setLineNumbersVisible(bool b);
    bool lineNumbersVisible() const;

    void setAlwaysOpenLinksInNextSplit(bool b);
    bool alwaysOpenLinksInNextSplit() const;

    void setMarksVisible(bool b);
    bool marksVisible() const;

    void setRequestMarkEnabled(bool b);
    bool requestMarkEnabled() const;

    void setLineSeparatorsAllowed(bool b);
    bool lineSeparatorsAllowed() const;

    bool codeFoldingVisible() const;

    void setCodeFoldingSupported(bool b);
    bool codeFoldingSupported() const;

    void setMouseNavigationEnabled(bool b);
    bool mouseNavigationEnabled() const;

    void setMouseHidingEnabled(bool b);
    bool mouseHidingEnabled() const;

    void setScrollWheelZoomingEnabled(bool b);
    bool scrollWheelZoomingEnabled() const;

    void setConstrainTooltips(bool b);
    bool constrainTooltips() const;

    void setCamelCaseNavigationEnabled(bool b);
    bool camelCaseNavigationEnabled() const;

    void setRevisionsVisible(bool b);
    bool revisionsVisible() const;

    void setVisibleWrapColumn(int column);
    int visibleWrapColumn() const;

    int columnCount() const;
    int rowCount() const;

    void setReadOnly(bool b);

    void insertCodeSnippet(const QTextCursor &cursor,
                           const QString &snippet,
                           const SnippetParser &parse);

    Utils::MultiTextCursor multiTextCursor() const;
    void setMultiTextCursor(const Utils::MultiTextCursor &cursor);

    QRegion translatedLineRegion(int lineStart, int lineEnd) const;

    QPoint toolTipPosition(const QTextCursor &c) const;
    void showTextMarksToolTip(const QPoint &pos,
                              const TextMarks &marks,
                              const TextMark *mainTextMark = nullptr) const;

    void invokeAssist(AssistKind assistKind, IAssistProvider *provider = nullptr);

    virtual TextEditor::AssistInterface *createAssistInterface(AssistKind assistKind,
                                                    AssistReason assistReason) const;
    static QMimeData *duplicateMimeData(const QMimeData *source);

    static QString msgTextTooLarge(quint64 size);

    void insertPlainText(const QString &text);

    QWidget *extraArea() const;
    virtual int extraAreaWidth(int *markWidthPtr = nullptr) const;
    virtual void extraAreaPaintEvent(QPaintEvent *);
    virtual void extraAreaLeaveEvent(QEvent *);
    virtual void extraAreaContextMenuEvent(QContextMenuEvent *);
    virtual void extraAreaMouseEvent(QMouseEvent *);
    void updateFoldingHighlight(const QPoint &pos);

    void setLanguageSettingsId(Utils::Id settingsId);
    Utils::Id languageSettingsId() const;

    void setCodeStyle(ICodeStylePreferences *settings);

    const DisplaySettings &displaySettings() const;
    const MarginSettings &marginSettings() const;
    const BehaviorSettings &behaviorSettings() const;

    void ensureCursorVisible();
    void ensureBlockIsUnfolded(QTextBlock block);

    static Utils::Id FakeVimSelection;
    static Utils::Id SnippetPlaceholderSelection;
    static Utils::Id CurrentLineSelection;
    static Utils::Id ParenthesesMatchingSelection;
    static Utils::Id AutoCompleteSelection;
    static Utils::Id CodeWarningsSelection;
    static Utils::Id CodeSemanticsSelection;
    static Utils::Id CursorSelection;
    static Utils::Id UndefinedSymbolSelection;
    static Utils::Id UnusedSymbolSelection;
    static Utils::Id OtherSelection;
    static Utils::Id ObjCSelection;
    static Utils::Id DebuggerExceptionSelection;

    void setExtraSelections(Utils::Id kind, const QList<QTextEdit::ExtraSelection> &selections);
    QList<QTextEdit::ExtraSelection> extraSelections(Utils::Id kind) const;
    QString extraSelectionTooltip(int pos) const;

    RefactorMarkers refactorMarkers() const;
    void setRefactorMarkers(const RefactorMarkers &markers);


    // keep the auto completion even if the focus is lost
    void keepAutoCompletionHighlight(bool keepHighlight);
    void setAutoCompleteSkipPosition(const QTextCursor &cursor);

    virtual void copy();
    virtual void paste();
    virtual void cut();
    virtual void selectAll();

    virtual void autoIndent();
    virtual void rewrapParagraph();
    virtual void unCommentSelection();

    virtual void autoFormat();

    virtual void encourageApply();

    virtual void setDisplaySettings(const TextEditor::DisplaySettings &);
    virtual void setMarginSettings(const TextEditor::MarginSettings &);
    void setBehaviorSettings(const TextEditor::BehaviorSettings &);
    void setTypingSettings(const TextEditor::TypingSettings &);
    void setStorageSettings(const TextEditor::StorageSettings &);
    void setCompletionSettings(const TextEditor::CompletionSettings &);
    void setExtraEncodingSettings(const TextEditor::ExtraEncodingSettings &);

    void circularPaste();
    void pasteWithoutFormat();
    void switchUtf8bom();

    void zoomF(float delta);
    void zoomReset();

    void cutLine();
    void copyLine();
    void copyWithHtml();
    void duplicateSelection();
    void duplicateSelectionAndComment();
    void deleteLine();
    void deleteEndOfLine();
    void deleteEndOfWord();
    void deleteEndOfWordCamelCase();
    void deleteStartOfLine();
    void deleteStartOfWord();
    void deleteStartOfWordCamelCase();
    void unfoldAll();
    void fold();
    void unfold();
    void selectEncoding();
    void updateTextCodecLabel();
    void selectLineEnding(int index);
    void updateTextLineEndingLabel();
    void addSelectionNextFindMatch();
    void addCursorsToLineEnds();

    void gotoBlockStart();
    void gotoBlockEnd();
    void gotoBlockStartWithSelection();
    void gotoBlockEndWithSelection();

    void gotoDocumentStart();
    void gotoDocumentEnd();
    void gotoLineStart();
    void gotoLineStartWithSelection();
    void gotoLineEnd();
    void gotoLineEndWithSelection();
    void gotoNextLine();
    void gotoNextLineWithSelection();
    void gotoPreviousLine();
    void gotoPreviousLineWithSelection();
    void gotoPreviousCharacter();
    void gotoPreviousCharacterWithSelection();
    void gotoNextCharacter();
    void gotoNextCharacterWithSelection();
    void gotoPreviousWord();
    void gotoPreviousWordWithSelection();
    void gotoNextWord();
    void gotoNextWordWithSelection();
    void gotoPreviousWordCamelCase();
    void gotoPreviousWordCamelCaseWithSelection();
    void gotoNextWordCamelCase();
    void gotoNextWordCamelCaseWithSelection();

    virtual bool selectBlockUp();
    virtual bool selectBlockDown();
    void selectWordUnderCursor();

    void showContextMenu();

    void moveLineUp();
    void moveLineDown();

    void viewPageUp();
    void viewPageDown();
    void viewLineUp();
    void viewLineDown();


    void joinLines();

    void insertLineAbove();
    void insertLineBelow();

    void uppercaseSelection();
    void lowercaseSelection();

    void sortSelectedLines();

    void cleanWhitespace();

    void indent();
    void unindent();

    void undo();
    void redo();

    void openLinkUnderCursor();
    void openLinkUnderCursorInNextSplit();

    virtual void findUsages();
    virtual void renameSymbolUnderCursor();

    /// Abort code assistant if it is running.
    void abortAssist();

    /// Overwrite the current highlighter with a new generic highlighter based on the mimetype of
    /// the current document
    void configureGenericHighlighter();
    /// Overwrite the current highlighter with a new generic highlighter based on the given mimetype
    //void configureGenericHighlighter(const Utils::MimeType &mimeType);

    Q_INVOKABLE void inSnippetMode(bool *active); // Used by FakeVim.

    /*! Returns the document line number for the visible \a row.
     *
     * The first visible row is 0, the last visible row is rowCount() - 1.
     *
     * Any invalid row will return -1 as line number.
     */
    int blockNumberForVisibleRow(int row) const;

    /*! Returns the first visible line of the document. */
    int firstVisibleBlockNumber() const;
    /*! Returns the last visible line of the document. */
    int lastVisibleBlockNumber() const;
    /*! Returns the line visible closest to the vertical center of the editor. */
    int centerVisibleBlockNumber() const;

    Core::HighlightScrollBarController *highlightScrollBarController() const;


#ifdef WITH_TESTS
    void processTooltipRequest(const QTextCursor &c);
#endif

signals:
    void assistFinished(); // Used in tests.
    void readOnlyChanged();

    void requestBlockUpdate(const QTextBlock &);

    void requestLinkAt(const QTextCursor &cursor, const Utils::LinkHandler &callback,
                       bool resolveTarget, bool inNextSplit);
    void requestUsages(const QTextCursor &cursor);
    void requestRename(const QTextCursor &cursor);
    void optionalActionMaskChanged();
    void toolbarOutlineChanged(QWidget *newOutline);

protected:
    QTextBlock blockForVisibleRow(int row) const;
    QTextBlock blockForVerticalOffset(int offset) const;
    bool event(QEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void changeEvent(QEvent *e) override;
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
    void showEvent(QShowEvent *) override;
    bool viewportEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *) override;
    void paintEvent(QPaintEvent *) override;
    virtual void paintBlock(QPainter *painter,
                            const QTextBlock &block,
                            const QPointF &offset,
                            const QVector<QTextLayout::FormatRange> &selections,
                            const QRect &clipRect) const;
    void timerEvent(QTimerEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    //void mouseDoubleClickEvent(QMouseEvent *) override;
    //void leaveEvent(QEvent *) override;
    //void keyReleaseEvent(QKeyEvent *) override;
    //void dragEnterEvent(QDragEnterEvent *e) override;

    QMimeData *createMimeDataFromSelection() const override;
    QMimeData *createMimeDataFromSelection(bool withHtml) const;
    bool canInsertFromMimeData(const QMimeData *source) const override;
    void insertFromMimeData(const QMimeData *source) override;
    void dragLeaveEvent(QDragLeaveEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dropEvent(QDropEvent *e) override;

    virtual QString plainTextFromSelection(const QTextCursor &cursor) const;
    virtual QString plainTextFromSelection(const Utils::MultiTextCursor &cursor) const;

    virtual QString lineNumber(int blockNumber) const;
    virtual int lineNumberDigits() const;
    virtual bool selectionVisible(int blockNumber) const;
    virtual bool replacementVisible(int blockNumber) const;
    virtual QColor replacementPenColor(int blockNumber) const;

    virtual void triggerPendingUpdates();
    virtual void applyFontSettings();

    virtual void finalizeInitialization() {}
    virtual void finalizeInitializationAfterDuplication(TextEditorWidget *) {}
    static QTextCursor flippedCursor(const QTextCursor &cursor);

    void setVisualIndentOffset(int offset);

public:
    QString selectedText() const;
    Core::BaseTextFind *finder();
    void findText(const QString& text,int flags,bool hightlight=true);
    void replaceText(const QString& before,const QString& after,int flags,bool hightlight=true);
    int replaceAll(const QString& before,const QString& after,int flags);
    void clearHighlights();

    void setupGenericHighlighter();
    void setupFallBackEditor(Utils::Id id);

    void remove(int length);
    void replace(int length, const QString &string);
    QChar characterAt(int pos) const;
    QString textAt(int from, int to) const;

    //void contextHelpItem(const Core::IContext::HelpCallback &callback);
    //void setContextHelpItem(const Core::HelpItem &item);

    Q_INVOKABLE bool inFindScope(const QTextCursor &cursor) const;



protected:
    /*!
       Reimplement this function to enable code navigation.

       \a resolveTarget is set to true when the target of the link is relevant
       (it isn't until the link is used).
     */
    virtual void findLinkAt(const QTextCursor &,
                            const Utils::LinkHandler &processLinkCallback,
                            bool resolveTarget = true,
                            bool inNextSplit = false);

    /*!
       Returns whether the link was opened successfully.
     */
    bool openLink(const Utils::Link &link, bool inNextSplit = false);

    /*!
      Reimplement this function to change the default replacement text.
      */
    virtual QString foldReplacementText(const QTextBlock &block) const;
    virtual void drawCollapsedBlockPopup(QPainter &painter,
                                         const QTextBlock &block,
                                         QPointF offset,
                                         const QRect &clip);
    int visibleFoldedBlockNumber() const;
    void doSetTextCursor(const QTextCursor &cursor) override;
    void doSetTextCursor(const QTextCursor &cursor, bool keepMultiSelection);

signals:
    void markRequested(TextEditor::TextEditorWidget *widget,
        int line, TextEditor::TextMarkRequestKind kind);
    void markContextMenuRequested(TextEditor::TextEditorWidget *widget,
        int line, QMenu *menu);
    void tooltipOverrideRequested(TextEditor::TextEditorWidget *widget,
        const QPoint &globalPos, int position, bool *handled);
    void tooltipRequested(const QPoint &globalPos, int position);
    //void activateEditor(Core::EditorManager::OpenEditorFlags flags = {});

protected:
    virtual void slotCursorPositionChanged(); // Used in VcsBase
    virtual void slotCodeStyleSettingsChanged(const QVariant &); // Used in CppEditor

private:
    Internal::TextEditorWidgetPrivate *d;
    bool isDestory=false;
    friend class Internal::TextEditorWidgetPrivate;
    friend class Internal::TextEditorOverlay;
    friend class RefactorOverlay;

    void updateVisualWrapColumn();
};




} // namespace TextEditor

QT_BEGIN_NAMESPACE

size_t qHash(const QColor &color);

QT_END_NAMESPACE
