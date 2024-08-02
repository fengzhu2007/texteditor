// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "circularclipboardassist.h"
#include "codeassist/assistinterface.h"
#include "codeassist/iassistprocessor.h"
#include "codeassist/iassistproposal.h"
#include "codeassist/assistproposalitem.h"
#include "codeassist/genericproposalmodel.h"
#include "codeassist/genericproposal.h"
#include "texteditor.h"
#include "circularclipboard.h"

#include "core/coreconstants.h"

//#include "utils/utilsicons.h"

#include <QApplication>
#include <QClipboard>

namespace TextEditor {
namespace Internal {

class ClipboardProposalItem: public AssistProposalItem
{
public:
    enum { maxLen = 80 };

    ClipboardProposalItem(QSharedPointer<const QMimeData> mimeData)
        : m_mimeData(mimeData)
    {
        QString text = mimeData->text().simplified();
        if (text.length() > maxLen) {
            text.truncate(maxLen);
            text.append(QLatin1String("..."));
        }
        setText(text);
    }

    ~ClipboardProposalItem() noexcept override = default;

    void apply(TextDocumentManipulatorInterface &manipulator, int /*basePosition*/) const override
    {

        //Move to last in circular clipboard
        if (CircularClipboard * clipboard = CircularClipboard::instance()) {
            clipboard->collect(m_mimeData);
            clipboard->toLastCollect();
        }

        //Copy the selected item
        QApplication::clipboard()->setMimeData(
                    TextEditorWidget::duplicateMimeData(m_mimeData.data()));

        //Paste
        manipulator.paste();
    }

private:
    QSharedPointer<const QMimeData> m_mimeData;
};

class ClipboardAssistProcessor: public IAssistProcessor
{
public:
    IAssistProposal *perform(const AssistInterface *interface) override
    {
        if (!interface)
            return nullptr;
        const QScopedPointer<const AssistInterface> AssistInterface(interface);

        QIcon icon ;
        CircularClipboard * clipboard = CircularClipboard::instance();
        QList<AssistProposalItemInterface *> items;
        items.reserve(clipboard->size());
        for (int i = 0; i < clipboard->size(); ++i) {
            QSharedPointer<const QMimeData> data = clipboard->next();

            AssistProposalItem *item = new ClipboardProposalItem(data);
            item->setIcon(icon);
            item->setOrder(clipboard->size() - 1 - i);
            items.append(item);
        }

        return new GenericProposal(interface->position(), items);
    }
};

IAssistProvider::RunType ClipboardAssistProvider::runType() const
{
    return Synchronous;
}

IAssistProcessor *ClipboardAssistProvider::createProcessor(const AssistInterface *) const
{
    return new ClipboardAssistProcessor;
}

} // namespace Internal
} // namespace TextEditor
