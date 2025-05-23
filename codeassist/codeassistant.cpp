// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "codeassistant.h"
#include "completionassistprovider.h"
#include "iassistprocessor.h"
#include "iassistproposal.h"
#include "iassistproposalmodel.h"
#include "iassistproposalwidget.h"
#include "assistinterface.h"
#include "assistproposalitem.h"
#include "runner.h"
#include "textdocumentmanipulator.h"

#include "textdocument.h"
#include "texteditor.h"
#include "texteditorsettings.h"
#include "completionsettings.h"
//#include <extensionsystem/pluginmanager.h"
#include "utils/algorithm.h"
#include "utils/executeondestruction.h"
#include "utils/qtcassert.h"

#include <QKeyEvent>
#include <QList>
#include <QObject>
#include <QScopedPointer>
#include <QTimer>

using namespace TextEditor::Internal;

namespace TextEditor {

class CodeAssistantPrivate : public QObject
{
public:
    CodeAssistantPrivate(CodeAssistant *assistant);

    void configure(TextEditorWidget *editorWidget);
    bool isConfigured() const;

    void invoke(AssistKind kind, IAssistProvider *provider = nullptr);
    void process();
    void requestProposal(AssistReason reason,
                         AssistKind kind,
                         IAssistProvider *provider = nullptr,
                         bool isUpdate = false);
    void cancelCurrentRequest();
    void invalidateCurrentRequestData();
    void displayProposal(IAssistProposal *newProposal, AssistReason reason);
    bool isDisplayingProposal() const;
    bool isWaitingForProposal() const;

    void notifyChange();
    bool hasContext() const;
    void destroyContext();

    QVariant userData() const;
    void setUserData(const QVariant &data);

    CompletionAssistProvider *identifyActivationSequence();

    void stopAutomaticProposalTimer();
    void startAutomaticProposalTimer();
    void automaticProposalTimeout();
    void clearAbortedPosition();
    void updateFromCompletionSettings(const TextEditor::CompletionSettings &settings);

