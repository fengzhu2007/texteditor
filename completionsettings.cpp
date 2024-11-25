// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "completionsettings.h"

#include <QSettings>

static const char settingsGroup[]               = "CppTools/Completion";
static const char caseSensitivityKey[]          = "CaseSensitivity";
static const char completionTriggerKey[]        = "CompletionTrigger";
static const char automaticProposalTimeoutKey[] = "AutomaticProposalTimeout";
static const char characterThresholdKey[]       = "CharacterThreshold";
static const char autoInsertBracesKey[]         = "AutoInsertBraces";
static const char surroundingAutoBracketsKey[]  = "SurroundingAutoBrackets";
static const char autoInsertQuotesKey[]         = "AutoInsertQuotes";
static const char surroundingAutoQuotesKey[]    = "SurroundingAutoQuotes";
static const char partiallyCompleteKey[]        = "PartiallyComplete";
static const char spaceAfterFunctionNameKey[]   = "SpaceAfterFunctionName";
static const char autoSplitStringsKey[]         = "AutoSplitStrings";
static const char animateAutoCompleteKey[]      = "AnimateAutoComplete";
static const char highlightAutoCompleteKey[]    = "HighlightAutoComplete";
static const char skipAutoCompleteKey[]         = "SkipAutoComplete";
static const char autoRemoveKey[]               = "AutoRemove";
static const char overwriteClosingCharsKey[]    = "OverwriteClosingChars";

using namespace TextEditor;

void CompletionSettings::toSettings(QSettings *s) const
{
    s->beginGroup(settingsGroup);
    s->setValue(caseSensitivityKey, (int) m_caseSensitivity);
    s->setValue(completionTriggerKey, (int) m_completionTrigger);
    s->setValue(automaticProposalTimeoutKey, m_automaticProposalTimeoutInMs);
    s->setValue(characterThresholdKey, m_characterThreshold);
    s->setValue(autoInsertBracesKey, m_autoInsertBrackets);
    s->setValue(surroundingAutoBracketsKey, m_surroundingAutoBrackets);
    s->setValue(autoInsertQuotesKey, m_autoInsertQuotes);
    s->setValue(surroundingAutoQuotesKey, m_surroundingAutoQuotes);
    s->setValue(partiallyCompleteKey, m_partiallyComplete);
    s->setValue(spaceAfterFunctionNameKey, m_spaceAfterFunctionName);
    s->setValue(autoSplitStringsKey, m_autoSplitStrings);
    s->setValue(animateAutoCompleteKey, m_animateAutoComplete);
    s->setValue(highlightAutoCompleteKey, m_highlightAutoComplete);
    s->setValue(skipAutoCompleteKey, m_skipAutoCompletedText);
    s->setValue(autoRemoveKey, m_autoRemove);
    s->setValue(overwriteClosingCharsKey, m_overwriteClosingChars);
    s->endGroup();
}

void CompletionSettings::fromSettings(QSettings *s)
{
    *this = CompletionSettings(); // Assign defaults

    s->beginGroup(settingsGroup);
    m_caseSensitivity = (CaseSensitivity)
            s->value(caseSensitivityKey, m_caseSensitivity).toInt();
    m_completionTrigger = (CompletionTrigger)
            s->value(completionTriggerKey, m_completionTrigger).toInt();
    m_automaticProposalTimeoutInMs =
            s->value(automaticProposalTimeoutKey, m_automaticProposalTimeoutInMs).toInt();
    m_characterThreshold =
            s->value(characterThresholdKey, m_characterThreshold).toInt();
    m_autoInsertBrackets =
            s->value(autoInsertBracesKey, m_autoInsertBrackets).toBool();
    m_surroundingAutoBrackets =
            s->value(surroundingAutoBracketsKey, m_surroundingAutoBrackets).toBool();
    m_autoInsertQuotes =
            s->value(autoInsertQuotesKey, m_autoInsertQuotes).toBool();
    m_surroundingAutoQuotes =
            s->value(surroundingAutoQuotesKey, m_surroundingAutoQuotes).toBool();
    m_partiallyComplete =
            s->value(partiallyCompleteKey, m_partiallyComplete).toBool();
    m_spaceAfterFunctionName =
            s->value(spaceAfterFunctionNameKey, m_spaceAfterFunctionName).toBool();
    m_autoSplitStrings =
            s->value(autoSplitStringsKey, m_autoSplitStrings).toBool();
    m_animateAutoComplete =
            s->value(animateAutoCompleteKey, m_animateAutoComplete).toBool();
    m_highlightAutoComplete =
            s->value(highlightAutoCompleteKey, m_highlightAutoComplete).toBool();
    m_skipAutoCompletedText =
            s->value(skipAutoCompleteKey, m_skipAutoCompletedText).toBool();
    m_autoRemove =
            s->value(autoRemoveKey, m_autoRemove).toBool();
    m_overwriteClosingChars =
            s->value(overwriteClosingCharsKey, m_overwriteClosingChars).toBool();
    s->endGroup();
}

