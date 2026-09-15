// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
//
// v2 split of texteditor.cpp (legacy file kept at ../texteditor.cpp).
// Settings: display/margin/behavior/typing/storage/completion/extra-encoding settings, fonts, tab stops, language/code style, syntax highlighter configuration.
//

#include "texteditorwidget_p.h"

namespace TextEditor {

using namespace Internal;

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


void TextEditorWidgetPrivate::applyFontSettingsDelayed()
{
    m_fontSettingsNeedsApply = true;
    if (q->isVisible())
        q->triggerPendingUpdates();
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
    QTextOption option = q->document()->defaultTextOption();
    option.setTabStopDistance(charWidth() * m_document->tabSettings().m_tabSize);
    q->document()->setDefaultTextOption(option);
    if (TextSuggestion *suggestion = TextDocumentLayout::suggestion(m_suggestionBlock)) {
        QTextOption option = suggestion->replacementDocument()->defaultTextOption();
        option.setTabStopDistance(option.tabStopDistance());
        suggestion->replacementDocument()->setDefaultTextOption(option);
    }
}


void TextEditorWidgetPrivate::applyTabSettings()
{
    updateTabStops();
    m_autoCompleter->setTabSettings(m_document->tabSettings());
}


void TextEditorWidget::configureGenericHighlighter()
{

    Highlighter::Definitions definitions = Highlighter::definitionsForDocument(textDocument());
    d->configureGenericHighlighter(definitions.isEmpty() ? Highlighter::Definition() : definitions.first());
    d->updateSyntaxInfoBar(definitions, textDocument()->filePath().fileName());
}



void TextEditorWidget::setupGenericHighlighter()
{
    setLineSeparatorsAllowed(true);

    connect(textDocument(), &IDocument::filePathChanged,
            d, &TextEditorWidgetPrivate::reconfigure);
}





} // namespace TextEditor