    bool eventFilter(QObject *o, QEvent *e) override;

private:
    bool requestActivationCharProposal();
    void processProposalItem(AssistProposalItemInterface *proposalItem);
    void handlePrefixExpansion(const QString &newPrefix);
    void finalizeProposal();
    void explicitlyAborted();
    bool isDestroyEvent(int key, const QString &keyText);

private:
    CodeAssistant *q = nullptr;
    TextEditorWidget *m_editorWidget = nullptr;
    Internal::ProcessorRunner *m_requestRunner = nullptr;
    QMetaObject::Connection m_runnerConnection;
    IAssistProvider *m_requestProvider = nullptr;
    IAssistProcessor *m_asyncProcessor = nullptr;
    AssistKind m_assistKind = TextEditor::Completion;
    IAssistProposalWidget *m_proposalWidget = nullptr;
    TextEditorWidget::SuggestionBlocker m_suggestionBlocker;
    QScopedPointer<IAssistProposal> m_proposal;
    bool m_receivedContentWhileWaiting = false;
    QTimer m_automaticProposalTimer;
    CompletionSettings m_settings;
    int m_abortedBasePosition = -1;
    static const QChar m_null;
    QVariant m_userData;
};

// --------------------
// CodeAssistantPrivate
// --------------------
const QChar CodeAssistantPrivate::m_null;

CodeAssistantPrivate::CodeAssistantPrivate(CodeAssistant *assistant)
    : q(assistant)
{
    m_automaticProposalTimer.setSingleShot(true);
    connect(&m_automaticProposalTimer, &QTimer::timeout,
            this, &CodeAssistantPrivate::automaticProposalTimeout);

    updateFromCompletionSettings(TextEditorSettings::completionSettings());
    connect(TextEditorSettings::instance(), &TextEditorSettings::completionSettingsChanged,
            this, &CodeAssistantPrivate::updateFromCompletionSettings);

    //connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
    //        this, &CodeAssistantPrivate::clearAbortedPosition);
}

void CodeAssistantPrivate::configure(TextEditorWidget *editorWidget)
{
    m_editorWidget = editorWidget;
    m_editorWidget->installEventFilter(this);
}

bool CodeAssistantPrivate::isConfigured() const
{
    return m_editorWidget != nullptr;
}

void CodeAssistantPrivate::invoke(AssistKind kind, IAssistProvider *provider)
{
    if (!isConfigured())
        return;

    stopAutomaticProposalTimer();

    if (isDisplayingProposal() && m_assistKind == kind && !m_proposal->isFragile()
        && m_proposal->supportsPrefix()) {
        m_proposalWidget->setReason(ExplicitlyInvoked);
        m_proposalWidget->updateProposal(m_editorWidget->textAt(
                        m_proposal->basePosition(),
                        m_editorWidget->position() - m_proposal->basePosition()));
    } else {
        requestProposal(ExplicitlyInvoked, kind, provider);
    }
}

bool CodeAssistantPrivate::requestActivationCharProposal()
{
    if (m_editorWidget->multiTextCursor().hasMultipleCursors())
        return false;
    if (m_assistKind == Completion && m_settings.m_completionTrigger != ManualCompletion) {
        if (CompletionAssistProvider *provider = identifyActivationSequence()) {
            requestProposal(ActivationCharacter, Completion, provider);
            return true;
        }
    }
    return false;
}

void CodeAssistantPrivate::process()
{
    if (!isConfigured())
        return;
    stopAutomaticProposalTimer();
    if (m_assistKind == TextEditor::Completion) {
        if (!requestActivationCharProposal()){
            startAutomaticProposalTimer();
        }

    } else if (m_assistKind != FunctionHint){
        m_assistKind = TextEditor::Completion;
    }
}

void CodeAssistantPrivate::requestProposal(AssistReason reason,
                                           AssistKind kind,
                                           IAssistProvider *provider,
                                           bool isUpdate)
{
    // make sure to cleanup old proposals if we cannot find a new assistant
    Utils::ExecuteOnDestruction earlyReturnContextClear([this] { destroyContext(); });
    if (isWaitingForProposal())
        cancelCurrentRequest();
    if (!provider) {
        if (kind == Completion)
            provider = m_editorWidget->textDocument()->completionAssistProvider();
        else if (kind == FunctionHint)
            provider = m_editorWidget->textDocument()->functionHintAssistProvider();
        else
            provider = m_editorWidget->textDocument()->quickFixAssistProvider();

        if (!provider)
            return;
    }
    AssistInterface *assistInterface = m_editorWidget->createAssistInterface(kind, reason);
    if (!assistInterface)
        return;
    // We got an assist provider and interface so no need to reset the current context anymore
    earlyReturnContextClear.reset({});

    m_assistKind = kind;
    m_requestProvider = provider;
    IAssistProcessor *processor = provider->createProcessor(assistInterface);
    switch (provider->runType()) {
    case IAssistProvider::Synchronous: {
        if (IAssistProposal *newProposal = processor->perform(assistInterface))
            displayProposal(newProposal, reason);
        delete processor;
        break;
    }
    case IAssistProvider::AsynchronousWithThread: {
        if (IAssistProposal *newProposal = processor->immediateProposal(assistInterface))
            displayProposal(newProposal, reason);

        m_requestRunner = new ProcessorRunner;
        m_runnerConnection = connect(m_requestRunner, &ProcessorRunner::finished,
                                     this, [this, reason, sender = m_requestRunner] {
            // Since the request runner is a different thread, there's still a gap in which the
            // queued signal could be processed after an invalidation of the current request.
            if (!m_requestRunner || m_requestRunner != sender)
                return;

            IAssistProposal *proposal = m_requestRunner->proposal();
            invalidateCurrentRequestData();
            displayProposal(proposal, reason);
            emit q->finished();
        });
        connect(m_requestRunner, &ProcessorRunner::finished,
                m_requestRunner, &ProcessorRunner::deleteLater);
        assistInterface->prepareForAsyncUse();
        m_requestRunner->setProcessor(processor);
        m_requestRunner->setAssistInterface(assistInterface);
        m_requestRunner->start();
        break;
    }
    case IAssistProvider::Asynchronous: {
        processor->setAsyncCompletionAvailableHandler([this, reason, processor](
                IAssistProposal *newProposal) {
            if (!processor->running()) {
                // do not delete this processor directly since this function is called from within the processor
                QMetaObject::invokeMethod(QCoreApplication::instance(), [processor] {
                    delete processor;
                }, Qt::QueuedConnection);
            }
            if (processor != m_asyncProcessor)
                return;

            invalidateCurrentRequestData();
            if (processor->needsRestart() && m_receivedContentWhileWaiting) {
                delete newProposal;
                m_receivedContentWhileWaiting = false;
                requestProposal(reason, m_assistKind, m_requestProvider);
            } else {
                displayProposal(newProposal, reason);
                if (processor->running()){
                    m_asyncProcessor = processor;
                }else{
                    emit q->finished();
                }
            }
        });

        // If there is a proposal, nothing asynchronous happened...
        if (IAssistProposal *newProposal = processor->perform(assistInterface)) {
            displayProposal(newProposal, reason);
            delete processor;
        } else if (!processor->running()) {
            if (isUpdate)
                destroyContext();
            delete processor;
        } else { // ...async request was triggered
            if (IAssistProposal *newProposal = processor->immediateProposal(assistInterface))
                displayProposal(newProposal, reason);
            QTC_CHECK(!m_asyncProcessor);
            m_asyncProcessor = processor;
        }

        break;
    }
    } // switch
}

void CodeAssistantPrivate::cancelCurrentRequest()
{
    if (m_requestRunner) {
        m_requestRunner->setDiscardProposal(true);
        disconnect(m_runnerConnection);
    }
    if (m_asyncProcessor) {
        m_asyncProcessor->cancel();
        delete m_asyncProcessor;
    }
    invalidateCurrentRequestData();
}

void CodeAssistantPrivate::displayProposal(IAssistProposal *newProposal, AssistReason reason)
{
    if (!newProposal)
        return;

    // TODO: The proposal should own the model until someone takes it explicitly away.
    QScopedPointer<IAssistProposal> proposalCandidate(newProposal);

    if (isDisplayingProposal() && !m_proposal->isFragile()
        && !m_proposalWidget->supportsModelUpdate(proposalCandidate->id())) {
        return;
    }
    int basePosition = proposalCandidate->basePosition();
    if (m_editorWidget->position() < basePosition) {
        destroyContext();
        return;
    }
    if (m_abortedBasePosition == basePosition && reason != ExplicitlyInvoked) {
        destroyContext();
        return;
    }

    if (m_editorWidget->suggestionVisible()) {
        if (reason != ExplicitlyInvoked) {
            destroyContext();
            return;
        }
        m_editorWidget->clearSuggestion();
    }


    const QString prefix = m_editorWidget->textAt(basePosition,
                                                  m_editorWidget->position() - basePosition);
    if (!newProposal->hasItemsToPropose(prefix, reason)) {
        if (newProposal->isCorrective(m_editorWidget))
            newProposal->makeCorrection(m_editorWidget);
        destroyContext();
        return;
    }
    if (m_proposalWidget
        && basePosition == proposalCandidate->basePosition()
        && m_proposalWidget->supportsModelUpdate(proposalCandidate->id())) {
        m_proposal.reset(proposalCandidate.take());
        m_proposal->setReason(reason);
        m_proposalWidget->updateModel(m_proposal->model());
        m_proposalWidget->updateProposal(prefix);
        return;
    }
    destroyContext();

    clearAbortedPosition();
    m_proposal.reset(proposalCandidate.take());
    m_proposal->setReason(reason);

    if (m_proposal->isCorrective(m_editorWidget))
        m_proposal->makeCorrection(m_editorWidget);

    m_editorWidget->keepAutoCompletionHighlight(true);
    basePosition = m_proposal->basePosition();
    m_proposalWidget = m_proposal->createWidget();
    connect(m_proposalWidget, &QObject::destroyed,
            this, &CodeAssistantPrivate::finalizeProposal);
    connect(m_proposalWidget, &IAssistProposalWidget::prefixExpanded,
            this, &CodeAssistantPrivate::handlePrefixExpansion);
    connect(m_proposalWidget, &IAssistProposalWidget::proposalItemActivated,
            this, &CodeAssistantPrivate::processProposalItem);
    connect(m_proposalWidget, &IAssistProposalWidget::explicitlyAborted,
            this, &CodeAssistantPrivate::explicitlyAborted);
    m_proposalWidget->setAssistant(q);
    m_proposalWidget->setReason(reason);
    m_proposalWidget->setKind(m_assistKind);
    m_proposalWidget->setBasePosition(basePosition);
    m_proposalWidget->setUnderlyingWidget(m_editorWidget);
    m_proposalWidget->setModel(m_proposal->model());
    m_proposalWidget->setDisplayRect(m_editorWidget->cursorRect(basePosition));
    m_proposalWidget->setIsSynchronized(!m_receivedContentWhileWaiting);
    m_proposalWidget->showProposal(prefix);
}

void CodeAssistantPrivate::processProposalItem(AssistProposalItemInterface *proposalItem)
{
    QTC_ASSERT(m_proposal, return);
    TextDocumentManipulator manipulator(m_editorWidget);
    proposalItem->apply(manipulator, m_proposal->basePosition());
    destroyContext();
    m_editorWidget->encourageApply();
    if (!proposalItem->isSnippet())
        requestActivationCharProposal();
}

void CodeAssistantPrivate::handlePrefixExpansion(const QString &newPrefix)
{
    QTC_ASSERT(m_proposal, return);

    QTextCursor cursor(m_editorWidget->document());
    cursor.setPosition(m_proposal->basePosition());
    cursor.movePosition(QTextCursor::EndOfWord);

    int currentPosition = m_editorWidget->position();
    const QString textAfterCursor = m_editorWidget->textAt(currentPosition,
                                                       cursor.position() - currentPosition);
    if (!textAfterCursor.startsWith(newPrefix)) {
        if (newPrefix.indexOf(textAfterCursor, currentPosition - m_proposal->basePosition()) >= 0)
            currentPosition = cursor.position();
        const QStringView prefixAddition = QStringView(newPrefix).mid(currentPosition
                                                                      - m_proposal->basePosition());
        // If remaining string starts with the prefix addition
        if (textAfterCursor.startsWith(prefixAddition))
            currentPosition += prefixAddition.size();
    }

    m_editorWidget->setCursorPosition(m_proposal->basePosition());
    m_editorWidget->replace(currentPosition - m_proposal->basePosition(), newPrefix);
    notifyChange();
}

void CodeAssistantPrivate::finalizeProposal()
{
    stopAutomaticProposalTimer();
    m_proposal.reset();
    m_proposalWidget = nullptr;
    if (m_receivedContentWhileWaiting)
        m_receivedContentWhileWaiting = false;
}

bool CodeAssistantPrivate::isDisplayingProposal() const
{
    return m_proposalWidget != nullptr && m_proposalWidget->proposalIsVisible();
}

bool CodeAssistantPrivate::isWaitingForProposal() const
{
    return m_requestRunner != nullptr || m_asyncProcessor != nullptr;
}

void CodeAssistantPrivate::invalidateCurrentRequestData()
{
    m_asyncProcessor = nullptr;
    m_requestRunner = nullptr;
    m_requestProvider = nullptr;
    m_receivedContentWhileWaiting = false;
}

CompletionAssistProvider *CodeAssistantPrivate::identifyActivationSequence()
{
    auto checkActivationSequence = [this](CompletionAssistProvider *provider) {

        if (!provider)
            return false;
        const int length = provider->activationCharSequenceLength();
        if (!length)
            return false;
        QString sequence = m_editorWidget->textAt(m_editorWidget->position() - length, length);

        // In pretty much all cases the sequence will have the appropriate length. Only in the
        // case of typing the very first characters in the document for providers that request a
        // length greater than 1 (currently only C++, which specifies 3), the sequence needs to
        // be prepended so it has the expected length.
        const int lengthDiff = length - sequence.length();
        for (int j = 0; j < lengthDiff; ++j)
            sequence.prepend(m_null);
        return provider->isActivationCharSequence(sequence);
    };

    auto provider = {
        m_editorWidget->textDocument()->completionAssistProvider(),
        m_editorWidget->textDocument()->functionHintAssistProvider()
    };
    return Utils::findOrDefault(provider, checkActivationSequence);
}

void CodeAssistantPrivate::notifyChange()
{
    stopAutomaticProposalTimer();

    if (isDisplayingProposal()) {
        QTC_ASSERT(m_proposal, return);
        if (m_editorWidget->position() < m_proposal->basePosition()) {
            destroyContext();
        } else if (m_proposal->supportsPrefix()) {
            m_proposalWidget->updateProposal(
                m_editorWidget->textAt(m_proposal->basePosition(),
                                     m_editorWidget->position() - m_proposal->basePosition()));
            if (!isDisplayingProposal())
                requestActivationCharProposal();
        } else {
            requestProposal(m_proposal->reason(), m_assistKind, m_requestProvider, true);
        }
    }
}

bool CodeAssistantPrivate::hasContext() const
{
    return m_requestRunner || m_asyncProcessor || m_proposalWidget;
}

void CodeAssistantPrivate::destroyContext()
{
    stopAutomaticProposalTimer();

    if (isWaitingForProposal()) {
        cancelCurrentRequest();
    } else if (m_proposalWidget) {
        m_editorWidget->keepAutoCompletionHighlight(false);
        if (m_proposalWidget->proposalIsVisible())
            m_proposalWidget->closeProposal();
        disconnect(m_proposalWidget, &QObject::destroyed,
                   this, &CodeAssistantPrivate::finalizeProposal);
        finalizeProposal();
    }
}

QVariant CodeAssistantPrivate::userData() const
{
    return m_userData;
}

void CodeAssistantPrivate::setUserData(const QVariant &data)
{
    m_userData = data;
}

void CodeAssistantPrivate::startAutomaticProposalTimer()
{
    if (m_settings.m_completionTrigger == AutomaticCompletion)
        m_automaticProposalTimer.start();
}

void CodeAssistantPrivate::automaticProposalTimeout()
{
    if (isWaitingForProposal()
        || m_editorWidget->multiTextCursor().hasMultipleCursors()
        || (isDisplayingProposal() && !m_proposal->isFragile())) {
        return;
    }
    requestProposal(IdleEditor, Completion);
}

void CodeAssistantPrivate::stopAutomaticProposalTimer()
{
    if (m_automaticProposalTimer.isActive())
        m_automaticProposalTimer.stop();
}

void CodeAssistantPrivate::updateFromCompletionSettings(
        const TextEditor::CompletionSettings &settings)
{
    m_settings = settings;
    m_automaticProposalTimer.setInterval(m_settings.m_automaticProposalTimeoutInMs);
}

void CodeAssistantPrivate::explicitlyAborted()
{
    QTC_ASSERT(m_proposal, return);
    //m_abortedBasePosition = m_proposal->basePosition();
}

void CodeAssistantPrivate::clearAbortedPosition()
{
    m_abortedBasePosition = -1;
}

bool CodeAssistantPrivate::isDestroyEvent(int key, const QString &keyText)
{
    if (keyText.isEmpty())
        return key != Qt::LeftArrow && key != Qt::RightArrow && key != Qt::Key_Shift;
    if (auto provider = qobject_cast<CompletionAssistProvider *>(m_requestProvider))
        return !provider->isContinuationChar(keyText.at(0));
    return false;
}

bool CodeAssistantPrivate::eventFilter(QObject *o, QEvent *e)
{
    Q_UNUSED(o)
    if (isWaitingForProposal()) {
        QEvent::Type type = e->type();
        if (type == QEvent::FocusOut) {
            destroyContext();
        } else if (type == QEvent::KeyPress) {
            auto keyEvent = static_cast<QKeyEvent *>(e);
            const QString &keyText = keyEvent->text();

            if (isDestroyEvent(keyEvent->key(), keyText))
                destroyContext();
            else if (!keyText.isEmpty() && !m_receivedContentWhileWaiting)
                m_receivedContentWhileWaiting = true;
        } else if (type == QEvent::KeyRelease
                   && static_cast<QKeyEvent *>(e)->key() == Qt::Key_Escape) {
            destroyContext();
        }
    }

    return false;
}

// -------------
// CodeAssistant
// -------------
CodeAssistant::CodeAssistant() : d(new CodeAssistantPrivate(this))
{
}

CodeAssistant::~CodeAssistant()
{
    destroyContext();
    delete d;
}

void CodeAssistant::configure(TextEditorWidget *editorWidget)
{
    d->configure(editorWidget);
}

void CodeAssistant::process()
{
    d->process();
}

void CodeAssistant::notifyChange()
{
    d->notifyChange();
}

bool CodeAssistant::hasContext() const
{
    return d->hasContext();
}

void CodeAssistant::destroyContext()
{
    d->destroyContext();
}

QVariant CodeAssistant::userData() const
{
    return d->userData();
}

void CodeAssistant::setUserData(const QVariant &data)
{
    d->setUserData(data);
}

void CodeAssistant::invoke(AssistKind kind, IAssistProvider *provider)
{
    d->invoke(kind, provider);
}

} // namespace TextEditor
