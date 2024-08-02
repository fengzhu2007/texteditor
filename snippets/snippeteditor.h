// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"
#include "texteditor.h"

namespace TextEditor {

class TEXTEDITOR_EXPORT SnippetEditorWidget : public TextEditorWidget
{
    Q_OBJECT

public:
    SnippetEditorWidget(QWidget *parent = nullptr);

signals:
    void snippetContentChanged();

protected:
    void focusOutEvent(QFocusEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *e) override;

    int extraAreaWidth(int * /* markWidthPtr */ = nullptr) const override { return 0; }
};

} // namespace TextEditor
