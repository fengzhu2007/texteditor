// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "textdocumentmanipulatorinterface.h"

QT_BEGIN_NAMESPACE
class QIcon;
class QString;
QT_END_NAMESPACE

#include <QString>

namespace TextEditor {

class TEXTEDITOR_EXPORT AssistProposalItemInterface
{
public:
    // We compare proposals by enum values, be careful changing their values
    enum class ProposalMatch
    {
        Full = 0,
        Exact = 1,
        Prefix = 2,
        Infix = 3,
        None = 4
    };

    AssistProposalItemInterface() = default;
    virtual ~AssistProposalItemInterface() noexcept = default;

    Q_DISABLE_COPY_MOVE(AssistProposalItemInterface)

    virtual QString text() const = 0;
    virtual QString filterText() const { return text(); }
    virtual bool implicitlyApplies() const = 0;
    virtual bool prematurelyApplies(const QChar &typedCharacter) const = 0;
    virtual void apply(TextDocumentManipulatorInterface &manipulator, int basePosition) const = 0;
    virtual QIcon icon() const = 0;
    virtual QString detail() const = 0;
    virtual bool isKeyword() const { return false; }
    virtual Qt::TextFormat detailFormat() const { return Qt::AutoText; }
    virtual bool isSnippet() const = 0;
    virtual bool isValid() const = 0;
    virtual quint64 hash() const = 0; // it is only for removing duplicates
    virtual bool requiresFixIts() const { return false; }

    inline int order() const { return m_order; }
    inline void setOrder(int order) { m_order = order; }
    inline ProposalMatch proposalMatch() { return m_proposalMatch; }
    inline void setProposalMatch(ProposalMatch match) { m_proposalMatch = match; }

private:
    int m_order = 0;
    ProposalMatch m_proposalMatch = ProposalMatch::None;
};

} // namespace TextEditor
