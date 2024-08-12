// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "core/core_global.h"

#include <QMetaType>
#include <QFlags>
#include <QTextDocument>

namespace Core {
namespace Constants {



} // namespace Constants

enum FindFlag {
    FindBackward = 0x01,
    FindCaseSensitively = 0x02,
    FindWholeWords = 0x04,
    FindRegularExpression = 0x08,
    FindPreserveCase = 0x10
};
Q_DECLARE_FLAGS(FindFlags, FindFlag)

// defined in findplugin.cpp
QTextDocument::FindFlags CORE_EXPORT textDocumentFlagsForFindFlags(FindFlags flags);

} // namespace Core

Q_DECLARE_OPERATORS_FOR_FLAGS(Core::FindFlags)
Q_DECLARE_METATYPE(Core::FindFlags)
