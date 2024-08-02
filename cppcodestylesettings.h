// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"

#include <QVariantMap>

#include <optional>

namespace CPlusPlus { class Overview; }
namespace TextEditor { class TabSettings; }
namespace ProjectExplorer { class Project; }

namespace CppEditor {

class TEXTEDITOR_EXPORT CppCodeStyleSettings
{
public:
    CppCodeStyleSettings();

    bool indentBlockBraces = false;
    bool indentBlockBody = true;
    bool indentClassBraces = false;
    bool indentEnumBraces = false;
    bool indentNamespaceBraces = false;
    bool indentNamespaceBody = false;
    bool indentAccessSpecifiers = false;
    bool indentDeclarationsRelativeToAccessSpecifiers = true;
    bool indentFunctionBody = true;
    bool indentFunctionBraces = false;
    bool indentSwitchLabels = false;
    bool indentStatementsRelativeToSwitchLabels = true;
    bool indentBlocksRelativeToSwitchLabels = false;
    bool indentControlFlowRelativeToSwitchLabels = true;

    // Formatting of pointer and reference declarations, see Overview::StarBindFlag.
    bool bindStarToIdentifier = true;
    bool bindStarToTypeName = false;
    bool bindStarToLeftSpecifier = false;
    bool bindStarToRightSpecifier = false;

    // false: if (a &&
    //            b)
    //            c;
    // true:  if (a &&
    //                b)
    //            c;
    // but always: while (a &&
    //                    b)
    //                 foo;
    bool extraPaddingForConditionsIfConfusingAlign = true;

    // false: a = a +
    //                b;
    // true:  a = a +
    //            b
    bool alignAssignments = false;

    // TODO only kept to allow conversion to the new setting getterNameTemplate in
    // CppEditor/QuickFixSetting. Remove in 4.16
    bool preferGetterNameWithoutGetPrefix = true;

    QVariantMap toMap() const;
    void fromMap(const QVariantMap &map);

    bool equals(const CppCodeStyleSettings &rhs) const;
    bool operator==(const CppCodeStyleSettings &s) const { return equals(s); }
    bool operator!=(const CppCodeStyleSettings &s) const { return !equals(s); }

    static CppCodeStyleSettings currentProjectCodeStyle();
    static CppCodeStyleSettings currentGlobalCodeStyle();
    static TextEditor::TabSettings currentProjectTabSettings();
    static TextEditor::TabSettings currentGlobalTabSettings();

};

} // namespace CppEditor

Q_DECLARE_METATYPE(CppEditor::CppCodeStyleSettings)
