// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "completionassistprovider.h"

#include "texteditorconstants.h"

//#include "assistproposaliteminterface.h"

#include <QMap>
namespace TextEditor {

class AssistProposalItemInterface;
class DocumentContentCompletionProviderPrivate;


class TEXTEDITOR_EXPORT Keywords
{
public:
    Keywords() = default;

    bool isVariable(const QString &word) const;
    bool isFunction(const QString &word) const;

    QStringList variables() const;
    QStringList functions() const;
    QStringList keywords() const;
    QStringList classes() const;
    QStringList others() const;
    QStringList argsForFunction(const QString &function) const;

    void setVariables(const QStringList& words);
    void setFunctions(const QStringList& words);
    void setKeywords(const QStringList& words);
    void setClasses(const QStringList& words);
    void setOthers(const QStringList& words);

    bool contains(const QString& word) const;

private:
    QStringList m_keywords;
    QStringList m_variables;
    QStringList m_functions;
    QStringList m_classes;
    QStringList m_others;
    QMap<QString, QStringList> m_functionArgs;
};



class TEXTEDITOR_EXPORT DocumentContentCompletionProvider : public CompletionAssistProvider
{
    Q_OBJECT

public:
    DocumentContentCompletionProvider(const QString &snippetGroup = QString(Constants::TEXT_SNIPPET_GROUP_ID));
    ~DocumentContentCompletionProvider();

    RunType runType() const override;
    IAssistProcessor *createProcessor(const AssistInterface *) const override;
    //void setKeyWords(const Keywords& keyWords);
    void setKeywordList(const QStringList& words);
    void setVariableList(const QStringList& words);
    void setFunctionList(const QStringList& words);
    void setClassList(const QStringList& words);
    void setOtherList(const QStringList& words);

private:
    QString m_snippetGroup;
    DocumentContentCompletionProviderPrivate* d;

    //Keywords m_keyWords;

};

} // TextEditor
