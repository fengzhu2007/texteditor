// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.h"

//#include <coreplugin/helpitem.h>
#include "core/icontext.h"

#include <functional>
#include <QList>
#include <QTextCursor>
#include <QObject>

QT_BEGIN_NAMESPACE
class QPoint;
QT_END_NAMESPACE

namespace TextEditor {

class TextEditorWidget;

class TEXTEDITOR_EXPORT BaseHoverHandler
{
public:
    virtual ~BaseHoverHandler();

    /*void contextHelpId(TextEditorWidget *widget,
                       int pos,
                       const Core::IContext::HelpCallback &callback);*/

    using ReportPriority = std::function<void(int priority)>;
    void checkPriority(TextEditorWidget *widget, int pos, ReportPriority report);
    virtual void abort() {} // Implement for asynchronous priority reporter

    void showToolTip(TextEditorWidget *widget, const QPoint &point);

protected:
    enum {
        Priority_None = 0,
        Priority_Tooltip = 5,
        Priority_Help = 10,
        Priority_Diagnostic = 20
    };
    void setPriority(int priority);
    int priority() const;

    void setToolTip(const QString &tooltip, Qt::TextFormat format = Qt::PlainText);
    const QString &toolTip() const;

    //void setLastHelpItemIdentified(const Core::HelpItem &help);
    //const Core::HelpItem &lastHelpItemIdentified() const;

    bool isContextHelpRequest() const;

    //void propagateHelpId(TextEditorWidget *widget, const Core::IContext::HelpCallback &callback);

    // identifyMatch() is required to report a priority by using the "report" callback.
    // It is recommended to use e.g.
    //    Utils::ExecuteOnDestruction reportPriority([this, report](){ report(priority()); });
    // at the beginning of an implementation to ensure this in any case.
    virtual void identifyMatch(TextEditorWidget *editorWidget, int pos, ReportPriority report);
    virtual void operateTooltip(TextEditorWidget *editorWidget, const QPoint &point);

private:
    void process(TextEditorWidget *widget, int pos, ReportPriority report);

    QString m_toolTip;
    Qt::TextFormat m_textFormat = Qt::PlainText;
    //Core::HelpItem m_lastHelpItemIdentified;
    int m_priority = -1;
    bool m_isContextHelpRequest = false;
};

class TEXTEDITOR_EXPORT HoverHandlerRunner : public QObject
{
public:
    using Callback = std::function<void(TextEditorWidget *, BaseHoverHandler *, int)>;

    HoverHandlerRunner(QObject *parent, QList<BaseHoverHandler *> &handlers)
        : QObject(parent), m_handlers(handlers) {}

    void startChecking(TextEditorWidget *widget, const QTextCursor &cursor, const Callback &callback)
    {
        abortHandlers();
        const int pos = cursor.position();
        BaseHoverHandler *bestHandler = nullptr;
        int bestPriority = 0; // Priority_None

        for (BaseHoverHandler *handler : m_handlers) {
            handler->checkPriority(
                widget, pos,
                [handler, &bestHandler, &bestPriority](int priority) {
                    if (priority > bestPriority) {
                        bestPriority = priority;
                        bestHandler = handler;
                    }
                });
        }

        if (bestHandler)
            callback(widget, bestHandler, pos);
    }

    void abortHandlers()
    {
        for (BaseHoverHandler *handler : m_handlers)
            handler->abort();
    }

private:
    QList<BaseHoverHandler *> &m_handlers;
};

} // namespace TextEditor
