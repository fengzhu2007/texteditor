// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "basefilefind.h"

namespace TextEditor {
namespace Internal {

class FindInOpenFiles : public BaseFileFind
{
    Q_OBJECT

public:
    FindInOpenFiles();

    QString id() const override;
    QString displayName() const override;
    bool isEnabled() const override;
    void writeSettings(QSettings *settings) override;
    void readSettings(QSettings *settings) override;

protected:
    Utils::FileIterator *files(const QStringList &nameFilters,
                               const QStringList &exclusionFilters,
                               const QVariant &additionalParameters) const override;
    QVariant additionalParameters() const override;
    QString label() const override;
    QString toolTip() const override;

private:
    void updateEnabledState();
};

} // namespace Internal
} // namespace TextEditor
