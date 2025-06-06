// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"

#include "textmark.h"
#include "textdocument.h"
#include "textsuggestion.h"
#include "utils/id.h"

#include "syntax-highlighting/state.h"

#include <QTextBlockUserData>
#include <QPlainTextDocumentLayout>
#include <QStack>

namespace TextEditor {

struct TEXTEDITOR_EXPORT Parenthesis
{
    enum Type : char { Opened, Closed };

    Parenthesis() = default;
    Parenthesis(Type t, QChar c, int position) : pos(position), chr(c), type(t) {}

    friend TEXTEDITOR_EXPORT QDebug operator<<(QDebug debug, const Parenthesis &parenthesis);

    int pos = -1;
    QChar chr;
    Utils::Id source;
    Type type = Opened;

    bool operator==(const Parenthesis &other) const;
};
using Parentheses = QVector<Parenthesis>;
TEXTEDITOR_EXPORT void insertSorted(Parentheses &list, const Parenthesis &elem);

class TEXTEDITOR_EXPORT CodeFormatterData
{
public:
    virtual ~CodeFormatterData();
};

class TEXTEDITOR_EXPORT TextBlockUserData : public QTextBlockUserData
{
public:

    inline TextBlockUserData()
        : m_foldingIndent(0)
        , m_lexerState(0)
        , m_folded(false)
        , m_ifdefedOut(false)
        , m_foldingStartIncluded(false)
        , m_foldingEndIncluded(false)
        , m_codeFormatterData(nullptr)
    {}
    ~TextBlockUserData() override;

    inline TextMarks marks() const { return m_marks; }
    void addMark(TextMark *mark);
    inline bool removeMark(TextMark *mark) { return m_marks.removeAll(mark); }

    inline TextMarks documentClosing() {
        const TextMarks marks = m_marks;
        for (TextMark *mrk : marks)
            mrk->setBaseTextDocument(nullptr);
        m_marks.clear();
        return marks;
    }

    inline void setFolded(bool b) { m_folded = b; }
    inline bool folded() const { return m_folded; }

    inline void setParentheses(const Parentheses &parentheses) { m_parentheses = parentheses; }
    inline void clearParentheses() { m_parentheses.clear(); }
    inline const Parentheses &parentheses() const { return m_parentheses; }
    inline bool hasParentheses() const { return !m_parentheses.isEmpty(); }
    int braceDepthDelta() const;

    inline bool setIfdefedOut() { bool result = m_ifdefedOut; m_ifdefedOut = true; return !result; }
    inline bool clearIfdefedOut() { bool result = m_ifdefedOut; m_ifdefedOut = false; return result;}
    inline bool ifdefedOut() const { return m_ifdefedOut; }


    enum MatchType { NoMatch, Match, Mismatch  };
    static MatchType checkOpenParenthesis(QTextCursor *cursor, QChar c);
    static MatchType checkClosedParenthesis(QTextCursor *cursor, QChar c);
    static MatchType matchCursorBackward(QTextCursor *cursor);
    static MatchType matchCursorForward(QTextCursor *cursor);
    static bool findPreviousOpenParenthesis(QTextCursor *cursor, bool select = false,
                                            bool onlyInCurrentBlock = false);
    static bool findNextClosingParenthesis(QTextCursor *cursor, bool select = false);

    static bool findPreviousBlockOpenParenthesis(QTextCursor *cursor, bool checkStartPosition = false);
    static bool findNextBlockClosingParenthesis(QTextCursor *cursor);

    // Get the code folding level
    inline int foldingIndent() const { return m_foldingIndent; }
    /* Set the code folding level.
     *
     * A code folding marker will appear the line *before* the one where the indention
     * level increases. The code folding reagion will end in the last line that has the same
     * indention level (or higher).
     */
    inline void setFoldingIndent(int indent) { m_foldingIndent = indent; }
    // Set whether the first character of the folded region will show when the code is folded.
    inline void setFoldingStartIncluded(bool included) { m_foldingStartIncluded = included; }
    inline bool foldingStartIncluded() const { return m_foldingStartIncluded; }
    // Set whether the last character of the folded region will show when the code is folded.
    inline void setFoldingEndIncluded(bool included) { m_foldingEndIncluded = included; }
    inline bool foldingEndIncluded() const { return m_foldingEndIncluded; }
    inline int lexerState() const { return m_lexerState; }
    inline void setLexerState(int state) { m_lexerState = state; }

    //inline void enterState(int state){m_states.push(state);}
    //inline int leaveState(){return m_states.pop();}

    inline void setStateStack(const QStack<int>& states){m_states = states;}
    inline QStack<int>& stateStack(){return m_states;}

    inline void setAdditionalAnnotationHeight(int annotationHeight)
    { m_additionalAnnotationHeight = annotationHeight; }
    inline int additionalAnnotationHeight() const { return m_additionalAnnotationHeight; }

    CodeFormatterData *codeFormatterData() const { return m_codeFormatterData; }
    void setCodeFormatterData(CodeFormatterData *data);

    KSyntaxHighlighting::State syntaxState() { return m_syntaxState; }
    void setSyntaxState(KSyntaxHighlighting::State state) { m_syntaxState = state; }

