// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "syntaxhighlighter.h"
#include "textdocument.h"
#include "textdocumentlayout.h"
#include "texteditorsettings.h"
#include "fontsettings.h"

#include <utils/algorithm.h>
#include <utils/qtcassert.h>

#include <QTextDocument>
#include <QPointer>

#include <cmath>

namespace TextEditor {

class SyntaxHighlighterPrivate
{
    SyntaxHighlighter *q_ptr = nullptr;
    Q_DECLARE_PUBLIC(SyntaxHighlighter)
public:
    SyntaxHighlighterPrivate()
    {
        updateFormats(TextEditorSettings::fontSettings());
    }

    QPointer<QTextDocument> doc;

    void reformatBlocks(int from, int charsRemoved, int charsAdded);
    void reformatBlock(const QTextBlock &block, int from, int charsRemoved, int charsAdded);

    inline void rehighlight(QTextCursor &cursor, QTextCursor::MoveOperation operation) {
        inReformatBlocks = true;
        int from = cursor.position();
        cursor.movePosition(operation);
        reformatBlocks(from, 0, cursor.position() - from);
        inReformatBlocks = false;
    }

    void applyFormatChanges(int from, int charsRemoved, int charsAdded);
    void updateFormats(const FontSettings &fontSettings);

    FontSettings fontSettings;
    QVector<QTextCharFormat> formatChanges;
    QTextBlock currentBlock;
    bool rehighlightPending = false;
    bool inReformatBlocks = false;
    TextDocumentLayout::FoldValidator foldValidator;
    QVector<QTextCharFormat> formats;
    QVector<std::pair<int,TextStyle>> formatCategories;
    QTextCharFormat whitespaceFormat;
    bool noAutomaticHighlighting = false;
};

static bool adjustRange(QTextLayout::FormatRange &range, int from, int charsDelta)
{
    if (range.start >= from) {
        range.start += charsDelta;
        return true;
    } else if (range.start + range.length > from) {
        range.length += charsDelta;
        return true;
    }
    return false;
}

void SyntaxHighlighter::delayedRehighlight()
{
    Q_D(SyntaxHighlighter);
    if (!d->rehighlightPending)
        return;
    d->rehighlightPending = false;
    rehighlight();
}

void SyntaxHighlighterPrivate::applyFormatChanges(int from, int charsRemoved, int charsAdded)
{
    bool formatsChanged = false;

    QTextLayout *layout = currentBlock.layout();

    QVector<QTextLayout::FormatRange> ranges;
    QVector<QTextLayout::FormatRange> oldRanges;
    std::tie(ranges, oldRanges)
        = Utils::partition(layout->formats(), [](const QTextLayout::FormatRange &range) {
              return range.format.property(QTextFormat::UserProperty).toBool();
          });

    if (currentBlock.contains(from)) {
        const int charsDelta = charsAdded - charsRemoved;
        for (QTextLayout::FormatRange &range : ranges)
            formatsChanged |= adjustRange(range, from - currentBlock.position(), charsDelta);
    }

    QTextCharFormat emptyFormat;

    QTextLayout::FormatRange r;

    QVector<QTextLayout::FormatRange> newRanges;
    int i = 0;
    while (i < formatChanges.count()) {

        while (i < formatChanges.count() && formatChanges.at(i) == emptyFormat)
            ++i;

        if (i >= formatChanges.count())
            break;

        r.start = i;
        r.format = formatChanges.at(i);

        while (i < formatChanges.count() && formatChanges.at(i) == r.format)
            ++i;

        r.length = i - r.start;

        newRanges << r;
    }

    formatsChanged = formatsChanged || (newRanges.size() != oldRanges.size());

    for (int i = 0; !formatsChanged && i < newRanges.size(); ++i) {
        const QTextLayout::FormatRange &o = oldRanges.at(i);
        const QTextLayout::FormatRange &n = newRanges.at(i);
        formatsChanged = (o.start != n.start || o.length != n.length || o.format != n.format);
    }

    if (formatsChanged) {
        ranges.append(newRanges);
        layout->setFormats(ranges);
        doc->markContentsDirty(currentBlock.position(), currentBlock.length());
    }
}

void SyntaxHighlighter::reformatBlocks(int from, int charsRemoved, int charsAdded)
{
    Q_D(SyntaxHighlighter);
    if (!d->inReformatBlocks)
        d->reformatBlocks(from, charsRemoved, charsAdded);
}

void SyntaxHighlighterPrivate::reformatBlocks(int from, int charsRemoved, int charsAdded)
{
    foldValidator.reset();

    rehighlightPending = false;

    QTextBlock block = doc->findBlock(from);
    if (!block.isValid())
        return;

    int endPosition;
    QTextBlock lastBlock = doc->findBlock(from + charsAdded + (charsRemoved > 0 ? 1 : 0));
    if (lastBlock.isValid())
        endPosition = lastBlock.position() + lastBlock.length();
    else
        endPosition =  doc->lastBlock().position() + doc->lastBlock().length(); //doc->docHandle()->length();

    bool forceHighlightOfNextBlock = false;

    while (block.isValid() && (block.position() < endPosition || forceHighlightOfNextBlock)) {
        const int stateBeforeHighlight = block.userState();

        reformatBlock(block, from, charsRemoved, charsAdded);

        forceHighlightOfNextBlock = (block.userState() != stateBeforeHighlight);

        block = block.next();
    }

    formatChanges.clear();

    foldValidator.finalize();
}

void SyntaxHighlighterPrivate::reformatBlock(const QTextBlock &block, int from, int charsRemoved, int charsAdded)
{
    Q_Q(SyntaxHighlighter);

    Q_ASSERT_X(!currentBlock.isValid(), "SyntaxHighlighter::reformatBlock()", "reFormatBlock() called recursively");

    currentBlock = block;

    formatChanges.fill(QTextCharFormat(), block.length() - 1);
    q->highlightBlock(block.text());
    applyFormatChanges(from, charsRemoved, charsAdded);

    foldValidator.process(currentBlock);

    currentBlock = QTextBlock();
}

/*!
    \class SyntaxHighlighter

    \brief The SyntaxHighlighter class allows you to define syntax highlighting rules and to query
    a document's current formatting or user data.

    The SyntaxHighlighter class is a copied and forked version of the QSyntaxHighlighter. There are
    a couple of binary incompatible changes that prevent doing this directly in Qt.

    The main difference from the QSyntaxHighlighter is the addition of setExtraFormats.
    This method prevents redoing syntax highlighting when setting the formats on the
    layout and subsequently marking the document contents dirty. It thus prevents the redoing of the
    semantic highlighting, which sets extra formats, and so on.

    Another way to implement the semantic highlighting is to use ExtraSelections on
    Q(Plain)TextEdit. The drawback of QTextEdit::setExtraSelections is that ExtraSelection uses a
    QTextCursor for positioning. That means that with every document change (that is, every
    keystroke), a whole bunch of cursors can be re-checked or re-calculated. This is not needed in
    our situation, because the next thing that will happen is that the highlighting will come up
    with new ranges, meaning that it destroys the cursors. To make things worse, QTextCursor
    calculates the pixel position in the line it's in. The calculations are done with
    QTextLine::cursorTo, which is very expensive and is not optimized for fixed-width fonts. Another
    reason not to use ExtraSelections is that those selections belong to the editor, not to the
    document. This means that every editor needs a highlighter, instead of every document. This
    could become expensive when multiple editors with the same document are opened.

    So, we use formats, because all those highlights should get removed or redone soon
    after the change happens.
*/

/*!
    Constructs a SyntaxHighlighter with the given \a parent.
*/
SyntaxHighlighter::SyntaxHighlighter(QObject *parent)
    : QObject(parent), d_ptr(new SyntaxHighlighterPrivate)
{
    d_ptr->q_ptr = this;
}

/*!
    Constructs a SyntaxHighlighter and installs it on \a parent.
    The specified QTextDocument also becomes the owner of the
    SyntaxHighlighter.
*/
SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QObject(parent), d_ptr(new SyntaxHighlighterPrivate)
{
    d_ptr->q_ptr = this;
    if (parent)
        setDocument(parent);
}

/*!
    Constructs a SyntaxHighlighter and installs it on \a parent's
    QTextDocument. The specified QTextEdit also becomes the owner of
    the SyntaxHighlighter.
*/
SyntaxHighlighter::SyntaxHighlighter(QTextEdit *parent)
    : QObject(parent), d_ptr(new SyntaxHighlighterPrivate)
{
    d_ptr->q_ptr = this;
    if (parent)
        setDocument(parent->document());
}

/*!
    Destructor that uninstalls this syntax highlighter from the text document.
*/
SyntaxHighlighter::~SyntaxHighlighter()
{
    setDocument(nullptr);
}

/*!
    Installs the syntax highlighter on the given QTextDocument \a doc.
    A SyntaxHighlighter can only be used with one document at a time.
*/
void SyntaxHighlighter::setDocument(QTextDocument *doc)
{
    Q_D(SyntaxHighlighter);
    if (d->doc) {
        disconnect(d->doc, &QTextDocument::contentsChange, this, &SyntaxHighlighter::reformatBlocks);

        QTextCursor cursor(d->doc);
        cursor.beginEditBlock();
        for (QTextBlock blk = d->doc->begin(); blk.isValid(); blk = blk.next())
            blk.layout()->clearFormats();
        cursor.endEditBlock();
    }
    d->doc = doc;
    if (d->doc) {
        if (!d->noAutomaticHighlighting) {
            connect(d->doc, &QTextDocument::contentsChange, this, &SyntaxHighlighter::reformatBlocks);
            d->rehighlightPending = true;
            QMetaObject::invokeMethod(this, &SyntaxHighlighter::delayedRehighlight,
                                      Qt::QueuedConnection);
        }
        d->foldValidator.setup(qobject_cast<TextDocumentLayout *>(doc->documentLayout()));
    }
}

/*!
    Returns the QTextDocument on which this syntax highlighter is
    installed.
*/
QTextDocument *SyntaxHighlighter::document() const
{
    Q_D(const SyntaxHighlighter);
    return d->doc;
}

/*!
    \since 4.2

    Reapplies the highlighting to the whole document.

    \sa rehighlightBlock()
*/
void SyntaxHighlighter::rehighlight()
{
    Q_D(SyntaxHighlighter);
    if (!d->doc)
        return;

    QTextCursor cursor(d->doc);
    d->rehighlight(cursor, QTextCursor::End);
}

/*!
    \since 4.6

    Reapplies the highlighting to the given QTextBlock \a block.

    \sa rehighlight()
*/
void SyntaxHighlighter::rehighlightBlock(const QTextBlock &block)
{
    Q_D(SyntaxHighlighter);
    if (!d->doc || !block.isValid() || block.document() != d->doc)
        return;

    const bool rehighlightPending = d->rehighlightPending;

    QTextCursor cursor(block);
    d->rehighlight(cursor, QTextCursor::EndOfBlock);

    if (rehighlightPending)
        d->rehighlightPending = rehighlightPending;
}

/*!
    \fn void SyntaxHighlighter::highlightBlock(const QString &text)

    Highlights the given text block. This function is called when
    necessary by the rich text engine, i.e. on text blocks which have
    changed.

    To provide your own syntax highlighting, you must subclass
    SyntaxHighlighter and reimplement highlightBlock(). In your
    reimplementation you should parse the block's \a text and call
    setFormat() as often as necessary to apply any font and color
    changes that you require. For example:

    \snippet doc/src/snippets/code/src_gui_text_SyntaxHighlighter.cpp 3

    Some syntaxes can have constructs that span several text
    blocks. For example, a C++ syntax highlighter should be able to
    cope with \c{/}\c{*...*}\c{/} multiline comments. To deal with
    these cases it is necessary to know the end state of the previous
    text block (e.g. "in comment").

    Inside your highlightBlock() implementation you can query the end
    state of the previous text block using the previousBlockState()
    function. After parsing the block you can save the last state
    using setCurrentBlockState().

    The currentBlockState() and previousBlockState() functions return
    an int value. If no state is set, the returned value is -1. You
    can designate any other value to identify any given state using
    the setCurrentBlockState() function. Once the state is set the
    QTextBlock keeps that value until it is set set again or until the
    corresponding paragraph of text gets deleted.

    For example, if you're writing a simple C++ syntax highlighter,
    you might designate 1 to signify "in comment". For a text block
    that ended in the middle of a comment you'd set 1 using
    setCurrentBlockState, and for other paragraphs you'd set 0.
    In your parsing code if the return value of previousBlockState()
    is 1, you would highlight the text as a C++ comment until you
    reached the closing \c{*}\c{/}.

    \sa previousBlockState(), setFormat(), setCurrentBlockState()
*/

/*!
    This function is applied to the syntax highlighter's current text
    block (i.e. the text that is passed to the highlightBlock()
    function).

    The specified \a format is applied to the text from the \a start
    position for a length of \a count characters (if \a count is 0,
    nothing is done). The formatting properties set in \a format are
    merged at display time with the formatting information stored
    directly in the document, for example as previously set with
    QTextCursor's functions. Note that the document itself remains
    unmodified by the format set through this function.

    \sa format(), highlightBlock()
*/
void SyntaxHighlighter::setFormat(int start, int count, const QTextCharFormat &format)
{
    Q_D(SyntaxHighlighter);
    if (start < 0 || start >= d->formatChanges.count())
        return;

    const int end = qMin(start + count, d->formatChanges.count());
    for (int i = start; i < end; ++i)
        d->formatChanges[i] = format;
}

/*!
    \overload

    The specified \a color is applied to the current text block from
    the \a start position for a length of \a count characters.

    The other attributes of the current text block, e.g. the font and
    background color, are reset to default values.

    \sa format(), highlightBlock()
*/
void SyntaxHighlighter::setFormat(int start, int count, const QColor &color)
{
    QTextCharFormat format;
    format.setForeground(color);
    setFormat(start, count, format);
}

/*!
    \overload

    The specified \a font is applied to the current text block from
    the \a start position for a length of \a count characters.

    The other attributes of the current text block, e.g. the font and
    background color, are reset to default values.

    \sa format(), highlightBlock()
*/
void SyntaxHighlighter::setFormat(int start, int count, const QFont &font)
{
    QTextCharFormat format;
    format.setFont(font);
    setFormat(start, count, format);
}

void SyntaxHighlighter::formatSpaces(const QString &text, int start, int count)
{
    Q_D(const SyntaxHighlighter);
    int offset = start;
    const int end = std::min(start + count, int(text.length()));
    while (offset < end) {
        if (text.at(offset).isSpace()) {
            int start = offset++;
            while (offset < end && text.at(offset).isSpace())
                ++offset;
            setFormat(start, offset - start, d->whitespaceFormat);
        } else {
            ++offset;
        }
    }
}

/*!
    The specified \a format is applied to all non-whitespace characters in the current text block
    with \a text, from the \a start position for a length of \a count characters.
    Whitespace characters are formatted with the visual whitespace format, merged with the
    non-whitespace format.

    \sa setFormat()
*/
void SyntaxHighlighter::setFormatWithSpaces(const QString &text, int start, int count,
                                            const QTextCharFormat &format)
{
    Q_D(const SyntaxHighlighter);
    QTextCharFormat visualSpaceFormat = d->whitespaceFormat;
    visualSpaceFormat.setBackground(format.background());

    const int end = std::min(start + count, int(text.length()));
    int index = start;

    while (index != end) {
        const bool isSpace = text.at(index).isSpace();
        const int start = index;

        do { ++index; }
        while (index != end && text.at(index).isSpace() == isSpace);

        const int tokenLength = index - start;
        if (isSpace)
            setFormat(start, tokenLength, visualSpaceFormat);
        else if (format.isValid())
            setFormat(start, tokenLength, format);
    }
}

/*!
    Returns the format at \a position inside the syntax highlighter's
    current text block.
*/
QTextCharFormat SyntaxHighlighter::format(int pos) const
{
    Q_D(const SyntaxHighlighter);
    if (pos < 0 || pos >= d->formatChanges.count())
        return QTextCharFormat();
    return d->formatChanges.at(pos);
}

/*!
    Returns the end state of the text block previous to the
    syntax highlighter's current block. If no value was
    previously set, the returned value is -1.

    \sa highlightBlock(), setCurrentBlockState()
*/
int SyntaxHighlighter::previousBlockState() const
{
    Q_D(const SyntaxHighlighter);
    if (!d->currentBlock.isValid())
        return -1;

    const QTextBlock previous = d->currentBlock.previous();
    if (!previous.isValid())
        return -1;

    return previous.userState();
}

/*!
    Returns the state of the current text block. If no value is set,
    the returned value is -1.
*/
int SyntaxHighlighter::currentBlockState() const
{
    Q_D(const SyntaxHighlighter);
    if (!d->currentBlock.isValid())
        return -1;

    return d->currentBlock.userState();
}

/*!
    Sets the state of the current text block to \a newState.

    \sa highlightBlock()
*/
void SyntaxHighlighter::setCurrentBlockState(int newState)
{
    Q_D(SyntaxHighlighter);
    if (!d->currentBlock.isValid())
        return;

    d->currentBlock.setUserState(newState);
}



/*!
    Attaches the given \a data to the current text block.  The
    ownership is passed to the underlying text document, i.e. the
    provided QTextBlockUserData object will be deleted if the
    corresponding text block gets deleted.

    QTextBlockUserData can be used to store custom settings. In the
    case of syntax highlighting, it is in particular interesting as
    cache storage for information that you may figure out while
    parsing the paragraph's text.

    For example while parsing the text, you can keep track of
    parenthesis characters that you encounter ('{[(' and the like),
    and store their relative position and the actual QChar in a simple
    class derived from QTextBlockUserData:

    \snippet doc/src/snippets/code/src_gui_text_SyntaxHighlighter.cpp 4

    During cursor navigation in the associated editor, you can ask the
    current QTextBlock (retrieved using the QTextCursor::block()
    function) if it has a user data object set and cast it to your \c
    BlockData object. Then you can check if the current cursor
    position matches with a previously recorded parenthesis position,
    and, depending on the type of parenthesis (opening or closing),
    find the next opening or closing parenthesis on the same level.

    In this way you can do a visual parenthesis matching and highlight
    from the current cursor position to the matching parenthesis. That
    makes it easier to spot a missing parenthesis in your code and to
    find where a corresponding opening/closing parenthesis is when
    editing parenthesis intensive code.

    \sa QTextBlock::setUserData()
*/
void SyntaxHighlighter::setCurrentBlockUserData(QTextBlockUserData *data)
{
    Q_D(SyntaxHighlighter);
    if (!d->currentBlock.isValid())
        return;

    d->currentBlock.setUserData(data);
}

/*!
    Returns the QTextBlockUserData object previously attached to the
    current text block.

    \sa QTextBlock::userData(), setCurrentBlockUserData()
*/
QTextBlockUserData *SyntaxHighlighter::currentBlockUserData() const
{
    Q_D(const SyntaxHighlighter);
    if (!d->currentBlock.isValid())
        return nullptr;

    return d->currentBlock.userData();
}

/*!
    \since 4.4

    Returns the current text block.
*/
QTextBlock SyntaxHighlighter::currentBlock() const
{
    Q_D(const SyntaxHighlighter);
    return d->currentBlock;
}

static bool byStartOfRange(const QTextLayout::FormatRange &range, const QTextLayout::FormatRange &other)
{
    return range.start < other.start;
}

// The formats is passed in by rvalue reference in order to prevent unnecessary copying of its items.
// After this function returns, the list is modified, and should be considered invalidated!
void SyntaxHighlighter::setExtraFormats(const QTextBlock &block,
                                        QVector<QTextLayout::FormatRange> &&formats)
{
    Q_D(SyntaxHighlighter);

    const int blockLength = block.length();
    if (block.layout() == nullptr || blockLength == 0)
        return;

    Utils::sort(formats, byStartOfRange);

    const QVector<QTextLayout::FormatRange> all = block.layout()->formats();
    QVector<QTextLayout::FormatRange> previousSemanticFormats;
    QVector<QTextLayout::FormatRange> formatsToApply;
    std::tie(previousSemanticFormats, formatsToApply)
        = Utils::partition(all, [](const QTextLayout::FormatRange &r) {
              return r.format.hasProperty(QTextFormat::UserProperty);
          });

    for (auto &format : formats)
        format.format.setProperty(QTextFormat::UserProperty, true);

    if (formats.size() == previousSemanticFormats.size()) {
        Utils::sort(previousSemanticFormats, byStartOfRange);
        if (formats == previousSemanticFormats)
            return;
    }

    formatsToApply += formats;

    bool wasInReformatBlocks = d->inReformatBlocks;
    d->inReformatBlocks = true;
    block.layout()->setFormats(formatsToApply);
    document()->markContentsDirty(block.position(), blockLength - 1);
    d->inReformatBlocks = wasInReformatBlocks;
}

void SyntaxHighlighter::clearExtraFormats(const QTextBlock &block)
{
    Q_D(SyntaxHighlighter);

    const int blockLength = block.length();
    if (block.layout() == nullptr || blockLength == 0)
        return;

    const QVector<QTextLayout::FormatRange> formatsToApply
        = Utils::filtered(block.layout()->formats(), [](const QTextLayout::FormatRange &r) {
              return !r.format.hasProperty(QTextFormat::UserProperty);
          });

    bool wasInReformatBlocks = d->inReformatBlocks;
    d->inReformatBlocks = true;
    block.layout()->setFormats(formatsToApply);
    document()->markContentsDirty(block.position(), blockLength - 1);
    d->inReformatBlocks = wasInReformatBlocks;
}

void SyntaxHighlighter::clearAllExtraFormats()
{
    QTextBlock b = document()->firstBlock();
    while (b.isValid()) {
        clearExtraFormats(b);
        b = b.next();
    }
}

/* Generate at least n different colors for highlighting, excluding background
 * color. */

QList<QColor> SyntaxHighlighter::generateColors(int n, const QColor &background)
{
    QList<QColor> result;
    // Assign a color gradient. Generate a sufficient number of colors
    // by using ceil and looping from 0..step.
    const double oneThird = 1.0 / 3.0;
    const int step = qRound(std::ceil(std::pow(double(n), oneThird)));
    result.reserve(step * step * step);
    const int factor = 255 / step;
    const int half = factor / 2;
    const int bgRed = background.red();
    const int bgGreen = background.green();
    const int bgBlue = background.blue();
    for (int r = step; r >= 0; --r) {
        const int red = r * factor;
        if (bgRed - half > red || bgRed + half <= red) {
            for (int g = step; g >= 0; --g) {
                const int green = g * factor;
                if (bgGreen - half > green || bgGreen + half <= green) {
                    for (int b = step; b >= 0 ; --b) {
                        const int blue = b * factor;
                        if (bgBlue - half > blue || bgBlue + half <= blue)
                            result.append(QColor(red, green, blue));
                    }
                }
            }
        }
    }
    return result;
}

void SyntaxHighlighter::setFontSettings(const FontSettings &fontSettings)
{
    Q_D(SyntaxHighlighter);
    d->updateFormats(fontSettings);
}

FontSettings SyntaxHighlighter::fontSettings() const
{
    Q_D(const SyntaxHighlighter);
    return d->fontSettings;
}
/*!
    The syntax highlighter is not anymore reacting to the text document if \a noAutomatic is
    \c true.
*/
void SyntaxHighlighter::setNoAutomaticHighlighting(bool noAutomatic)
{
    Q_D(SyntaxHighlighter);
    d->noAutomaticHighlighting = noAutomatic;
}

/*!
    Creates text format categories for the text styles themselves, so the highlighter can
    use \c{formatForCategory(C_COMMENT)} and similar, and avoid creating its own format enum.
    \sa setTextFormatCategories()
*/
void SyntaxHighlighter::setDefaultTextFormatCategories()
{
    // map all text styles to themselves
    setTextFormatCategories(C_LAST_STYLE_SENTINEL, [](int i) { return TextStyle(i); });
}

/*!
    Uses the \a formatMapping function to create a mapping from the custom formats (the ints)
    to text styles. The \a formatMapping must handle all values from 0 to \a count.

    \sa setDefaultTextFormatCategories()
    \sa setTextFormatCategories()
*/
void SyntaxHighlighter::setTextFormatCategories(int count,
                                                std::function<TextStyle(int)> formatMapping)
{
    QVector<std::pair<int, TextStyle>> categories;
    categories.reserve(count);
    for (int i = 0; i < count; ++i)
        categories.append({i, formatMapping(i)});
    setTextFormatCategories(categories);
}

/*!
    Creates a mapping between custom format enum values (the int values in the pairs) to
    text styles. Afterwards \c{formatForCategory(MyCustomFormatEnumValue)} can be used to
    efficiently retrieve the text style for a value.

    Note that this creates a vector with a size of the maximum int value in \a categories.

    \sa setDefaultTextFormatCategories()
*/
void SyntaxHighlighter::setTextFormatCategories(const QVector<std::pair<int, TextStyle>> &categories)
{
    Q_D(SyntaxHighlighter);
    d->formatCategories = categories;
    const int maxCategory = Utils::maxElementOr(categories, {-1, C_TEXT}).first;
    d->formats = QVector<QTextCharFormat>(maxCategory + 1);
    d->updateFormats(TextEditorSettings::fontSettings());
}

QTextCharFormat SyntaxHighlighter::formatForCategory(int category) const
{
    Q_D(const SyntaxHighlighter);
    QTC_ASSERT(d->formats.size() > category, return QTextCharFormat());

    return d->formats.at(category);
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    formatSpaces(text);
}

void SyntaxHighlighterPrivate::updateFormats(const FontSettings &fontSettings)
{
    this->fontSettings = fontSettings;
    // C_TEXT is handled by text editor's foreground and background color,
    // so use empty format for that
    for (const auto &pair : std::as_const(formatCategories)) {
        formats[pair.first] = pair.second == C_TEXT ? QTextCharFormat()
                                                    : fontSettings.toTextCharFormat(pair.second);
    }
    whitespaceFormat = fontSettings.toTextCharFormat(C_VISUAL_WHITESPACE);
}

} // namespace TextEditor

#include "moc_syntaxhighlighter.cpp"
