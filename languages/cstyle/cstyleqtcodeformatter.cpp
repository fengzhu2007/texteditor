#include "cstyleqtcodeformatter.h"
#include "tabsettings.h"

using namespace TextEditor;


namespace CStyle{

CreatorCodeFormatter::CreatorCodeFormatter() = default;

CreatorCodeFormatter::CreatorCodeFormatter(const TabSettings &tabSettings)
{
    setTabSize(tabSettings.m_tabSize);
    setIndentSize(tabSettings.m_indentSize);
}

void CreatorCodeFormatter::saveBlockData(QTextBlock *block, const BlockData &data) const
{
    TextBlockUserData *userData = TextDocumentLayout::userData(*block);
    auto cppData = static_cast<CStyleCodeFormatterData *>(userData->codeFormatterData());
    if (!cppData) {
        cppData = new CStyleCodeFormatterData;
        userData->setCodeFormatterData(cppData);
    }
    cppData->m_data = data;
}

bool CreatorCodeFormatter::loadBlockData(const QTextBlock &block, BlockData *data) const
{
    TextBlockUserData *userData = TextDocumentLayout::textUserData(block);
    if (!userData)
        return false;
    auto cppData = static_cast<const CStyleCodeFormatterData *>(userData->codeFormatterData());
    if (!cppData)
        return false;

    *data = cppData->m_data;
    return true;
}

void CreatorCodeFormatter::saveLexerState(QTextBlock *block, int state) const
{
    TextDocumentLayout::setLexerState(*block, state);
}

int CreatorCodeFormatter::loadLexerState(const QTextBlock &block) const
{
    return TextDocumentLayout::lexerState(block);
}


}
