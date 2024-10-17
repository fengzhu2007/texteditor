// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "syntaxhighlighter.h"

namespace Python {

class Scanner;

class Highlighter : public TextEditor::SyntaxHighlighter
{
public:
    Highlighter();

private:
    void highlightBlock(const QString &text) override;
    int highlightLine(const QString &text, int initialState);
    void highlightImport(Scanner &scanner);

    int m_lastIndent = 0;
    bool withinLicenseHeader = false;
};


} // namespace Python
