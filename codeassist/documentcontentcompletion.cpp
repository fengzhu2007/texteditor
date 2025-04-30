// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "documentcontentcompletion.h"

#include "assistinterface.h"
#include "assistproposalitem.h"
#include "genericproposal.h"
//#include "genericproposalmodel.h"
#include "iassistprocessor.h"
//#include "../snippets/snippetassistcollector.h"
#include "../completionsettings.h"
#include "../texteditorsettings.h"

#include "textdocumentlayout.h"
#include "codeformatter.h"
#include "languages/token.h"

#include "utils/algorithm.h"
#include "utils/runextensions.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSet>
#include <QTextBlock>
#include <QTextDocument>
#include <QFutureInterface>
#include <QFutureWatcher>

using namespace TextEditor;


namespace TextEditor{

class DocumentContentCompletionProviderPrivate{
public:
    Keywords keywords;

};
}




bool Keywords::isVariable(const QString &word) const
{
    return std::binary_search(m_variables.constBegin(), m_variables.constEnd(), word);
}

bool Keywords::isFunction(const QString &word) const
{
    return std::binary_search(m_functions.constBegin(), m_functions.constEnd(), word);
}

QStringList Keywords::variables() const
{
    return m_variables;
}

QStringList Keywords::functions() const
{
    return m_functions;
}

QStringList Keywords::keywords() const{
    return m_keywords;
}

QStringList Keywords::classes() const{
    return m_classes;
}

QStringList Keywords::others() const{
    return m_others;
}

void Keywords::setVariables(const QStringList& words){
    m_variables = words;
    Utils::sort(m_variables);
}
void Keywords::setFunctions(const QStringList& words){
    m_functions = words;
    Utils::sort(m_functions);
}
void Keywords::setKeywords(const QStringList& words){
    m_keywords = words;
    Utils::sort(m_keywords);
}
void Keywords::setClasses(const QStringList& words){
    m_classes = words;
    Utils::sort(m_classes);
}
void Keywords::setOthers(const QStringList& words){
    m_others = words;
    Utils::sort(m_others);
}

bool Keywords::contains(const QString& word) const{
    return m_keywords.contains(word) || m_variables.contains(word) || m_functions.contains(word) || m_classes.contains(word) || m_others.contains(word);
}

QStringList Keywords::argsForFunction(const QString &function) const
{
    return m_functionArgs.value(function);
}



class DocumentContentCompletionProcessor final : public IAssistProcessor
{
public:
    DocumentContentCompletionProcessor(const QString &snippetGroupId,const Keywords& keywords,CodeFormatter* codeFormatter);
    ~DocumentContentCompletionProcessor() final;
    QList<AssistProposalItemInterface *> generateProposalList(const QStringList &words, const QIcon &icon);
    IAssistProposal *perform(const AssistInterface *interface) override;
    bool running() final { return m_watcher.isRunning(); }
    void cancel() final;
    inline CodeFormatter* codeFormatter(){return m_codeFormatter;}

private:
    QString m_snippetGroup;
    QFutureWatcher<QStringList> m_watcher;
    Keywords m_keywords;
    QIcon m_keywordIcon;
    QIcon m_classIcon;
    QIcon m_functionIcon;
    QIcon m_variantIcon;
    QIcon m_constantIcon;
    QIcon m_fieldIcon;
    CodeFormatter* m_codeFormatter;
};




DocumentContentCompletionProvider::DocumentContentCompletionProvider(const QString &snippetGroup)
    : m_snippetGroup(snippetGroup)
{
    d = new DocumentContentCompletionProviderPrivate;
    m_codeFormatter = nullptr;

}
DocumentContentCompletionProvider::~DocumentContentCompletionProvider(){
    delete d;
}

IAssistProvider::RunType DocumentContentCompletionProvider::runType() const
{
    return Asynchronous;
}

IAssistProcessor *DocumentContentCompletionProvider::createProcessor(const AssistInterface *) const
{
    //qDebug()<<"DocumentContentCompletionProvider::createProcessor";
    return new DocumentContentCompletionProcessor(m_snippetGroup,d->keywords,m_codeFormatter);
}


void DocumentContentCompletionProvider::setKeywordList(const QStringList& words){
    d->keywords.setKeywords(words);
}
void DocumentContentCompletionProvider::setFunctionList(const QStringList& words){
    d->keywords.setFunctions(words);
}
void DocumentContentCompletionProvider::setClassList(const QStringList& words){
    d->keywords.setClasses(words);
}

void DocumentContentCompletionProvider::setVariableList(const QStringList& words){
    d->keywords.setVariables(words);
}
void DocumentContentCompletionProvider::setOtherList(const QStringList& words){
    d->keywords.setOthers(words);
}



DocumentContentCompletionProcessor::DocumentContentCompletionProcessor(const QString &snippetGroupId,const Keywords& keywords,TextEditor::CodeFormatter* codeFormatter)
    : m_snippetGroup(snippetGroupId),m_keywords(keywords),m_codeFormatter(codeFormatter)
{
    m_keywordIcon = QIcon(":/resource/icons/IntelliSenseKeyword_16x.svg");
    m_classIcon = QIcon(":/resource/icons/Class_16x.svg");
    m_functionIcon = QIcon(":/resource/icons/Method_16x.svg");
    m_variantIcon = QIcon(":/resource/icons/LocalVariable_16x.svg");
    m_constantIcon = QIcon(":/resource/icons/Constant_16x.svg");
    m_fieldIcon = QIcon(":/resource/icons/Field_16x.svg");
}

