// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"
#include "../token.h"

#include <QStringList>

namespace Jsx {

class TEXTEDITOR_EXPORT Scanner
{
public:
    enum {
        FlagsBits = 4,
        BraceCounterBits = 7
    };
    enum {
        Normal = 0,
        MultiLineComment = 1,
        MultiLineStringDQuote = MultiLineComment<<1,
        MultiLineStringSQuote = MultiLineComment<<2,
        MultiLineStringBQuote = MultiLineComment<<3,
        MultiLineMask = MultiLineComment | MultiLineStringDQuote | MultiLineStringBQuote,

        RegexpMayFollow = MultiLineComment<<11, // flag that may be combined with the above


        MultiLineElement = MultiLineComment<<4,
        MultiLineElement1 = MultiLineComment<<5,
        MultiLineElement2 = MultiLineComment<<6,
        MultiLineElement3 = MultiLineComment<<7,
        MultiLineElement4 = MultiLineComment<<8,
        MultiLineElement5 = MultiLineComment<<9,
        MultiLineElement6 = MultiLineComment<<10,
        MultiLineElementMask = MultiLineElement|MultiLineElement1|MultiLineElement2|MultiLineElement3|MultiLineElement4|MultiLineElement5|MultiLineElement6,

        ElementStartTag = MultiLineComment<<12,
        ElementEndTag = MultiLineComment<<13,

        MultiLineAttrValue = MultiLineComment << 14,


        MultiLineInnerText = MultiLineComment<<15,


        // templates can be nested, which means that the scanner/lexer cannot
        // be a simple state machine anymore, but should have a stack to store
        // the state (the number of open braces in the current template
        // string).
        // The lexer stare is currently stored in an int, so we abuse that and
        // store a the number of open braces (maximum 0x7f = 127) for at most 5
        // nested templates in the int after the flags for the multiline
        // comments and strings.
        TemplateExpression = MultiLineComment << 20,
        TemplateExpressionOpenBracesMask0 = MultiLineComment << 21,
        TemplateExpressionOpenBracesMask1 = MultiLineComment << 22,
        TemplateExpressionOpenBracesMask2 = MultiLineComment << 23,
        TemplateExpressionOpenBracesMask3 = MultiLineComment << 24,
        TemplateExpressionOpenBracesMask4 = MultiLineComment << 25,
        TemplateExpressionOpenBracesMask = TemplateExpressionOpenBracesMask1 | TemplateExpressionOpenBracesMask2
                                           | TemplateExpressionOpenBracesMask3 | TemplateExpressionOpenBracesMask4
    };

    Scanner();
    virtual ~Scanner();

    bool scanComments() const;
    void setScanComments(bool scanComments);

    QList<Code::Token> operator()(int& from,const QString &text, int startState,const QStack<int>& stateStack={});
    int state() const;

    bool isKeyword(const QString &text) const;
    static QStringList keywords();

    inline QStack<int>& statesStack() {return m_stateStack;}

private:
    int _state;
    bool _scanComments: 1;
    QStack<int> m_stateStack;

};

} // namespace CStyle
