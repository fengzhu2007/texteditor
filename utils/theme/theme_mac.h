// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <qglobal.h>

QT_BEGIN_NAMESPACE
class QMenu;
QT_END_NAMESPACE

namespace Utils {
namespace Internal {

void forceMacAppearance(bool dark);
bool currentAppearanceIsDark();
void setMacOSHelpMenu(QMenu *menu);

} // Internal
} // Utils
