// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "cppcodestylesettings.h"

//#include "cppeditorconstants.h"
//#include "cpptoolssettings.h"



#include "tabsettings.h"

//#include <cplusplus/Overview.h>


static const char indentBlockBracesKey[] = "IndentBlockBraces";
static const char indentBlockBodyKey[] = "IndentBlockBody";
static const char indentClassBracesKey[] = "IndentClassBraces";
static const char indentEnumBracesKey[] = "IndentEnumBraces";
static const char indentNamespaceBracesKey[] = "IndentNamespaceBraces";
static const char indentNamespaceBodyKey[] = "IndentNamespaceBody";
static const char indentAccessSpecifiersKey[] = "IndentAccessSpecifiers";
static const char indentDeclarationsRelativeToAccessSpecifiersKey[] = "IndentDeclarationsRelativeToAccessSpecifiers";
static const char indentFunctionBodyKey[] = "IndentFunctionBody";
static const char indentFunctionBracesKey[] = "IndentFunctionBraces";
static const char indentSwitchLabelsKey[] = "IndentSwitchLabels";
static const char indentStatementsRelativeToSwitchLabelsKey[] = "IndentStatementsRelativeToSwitchLabels";
static const char indentBlocksRelativeToSwitchLabelsKey[] = "IndentBlocksRelativeToSwitchLabels";
static const char indentControlFlowRelativeToSwitchLabelsKey[] = "IndentControlFlowRelativeToSwitchLabels";
static const char bindStarToIdentifierKey[] = "BindStarToIdentifier";
static const char bindStarToTypeNameKey[] = "BindStarToTypeName";
static const char bindStarToLeftSpecifierKey[] = "BindStarToLeftSpecifier";
static const char bindStarToRightSpecifierKey[] = "BindStarToRightSpecifier";
static const char extraPaddingForConditionsIfConfusingAlignKey[] = "ExtraPaddingForConditionsIfConfusingAlign";
static const char alignAssignmentsKey[] = "AlignAssignments";
static const char shortGetterNameKey[] = "ShortGetterName";