DocumentContentCompletionProcessor::~DocumentContentCompletionProcessor()
{
    cancel();
}

QList<AssistProposalItemInterface *> DocumentContentCompletionProcessor::generateProposalList(const QStringList &words, const QIcon &icon)
{

    return Utils::transform(words, [this, &icon](const QString &word) -> AssistProposalItemInterface * {
        AssistProposalItem *item = new AssistProposalItem();
        item->setText(word);
        item->setIcon(icon);
        return item;
    });
}

static void createProposal(QFutureInterface<QStringList> &future,CodeFormatter* codeFormatter, const QString &text,
                           const QString &wordUnderCursor,const Keywords& keywords)
{
    qDebug()<<"createProposal"<<wordUnderCursor;
    QSet<QString> words;
    if(codeFormatter!=nullptr){
        QList<Code::Token> tokens = codeFormatter->tokenize(text);
        int wordUnderCursorFound = 0;
        for(auto tk:tokens){
            if(codeFormatter->isVariantKind(tk.kind)){
                const QString word = text.mid(tk.offset,tk.length);
                if (word == wordUnderCursor) {
                    // Only add the word under cursor if it
                    // already appears elsewhere in the text
                    if (++wordUnderCursorFound < 2)
                        continue;
                }

                if (!words.contains(word) && !keywords.contains(word))
                    words.insert(word);
            }
        }
    }
    //qDebug()<<"text"<<text;
    future.reportResult(Utils::toList(words));
}

IAssistProposal *DocumentContentCompletionProcessor::perform(const AssistInterface *interface)
{
    QScopedPointer<const AssistInterface> assistInterface(interface);
    if (running())
        return nullptr;

    int pos = interface->position();


    /*if(m_codeFormatter!=nullptr){
        if(m_codeFormatter->isInStringORCommentLiteral(block,pos - block.position())){
            return nullptr;
        }
    }*/

    QChar chr;
    // Skip to the start of a name
    if(m_codeFormatter!=nullptr){

        //QTextCursor startPositionCursor(interface->textDocument());
        //startPositionCursor.setPosition(pos);
        //QTextBlock block = startPositionCursor.block();
        QTextBlock block = interface->textDocument()->findBlock(pos);
        pos = m_codeFormatter->indentifierPosition(block,pos);
        qDebug()<<"ret:"<<pos<<interface->position();
        if(pos==-1){
            return nullptr;
        }
        /*do{
            chr = interface->characterAt(--pos);
        }while (m_codeFormatter->isIdentifier(chr));*/


    }else{
        do {
            chr = interface->characterAt(--pos);
        } while (chr.isLetterOrNumber() || chr == '_' );
        ++pos;
    }


    int length = interface->position() - pos;

    /*if (interface->reason() == IdleEditor) {
        QChar characterUnderCursor = interface->characterAt(interface->position());
        if (characterUnderCursor.isLetterOrNumber()
                || length < TextEditorSettings::completionSettings().m_characterThreshold) {
            return nullptr;
        }
    }*/





    //qDebug()<<"perform"<<pos<<length;


    const QString wordUnderCursor = interface->textAt(pos, length);
    const QString text = interface->textDocument()->toPlainText();



    //qDebug()<<"wordUnderCursor"<<wordUnderCursor;

    m_watcher.setFuture(Utils::runAsync(&createProposal,m_codeFormatter, text, wordUnderCursor,this->m_keywords));
    QObject::connect(&m_watcher, &QFutureWatcher<QStringList>::resultReadyAt,
                     &m_watcher, [this, pos](int index ){

        /*const TextEditor::SnippetAssistCollector snippetCollector(m_snippetGroup, QIcon(":/texteditor/images/snippet.png"));
        QList<AssistProposalItemInterface *> items = snippetCollector.collect();*/

        QList<AssistProposalItemInterface *> items;
        for (const QString &word : m_watcher.resultAt(index)) {
            auto item = new AssistProposalItem();
            item->setText(word);
            item->setIcon(this->m_variantIcon);
            items.append(item);
        }

        for(const QString &word:this->m_keywords.keywords()){
            auto item = new AssistProposalItem();
            item->setText(word);
            item->setIcon(this->m_keywordIcon);
            items.append(item);
        }
        for(const QString &word:this->m_keywords.variables()){
            auto item = new AssistProposalItem();
            item->setText(word);
            item->setIcon(this->m_constantIcon);
            items.append(item);
        }
        for(const QString &word:this->m_keywords.functions()){
            auto item = new AssistProposalItem();
            item->setText(word);
            item->setIcon(this->m_functionIcon);
            items.append(item);
        }
        for(const QString &word:this->m_keywords.classes()){
            auto item = new AssistProposalItem();
            item->setText(word);
            item->setIcon(this->m_classIcon);
            items.append(item);
        }
        for(const QString &word:this->m_keywords.others()){
            auto item = new AssistProposalItem();
            item->setText(word);
            item->setIcon(this->m_variantIcon);
            items.append(item);
        }

        setAsyncProposalAvailable(new GenericProposal(pos, items));
    });
    return nullptr;
}

void DocumentContentCompletionProcessor::cancel()
{
    if (running())
        m_watcher.cancel();
}
