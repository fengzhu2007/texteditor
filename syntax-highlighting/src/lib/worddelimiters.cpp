/*
    SPDX-FileCopyrightText: 2020 Jonathan Poelen <jonathan.poelen@gmail.com>

    SPDX-License-Identifier: MIT
*/

#include "worddelimiters_p.h"

using namespace KSyntaxHighlighting;

WordDelimiters::WordDelimiters()
    : asciiDelimiters{}
{
    for (const char *p = "\t !%&()*+,-./:;<=>?[\\]^{|}~"; *p; ++p) {
        asciiDelimiters.set(*p);
    }
}

bool WordDelimiters::contains(QChar c) const
{
    if (c.unicode() < 128) {
        return asciiDelimiters.test(c.unicode());
    }
    // perf tells contains is MUCH faster than binary search here, very short array
    return notAsciiDelimiters.contains(c);
}

void WordDelimiters::append(QStringView s)
{
    for (QChar c : s) {
        if (c.unicode() < 128) {
            asciiDelimiters.set(c.unicode());
        } else {
            notAsciiDelimiters.append(c);
        }
    }
}

void WordDelimiters::remove(QStringView s)
{
    for (QChar c : s) {
        if (c.unicode() < 128) {
            asciiDelimiters.set(c.unicode(), false);
        } else {
            notAsciiDelimiters.remove(c);
        }
    }
}
