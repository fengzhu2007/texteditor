// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "qtcassert.h"

#include <QByteArray>
#include <QDebug>
#include <QMutex>
#include <QTime>

#if defined(Q_OS_UNIX)
#include <stdio.h>
#include <signal.h>
#include <execinfo.h>
#elif defined(_MSC_VER)
#ifdef QTCREATOR_PCH_H
#define CALLBACK WINAPI
#define OUT
#define IN
#endif
#include <Windows.h>
#include <DbgHelp.h>
#endif

namespace Utils {

void dumpBacktrace(int maxdepth)
{

}

void writeAssertLocation(const char *msg)
{
    const QByteArray time = QTime::currentTime().toString(Qt::ISODateWithMs).toLatin1();
    static bool goBoom = qEnvironmentVariableIsSet("QTC_FATAL_ASSERTS");
    if (goBoom)
        qFatal("SOFT ASSERT [%s] made fatal: %s", time.data(), msg);
    else
        qDebug("SOFT ASSERT [%s]: %s", time.data(), msg);

    static int maxdepth = qEnvironmentVariableIntValue("QTC_BACKTRACE_MAXDEPTH");
    if (maxdepth != 0)
        dumpBacktrace(maxdepth);
}

} // namespace Utils
