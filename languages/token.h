#ifndef Code_TOKEN_H
#define Code_TOKEN_H

#include "texteditor_global.h"
#include "textdocumentlayout.h"
#include <QStack>
namespace Code{




struct State {
    State()
        : savedIndentDepth(0)
        , type(0)
    {}

    State(quint8 ty, quint16 savedDepth)
        : savedIndentDepth(savedDepth)
        , type(ty)
    {}

    quint16 savedIndentDepth;
    quint8 type;

    bool operator==(const State &other) const {
        return type == other.type
               && savedIndentDepth == other.savedIndentDepth;
    }
};

class TEXTEDITOR_EXPORT BlockData
{
public:
    BlockData()
        : m_indentDepth(0)
        , m_blockRevision(-1)
    {
    }

    QStack<State> m_beginState;
    QStack<State> m_endState;
    QStack<State> m_storedState;
    QStack<State> m_otherState;
    int m_indentDepth;
    int m_blockRevision;
};

class CodeFormatterData: public TextEditor::CodeFormatterData
{
public:
    BlockData m_data;
};

class TEXTEDITOR_EXPORT Token
{
public:
    enum Language{
        None,
        Html,
        Javascript,
        Css,
        Php,
    };
    enum Kind {
        EndOfFile,
        Keyword,
        Identifier,
        String,
        Comment,
        Number,
        Operator,
        LeftParenthesis,//(
        RightParenthesis,//)
        LeftBrace,//{
        RightBrace,//}
        LeftBracket,//[
        RightBracket,//]
        Semicolon,//;
        Colon,//:
        Comma,//,
        Dot,//.
        Equal,//=
        Delimiter,
        RegExp,



        //html token type
        InnerText,//20
        TagDefine,//DOCTYPE
        TagStart,//div
        TagEnd,//div
        TagLeftBracket,// <,</
        TagRightBracket,//>,/>
        CommentTagStart,//<!--
        CommentTagEnd,//-->
        AttrName,//xxxx
        AttrValue,//
        UnComplate,

        //php
        PhpLeftBracket,//<?
        PhpRightBracket,//?>
        StringBracket,//<<<
        TQouteTag,

        //css
        Selector,
        AtRules,
        PseudoClasses,
        Variant,
        CssOther,
        Invalid,


        Whitespace,

        //python
        Type,
        ClassField,
        MagicAttr,
        ImportedModule,

        XmlLeftBracket,//<?
        XmlRightBracket,
        XmlDataLeftTag,
        XmlDataRightTag,
        XmlData,

        //typescript
        Decorator,
        Generic,



        Doxygen,
        TokenEnd

    };

    inline Token(): offset(0), length(0), kind(EndOfFile) {}
    inline Token(int o, int l, Kind k,Language lang=Html): offset(o), length(l), kind(k), lang(lang) {}
    inline int begin() const { return offset; }
    inline int end() const { return offset + length; }
    inline bool is(int k,int l) const { return k == kind && l==lang; }
    inline bool is(int k) const { return k == kind; }
    inline bool isNot(int k,int l) const { return k != kind || l != lang; }
    inline bool isNot(int k) const { return k != kind; }

    static int compare(const Token &t1, const Token &t2) {
        if (int c = t1.offset - t2.offset)
            return c;
        if (int c = t1.length - t2.length)
            return c;
        if (int c = t1.kind - t2.kind)
            return c;
        return int(t1.lang) - int(t2.lang);
    }

    bool isEndOfBlock() { return offset == -1; }

public:
    int offset = 0;
    int length = 0;
    Kind kind = EndOfFile;
    Language lang = None;
};

inline int operator == (const Token &t1, const Token &t2) {
    return Token::compare(t1, t2) == 0;
}
inline int operator != (const Token &t1, const Token &t2) {
    return Token::compare(t1, t2) != 0;
}





}

#endif // Code_TOKEN_H
