#pragma once

#include "texteditor_global.h"

#include "languages/token.h"
#include "languages/php/phpscanner.h"
#include "languages/javascript/jsscanner.h"
#include "languages/css/cssscanner.h"
#include <QStringList>

using namespace Code;

namespace Html {

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
        MultiLineStringDQuote = MultiLineComment<<1,//"
        MultiLineStringSQuote = MultiLineComment<<2,//'

        MultiLineMask = MultiLineComment |MultiLineStringDQuote|MultiLineStringSQuote,

        MultiLineElement = MultiLineComment<<4,
        MultiLineCss= MultiLineComment<<5,
        MultiLineJavascript=MultiLineComment<<6,
        MultiLinePhp=MultiLineComment<<7,
        MultiLineAttrValue = MultiLineComment << 8,

        JavascriptTagStart = MultiLineComment << 9,
        CSSTagStart = MultiLineComment << 10,



        TemplateExpressionOpenBracesMask0 = MultiLineComment << 21,
        TemplateExpressionOpenBracesMask1 = MultiLineComment << 22,
        TemplateExpressionOpenBracesMask2 = MultiLineComment << 23,
        TemplateExpressionOpenBracesMask3 = MultiLineComment << 24,
        TemplateExpressionOpenBracesMask4 = MultiLineComment << 25,

    };

    Scanner();
    virtual ~Scanner();

    bool scanComments() const;
    void setScanComments(bool scanComments);

    QList<Token> operator()(int& from,const QString &text, int startState = Normal);
    int state() const;

    bool isKeyword(const QString &text) const;

    inline bool maybyText(){
        return _state==Normal;
    }


    static QStringList keywords();
    static void dump(const QString &text,QList<Token> tokens);

private:
    int _state;
    bool _scanComments: 1;
    Php::Scanner phpScanner;
    Javascript::Scanner jsScanner;
    Css::Scanner cssScanner;

    friend Php::Scanner;
    friend Javascript::Scanner;
    friend Css::Scanner;

};

}