QJsonObject CompletionSettings::toJson() const{
    return {
        {caseSensitivityKey,(int)m_caseSensitivity},
        {completionTriggerKey,(int)m_completionTrigger},
        {automaticProposalTimeoutKey,m_automaticProposalTimeoutInMs},
        {characterThresholdKey,m_characterThreshold},
        {autoInsertBracesKey,m_autoInsertBrackets},
        {surroundingAutoBracketsKey,m_surroundingAutoBrackets},
        {autoInsertQuotesKey,m_autoInsertQuotes},
        {surroundingAutoQuotesKey,m_surroundingAutoQuotes},
        {partiallyCompleteKey,m_partiallyComplete},
        {spaceAfterFunctionNameKey,m_spaceAfterFunctionName},
        {autoSplitStringsKey,m_autoSplitStrings},
        {animateAutoCompleteKey,m_animateAutoComplete},
        {highlightAutoCompleteKey,m_highlightAutoComplete},
        {skipAutoCompleteKey,m_skipAutoCompletedText},
        {autoRemoveKey,m_autoRemove},
        {overwriteClosingCharsKey,m_overwriteClosingChars},
    };
}

void CompletionSettings::fromJson(const QJsonObject& data){
    if(data.contains(caseSensitivityKey)){
        m_caseSensitivity = (CaseSensitivity)data.find(caseSensitivityKey)->toInt();
    }
    if(data.contains(completionTriggerKey)){
        m_completionTrigger = (CompletionTrigger)data.find(completionTriggerKey)->toInt();
    }
    if(data.contains(automaticProposalTimeoutKey)){
        m_automaticProposalTimeoutInMs = data.find(automaticProposalTimeoutKey)->toInt();
    }
    if(data.contains(characterThresholdKey)){
        m_characterThreshold = data.find(characterThresholdKey)->toInt();
    }
    if(data.contains(autoInsertBracesKey)){
        m_autoInsertBrackets = data.find(autoInsertBracesKey)->toBool();
    }
    if(data.contains(surroundingAutoBracketsKey)){
        m_surroundingAutoBrackets = data.find(surroundingAutoBracketsKey)->toBool();
    }
    if(data.contains(autoInsertQuotesKey)){
        m_autoInsertQuotes = data.find(autoInsertQuotesKey)->toBool();
    }
    if(data.contains(surroundingAutoQuotesKey)){
        m_surroundingAutoQuotes = data.find(surroundingAutoQuotesKey)->toBool();
    }
    if(data.contains(partiallyCompleteKey)){
        m_partiallyComplete = data.find(partiallyCompleteKey)->toBool();
    }
    if(data.contains(spaceAfterFunctionNameKey)){
        m_spaceAfterFunctionName = data.find(spaceAfterFunctionNameKey)->toBool();
    }
    if(data.contains(autoSplitStringsKey)){
        m_autoSplitStrings = data.find(autoSplitStringsKey)->toBool();
    }
    if(data.contains(animateAutoCompleteKey)){
        m_animateAutoComplete = data.find(animateAutoCompleteKey)->toBool();
    }
    if(data.contains(highlightAutoCompleteKey)){
        m_highlightAutoComplete = data.find(highlightAutoCompleteKey)->toBool();
    }
    if(data.contains(skipAutoCompleteKey)){
        m_skipAutoCompletedText = data.find(skipAutoCompleteKey)->toBool();
    }
    if(data.contains(autoRemoveKey)){
        m_autoRemove = data.find(autoRemoveKey)->toBool();
    }
    if(data.contains(overwriteClosingCharsKey)){
        m_overwriteClosingChars = data.find(overwriteClosingCharsKey)->toBool();
    }
}

bool CompletionSettings::equals(const CompletionSettings &cs) const
{
    return m_caseSensitivity                == cs.m_caseSensitivity
        && m_completionTrigger              == cs.m_completionTrigger
        && m_automaticProposalTimeoutInMs   == cs.m_automaticProposalTimeoutInMs
        && m_characterThreshold             == cs.m_characterThreshold
        && m_autoInsertBrackets             == cs.m_autoInsertBrackets
        && m_surroundingAutoBrackets        == cs.m_surroundingAutoBrackets
        && m_autoInsertQuotes               == cs.m_autoInsertQuotes
        && m_surroundingAutoQuotes          == cs.m_surroundingAutoQuotes
        && m_partiallyComplete              == cs.m_partiallyComplete
        && m_spaceAfterFunctionName         == cs.m_spaceAfterFunctionName
        && m_autoSplitStrings               == cs.m_autoSplitStrings
        && m_animateAutoComplete            == cs.m_animateAutoComplete
        && m_highlightAutoComplete          == cs.m_highlightAutoComplete
        && m_skipAutoCompletedText          == cs.m_skipAutoCompletedText
        && m_autoRemove                     == cs.m_autoRemove
        && m_overwriteClosingChars          == cs.m_overwriteClosingChars
        ;
}
