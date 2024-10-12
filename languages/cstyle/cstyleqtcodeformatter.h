#ifndef CSTYLEQTCODEFORMATTER_H
#define CSTYLEQTCODEFORMATTER_H

#include "texteditor_global.h"
#include "textdocumentlayout.h"
#include "cstylecodeformatter.h"

class CStyleCodeFormatter
{
public:
    CStyleCodeFormatter();
};



namespace TextEditor { class TabSettings; }

namespace CStyle {

class TEXTEDITOR_EXPORT CreatorCodeFormatter : public CStyle::QtStyleCodeFormatter
{
public:
    CreatorCodeFormatter();
    explicit CreatorCodeFormatter(const TextEditor::TabSettings &tabSettings);

protected:
    void saveBlockData(QTextBlock *block, const BlockData &data) const override;
    bool loadBlockData(const QTextBlock &block, BlockData *data) const override;

    void saveLexerState(QTextBlock *block, int state) const override;
    int loadLexerState(const QTextBlock &block) const override;

private:
    class CStyleCodeFormatterData: public TextEditor::CodeFormatterData
    {
    public:
        CStyle::CodeFormatter::BlockData m_data;
    };
};

} // namespace QmlJSTools


#endif // CSTYLEQTCODEFORMATTER_H
