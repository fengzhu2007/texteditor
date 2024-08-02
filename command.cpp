// Copyright (C) 2016 Lorenz Haas
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "command.h"

namespace TextEditor {

bool Command::isValid() const
{
    return !m_executable.isEmpty();
}

QString Command::executable() const
{
    return m_executable;
}

void Command::setExecutable(const QString &executable)
{
    m_executable = executable;
}

QStringList Command::options() const
{
    return m_options;
}

void Command::addOption(const QString &option)
{
    m_options << option;
}

Command::Processing Command::processing() const
{
    return m_processing;
}

void Command::setProcessing(const Processing &processing)
{
    m_processing = processing;
}

bool Command::pipeAddsNewline() const
{
    return m_pipeAddsNewline;
}

void Command::setPipeAddsNewline(bool pipeAddsNewline)
{
    m_pipeAddsNewline = pipeAddsNewline;
}

bool Command::returnsCRLF() const
{
    return m_returnsCRLF;
}

void Command::setReturnsCRLF(bool returnsCRLF)
{
    m_returnsCRLF = returnsCRLF;
}

} // namespace TextEditor