namespace CppEditor {

// ------------------ CppCodeStyleSettingsWidget

CppCodeStyleSettings::CppCodeStyleSettings() = default;

QVariantMap CppCodeStyleSettings::toMap() const
{
    return {
        {indentBlockBracesKey, indentBlockBraces},
        {indentBlockBodyKey, indentBlockBody},
        {indentClassBracesKey, indentClassBraces},
        {indentEnumBracesKey, indentEnumBraces},
        {indentNamespaceBracesKey, indentNamespaceBraces},
        {indentNamespaceBodyKey, indentNamespaceBody},
        {indentAccessSpecifiersKey, indentAccessSpecifiers},
        {indentDeclarationsRelativeToAccessSpecifiersKey, indentDeclarationsRelativeToAccessSpecifiers},
        {indentFunctionBodyKey, indentFunctionBody},
        {indentFunctionBracesKey, indentFunctionBraces},
        {indentSwitchLabelsKey, indentSwitchLabels},
        {indentStatementsRelativeToSwitchLabelsKey, indentStatementsRelativeToSwitchLabels},
        {indentBlocksRelativeToSwitchLabelsKey, indentBlocksRelativeToSwitchLabels},
        {indentControlFlowRelativeToSwitchLabelsKey, indentControlFlowRelativeToSwitchLabels},
        {bindStarToIdentifierKey, bindStarToIdentifier},
        {bindStarToTypeNameKey, bindStarToTypeName},
        {bindStarToLeftSpecifierKey, bindStarToLeftSpecifier},
        {bindStarToRightSpecifierKey, bindStarToRightSpecifier},
        {extraPaddingForConditionsIfConfusingAlignKey, extraPaddingForConditionsIfConfusingAlign},
        {alignAssignmentsKey, alignAssignments},
        {shortGetterNameKey, preferGetterNameWithoutGetPrefix}
    };
}

void CppCodeStyleSettings::fromMap(const QVariantMap &map)
{
    indentBlockBraces = map.value(indentBlockBracesKey, indentBlockBraces).toBool();
    indentBlockBody = map.value(indentBlockBodyKey, indentBlockBody).toBool();
    indentClassBraces = map.value(indentClassBracesKey, indentClassBraces).toBool();
    indentEnumBraces = map.value(indentEnumBracesKey, indentEnumBraces).toBool();
    indentNamespaceBraces = map.value(indentNamespaceBracesKey, indentNamespaceBraces).toBool();
    indentNamespaceBody = map.value(indentNamespaceBodyKey, indentNamespaceBody).toBool();
    indentAccessSpecifiers = map.value(indentAccessSpecifiersKey, indentAccessSpecifiers).toBool();
    indentDeclarationsRelativeToAccessSpecifiers =
            map.value(indentDeclarationsRelativeToAccessSpecifiersKey,
                      indentDeclarationsRelativeToAccessSpecifiers).toBool();
    indentFunctionBody = map.value(indentFunctionBodyKey, indentFunctionBody).toBool();
    indentFunctionBraces = map.value(indentFunctionBracesKey, indentFunctionBraces).toBool();
    indentSwitchLabels = map.value(indentSwitchLabelsKey, indentSwitchLabels).toBool();
    indentStatementsRelativeToSwitchLabels = map.value(indentStatementsRelativeToSwitchLabelsKey,
                                indentStatementsRelativeToSwitchLabels).toBool();
    indentBlocksRelativeToSwitchLabels = map.value(indentBlocksRelativeToSwitchLabelsKey,
                                indentBlocksRelativeToSwitchLabels).toBool();
    indentControlFlowRelativeToSwitchLabels = map.value(indentControlFlowRelativeToSwitchLabelsKey,
                                indentControlFlowRelativeToSwitchLabels).toBool();
    bindStarToIdentifier = map.value(bindStarToIdentifierKey, bindStarToIdentifier).toBool();
    bindStarToTypeName = map.value(bindStarToTypeNameKey, bindStarToTypeName).toBool();
    bindStarToLeftSpecifier = map.value(bindStarToLeftSpecifierKey, bindStarToLeftSpecifier).toBool();
    bindStarToRightSpecifier = map.value(bindStarToRightSpecifierKey, bindStarToRightSpecifier).toBool();
    extraPaddingForConditionsIfConfusingAlign = map.value(extraPaddingForConditionsIfConfusingAlignKey,
                                extraPaddingForConditionsIfConfusingAlign).toBool();
    alignAssignments = map.value(alignAssignmentsKey, alignAssignments).toBool();
    preferGetterNameWithoutGetPrefix = map.value(shortGetterNameKey,
                                preferGetterNameWithoutGetPrefix).toBool();
}

bool CppCodeStyleSettings::equals(const CppCodeStyleSettings &rhs) const
{
    return indentBlockBraces == rhs.indentBlockBraces
           && indentBlockBody == rhs.indentBlockBody
           && indentClassBraces == rhs.indentClassBraces
           && indentEnumBraces == rhs.indentEnumBraces
           && indentNamespaceBraces == rhs.indentNamespaceBraces
           && indentNamespaceBody == rhs.indentNamespaceBody
           && indentAccessSpecifiers == rhs.indentAccessSpecifiers
           && indentDeclarationsRelativeToAccessSpecifiers == rhs.indentDeclarationsRelativeToAccessSpecifiers
           && indentFunctionBody == rhs.indentFunctionBody
           && indentFunctionBraces == rhs.indentFunctionBraces
           && indentSwitchLabels == rhs.indentSwitchLabels
           && indentStatementsRelativeToSwitchLabels == rhs.indentStatementsRelativeToSwitchLabels
           && indentBlocksRelativeToSwitchLabels == rhs.indentBlocksRelativeToSwitchLabels
           && indentControlFlowRelativeToSwitchLabels == rhs.indentControlFlowRelativeToSwitchLabels
           && bindStarToIdentifier == rhs.bindStarToIdentifier
           && bindStarToTypeName == rhs.bindStarToTypeName
           && bindStarToLeftSpecifier == rhs.bindStarToLeftSpecifier
           && bindStarToRightSpecifier == rhs.bindStarToRightSpecifier
           && extraPaddingForConditionsIfConfusingAlign == rhs.extraPaddingForConditionsIfConfusingAlign
           && alignAssignments == rhs.alignAssignments
           && preferGetterNameWithoutGetPrefix == rhs.preferGetterNameWithoutGetPrefix
           ;
}

CppCodeStyleSettings CppCodeStyleSettings::currentProjectCodeStyle()
{
    return {};
}

CppCodeStyleSettings CppCodeStyleSettings::currentGlobalCodeStyle()
{
    return {};
}


TextEditor::TabSettings CppCodeStyleSettings::currentProjectTabSettings()
{
    return {};
}

TextEditor::TabSettings CppCodeStyleSettings::currentGlobalTabSettings()
{
    return {};
}



} // namespace CppEditor
