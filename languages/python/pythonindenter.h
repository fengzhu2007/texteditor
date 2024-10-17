// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "textindenter.h"
#include "texteditor_global.h"

namespace Python {

TEXTEDITOR_EXPORT TextEditor::TextIndenter *createIndenter(QTextDocument *doc);

} // namespace Python
