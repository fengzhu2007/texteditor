// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"
#include "../token.h"

#include <QStringList>


namespace Html{
class Scanner;
}

namespace Css {

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



        AtRulesABegin = MultiLineComment<<14,//single line
        AtRulesBBegin = MultiLineComment<<15,//attribute container
        AtRulesCBegin = MultiLineComment<<16,//selector container


        AtRulesBBeginAttrList = MultiLineComment<<17,//in AtRulesBBegin

        AtRulesCBeginSelectorList = MultiLineComment<<18,
        AtRulesCBeginSelectorAttrList = MultiLineComment<<19,//in at rule selector (MultiLineAtRules)

        MultiLineAttrList = MultiLineComment<<26,
        MultiLineAttrValue = MultiLineComment<<27,
        PseudoClasses = MultiLineComment<<28,
        RootPseudoClasses = MultiLineComment<<29,

    };

    Scanner(Html::Scanner* htmlScanner=nullptr);
    virtual ~Scanner();

    bool scanComments() const;
    void setScanComments(bool scanComments);

    QList<Code::Token> operator()(int& from,const QString &text, int& startState);
    int state() const;

    bool isKeyword(const QString &text) const;
    static QStringList keywords();

private:
    int _state;
    bool _scanComments: 1;
    Html::Scanner* pHtmlScanner;
};

} // namespace CStyle