    QByteArray expectedRawStringSuffix() { return m_expectedRawStringSuffix; }
    void setExpectedRawStringSuffix(const QByteArray &suffix) { m_expectedRawStringSuffix = suffix; }

    void insertSuggestion(std::unique_ptr<TextSuggestion> &&suggestion);
    TextSuggestion *suggestion() const;
    void clearSuggestion();

private:
    TextMarks m_marks;
    int m_foldingIndent : 16;
    int m_lexerState : 32;
    uint m_folded : 1;
    uint m_ifdefedOut : 1;
    uint m_foldingStartIncluded : 1;
    uint m_foldingEndIncluded : 1;
    int m_additionalAnnotationHeight = 0;
    Parentheses m_parentheses;
    CodeFormatterData *m_codeFormatterData;
    std::unique_ptr<TextSuggestion> m_suggestion;
    KSyntaxHighlighting::State m_syntaxState;
    QByteArray m_expectedRawStringSuffix; // A bit C++-specific, but let's be pragmatic.
    QStack<int> m_states;
};


class TEXTEDITOR_EXPORT TextDocumentLayout : public QPlainTextDocumentLayout
{
    Q_OBJECT

public:
    enum ExpressBracesMask{
        TemplateExpressionOpenBracesMask0 = 1 << 21,
        TemplateExpressionOpenBracesMask1 = 1 << 22,
        TemplateExpressionOpenBracesMask2 = 1 << 23,
        TemplateExpressionOpenBracesMask3 = 1 << 24,
        TemplateExpressionOpenBracesMask4 = 1 << 25,
    };

    TextDocumentLayout(QTextDocument *doc);
    ~TextDocumentLayout() override;

    static void setParentheses(const QTextBlock &block, const Parentheses &parentheses);
    static void clearParentheses(const QTextBlock &block) { setParentheses(block, Parentheses());}
    static Parentheses parentheses(const QTextBlock &block);
    static bool hasParentheses(const QTextBlock &block);
    static bool setIfdefedOut(const QTextBlock &block);
    static bool clearIfdefedOut(const QTextBlock &block);
    static bool ifdefedOut(const QTextBlock &block);
    static int braceDepthDelta(const QTextBlock &block);
    static int braceDepth(const QTextBlock &block);
    static void setBraceDepth(QTextBlock &block, int depth);
    static void changeBraceDepth(QTextBlock &block, int delta);
    static void setFoldingIndent(const QTextBlock &block, int indent);
    static int foldingIndent(const QTextBlock &block);
    static void setLexerState(const QTextBlock &block, int state);
    static int lexerState(const QTextBlock &block);
    static void setStateStack(const QTextBlock &block,const QStack<int> stateStack);
    static QStack<int>* stateStack(const QTextBlock &block);

    static void changeFoldingIndent(QTextBlock &block, int delta);
    static bool canFold(const QTextBlock &block);
    static void doFoldOrUnfold(const QTextBlock& block, bool unfold);
    static bool isFolded(const QTextBlock &block);
    static void setFolded(const QTextBlock &block, bool folded);
    static void setExpectedRawStringSuffix(const QTextBlock &block, const QByteArray &suffix);
    static QByteArray expectedRawStringSuffix(const QTextBlock &block);
    static TextSuggestion *suggestion(const QTextBlock &block);
    static void updateSuggestionFormats(const QTextBlock &block, const FontSettings &fontSettings);

    class TEXTEDITOR_EXPORT FoldValidator
    {
    public:
        void setup(TextDocumentLayout *layout);
        void reset();
        void process(QTextBlock block);
        void finalize();

    private:
        TextDocumentLayout *m_layout = nullptr;
        bool m_requestDocUpdate = false;
        int m_insideFold = 0;
    };

    static TextBlockUserData *textUserData(const QTextBlock &block) {
        return static_cast<TextBlockUserData*>(block.userData());
    }
    static TextBlockUserData *userData(const QTextBlock &block) {
        auto data = static_cast<TextBlockUserData*>(block.userData());
        if (!data && block.isValid())
            const_cast<QTextBlock &>(block).setUserData((data = new TextBlockUserData));
        return data;
    }


    void requestExtraAreaUpdate();

    void emitDocumentSizeChanged() { emit documentSizeChanged(documentSize()); }

    int lastSaveRevision = 0;
    bool hasMarks = false;
    bool hasLocationMarker = false;
    int m_requiredWidth = 0;

    void setRequiredWidth(int width);

    QSizeF documentSize() const override;
    QRectF blockBoundingRect(const QTextBlock &block) const override;

    TextMarks documentClosing();
    void documentReloaded(TextMarks marks, TextDocument *baseextDocument);
    void updateMarksLineNumber();
    void updateMarksBlock(const QTextBlock &block);
    void scheduleUpdate();
    void requestUpdateNow();

private:
    bool m_updateScheduled = false;

signals:
    void updateExtraArea();
    void foldChanged(const int blockNumber, bool folded);
    void parenthesesChanged(const QTextBlock block);
};

} // namespace TextEditor
