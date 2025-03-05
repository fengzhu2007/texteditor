// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "linecolumn.h"

#include <QString>

QT_BEGIN_NAMESPACE
class QTextCursor;
class QTextDocument;
QT_END_NAMESPACE

namespace Utils {
namespace Text {


class QTCREATOR_UTILS_EXPORT Position
{
public:
    int line = 0; // 1-based
    int column = -1; // 0-based

    bool operator<(const Position &other) const
    { return line < other.line || (line == other.line && column < other.column); }
    bool operator==(const Position &other) const;

    bool operator!=(const Position &other) const { return !(operator==(other)); }

    bool isValid() const { return line > 0 && column >= 0; }

    int positionInDocument(QTextDocument *doc) const;
    QTextCursor toTextCursor(QTextDocument *doc) const;

    static Position fromFileName(QStringView fileName, int &postfixPos);
    static Position fromPositionInDocument(const QTextDocument *document, int pos);
    static Position fromCursor(const QTextCursor &cursor);

    int toPositionInDocument(const QTextDocument *document) const;
};

class QTCREATOR_UTILS_EXPORT Range
{
public:
    int length(const QString &text) const;

    Position begin;
    Position end;

    bool operator<(const Range &other) const { return begin < other.begin; }
    bool operator==(const Range &other) const;

    bool operator!=(const Range &other) const { return !(operator==(other)); }

    QTextCursor toTextCursor(QTextDocument *doc) const;
};



struct Replacement
{
    Replacement() = default;
    Replacement(int offset, int length, const QString &text)
        : offset(offset)
        , length(length)
        , text(text)
    {}

    int offset = -1;
    int length = -1;
    QString text;

    bool isValid() const { return offset >= 0 && length >= 0;  }
};
using Replacements = std::vector<Replacement>;

QTCREATOR_UTILS_EXPORT void applyReplacements(QTextDocument *doc, const Replacements &replacements);

// line is 1-based, column is 1-based
QTCREATOR_UTILS_EXPORT bool convertPosition(const QTextDocument *document,
                                            int pos,
                                            int *line, int *column);
QTCREATOR_UTILS_EXPORT
OptionalLineColumn convertPosition(const QTextDocument *document, int pos);

// line and column are 1-based
QTCREATOR_UTILS_EXPORT int positionInText(const QTextDocument *textDocument, int line, int column);

QTCREATOR_UTILS_EXPORT QString textAt(QTextCursor tc, int pos, int length);

QTCREATOR_UTILS_EXPORT QTextCursor selectAt(QTextCursor textCursor, int line, int column, uint length);

QTCREATOR_UTILS_EXPORT QTextCursor flippedCursor(const QTextCursor &cursor);

QTCREATOR_UTILS_EXPORT QTextCursor wordStartCursor(const QTextCursor &cursor);
QTCREATOR_UTILS_EXPORT QString wordUnderCursor(const QTextCursor &cursor);

QTCREATOR_UTILS_EXPORT bool utf8AdvanceCodePoint(const char *&current);
QTCREATOR_UTILS_EXPORT int utf8NthLineOffset(const QTextDocument *textDocument,
                                             const QByteArray &buffer,
                                             int line);

QTCREATOR_UTILS_EXPORT LineColumn utf16LineColumn(const QByteArray &utf8Buffer, int utf8Offset);
QTCREATOR_UTILS_EXPORT QString utf16LineTextInUtf8Buffer(const QByteArray &utf8Buffer,
                                                         int currentUtf8Offset);

QTCREATOR_UTILS_EXPORT QDebug &operator<<(QDebug &stream, const Position &pos);
} // Text
} // Utils

Q_DECLARE_METATYPE(Utils::Text::Position)
Q_DECLARE_METATYPE(Utils::Text::Range)
