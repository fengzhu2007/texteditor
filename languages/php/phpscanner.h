// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"
#include "../token.h"
#include <QStringList>
namespace Html{
class Scanner;
}
namespace Php {

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
        MultiLineStringTQuote = MultiLineComment<<3,
        MultiLineMask = MultiLineComment | MultiLineStringDQuote | MultiLineStringSQuote | MultiLineStringTQuote,


        PHPTQouteStart = MultiLineComment << 11,
        PHPTQouteEnd = MultiLineComment << 12,

        //RegexpMayFollow = 8, // flag that may be combined with the above

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

    Scanner(Html::Scanner* htmlScanner=nullptr);
    virtual ~Scanner();

    bool scanComments() const;
    void setScanComments(bool scanComments);

    QList<Code::Token> operator()(int& from,const QString &text, int& startState);
    int state() const;

    bool isKeyword(const QString &text) const;
    static QStringList keywords();

    static QString currentTQouteTag();
    static void setCurrentTQouteTag(const QString& tag);

private:
    int _state;
    bool _scanComments: 1;
    Html::Scanner* pHtmlScanner;
    static QString TQuoteTag;
};

} // namespace CStyle
