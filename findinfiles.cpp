// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "findinfiles.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/find/findplugin.h>
#include <coreplugin/icore.h>

#include <utils/filesearch.h>
#include <utils/fileutils.h>
#include <utils/historycompleter.h>
#include <utils/pathchooser.h>
#include <utils/qtcassert.h>
#include <utils/stringutils.h>

#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>

using namespace Core;
using namespace TextEditor;
using namespace Utils;

static FindInFiles *m_instance = nullptr;
static const char HistoryKey[] = "FindInFiles.Directories.History";

FindInFiles::FindInFiles()
{
    m_instance = this;
    connect(EditorManager::instance(), &EditorManager::findOnFileSystemRequest,
            this, &FindInFiles::findOnFileSystem);
}

FindInFiles::~FindInFiles() = default;

bool FindInFiles::isValid() const
{
    return m_isValid;
}

QString FindInFiles::id() const
{
    return QLatin1String("Files on Disk");
}

QString FindInFiles::displayName() const
{
    return tr("Files in File System");
}

FileIterator *FindInFiles::files(const QStringList &nameFilters,
                                 const QStringList &exclusionFilters,
                                 const QVariant &additionalParameters) const
{
    return new SubDirFileIterator({FilePath::fromVariant(additionalParameters)},
                                  nameFilters,
                                  exclusionFilters,
                                  EditorManager::defaultTextCodec());
}

QVariant FindInFiles::additionalParameters() const
{
    return path().toVariant();
}

QString FindInFiles::label() const
{
    QString title = currentSearchEngine()->title();

    const QChar slash = QLatin1Char('/');
    const QStringList &nonEmptyComponents = path().toFileInfo().absoluteFilePath()
            .split(slash, Qt::SkipEmptyParts);
    return tr("%1 \"%2\":")
            .arg(title)
            .arg(nonEmptyComponents.isEmpty() ? QString(slash) : nonEmptyComponents.last());
}

QString FindInFiles::toolTip() const
{
    //: the last arg is filled by BaseFileFind::runNewSearch
    QString tooltip = tr("Path: %1\nFilter: %2\nExcluding: %3\n%4")
            .arg(path().toUserOutput())
            .arg(fileNameFilters().join(','))
            .arg(fileExclusionFilters().join(','));

    const QString searchEngineToolTip = currentSearchEngine()->toolTip();
    if (!searchEngineToolTip.isEmpty())
        tooltip = tooltip.arg(searchEngineToolTip);

    return tooltip;
}

void FindInFiles::syncSearchEngineCombo(int selectedSearchEngineIndex)
{
    QTC_ASSERT(m_searchEngineCombo && selectedSearchEngineIndex >= 0
               && selectedSearchEngineIndex < searchEngines().size(), return);

    m_searchEngineCombo->setCurrentIndex(selectedSearchEngineIndex);
}

void FindInFiles::setValid(bool valid)
{
    if (valid == m_isValid)
        return;
    m_isValid = valid;
    emit validChanged(m_isValid);
}

void FindInFiles::searchEnginesSelectionChanged(int index)
{
    setCurrentSearchEngine(index);
    m_searchEngineWidget->setCurrentIndex(index);
}

QWidget *FindInFiles::createConfigWidget()
{
    if (!m_configWidget) {
        m_configWidget = new QWidget;
        auto gridLayout = new QGridLayout(m_configWidget);
        gridLayout->setContentsMargins(0, 0, 0, 0);
        m_configWidget->setLayout(gridLayout);

        int row = 0;
        auto searchEngineLabel = new QLabel(tr("Search engine:"));
        gridLayout->addWidget(searchEngineLabel, row, 0, Qt::AlignRight);
        m_searchEngineCombo = new QComboBox;
        connect(m_searchEngineCombo, &QComboBox::currentIndexChanged,
                this, &FindInFiles::searchEnginesSelectionChanged);
        searchEngineLabel->setBuddy(m_searchEngineCombo);
        gridLayout->addWidget(m_searchEngineCombo, row, 1);

        m_searchEngineWidget = new QStackedWidget(m_configWidget);
        const QVector<SearchEngine *> searchEngineVector = searchEngines();
        for (const SearchEngine *searchEngine : searchEngineVector) {
            m_searchEngineWidget->addWidget(searchEngine->widget());
            m_searchEngineCombo->addItem(searchEngine->title());
        }
        gridLayout->addWidget(m_searchEngineWidget, row++, 2);

        QLabel *dirLabel = new QLabel(tr("Director&y:"));
        gridLayout->addWidget(dirLabel, row, 0, Qt::AlignRight);
        m_directory = new PathChooser;
        m_directory->setExpectedKind(PathChooser::ExistingDirectory);
        m_directory->setPromptDialogTitle(tr("Directory to Search"));
        connect(m_directory.data(), &PathChooser::textChanged, this,
                [this] { pathChanged(m_directory->filePath()); });
        m_directory->setHistoryCompleter(QLatin1String(HistoryKey),
                                         /*restoreLastItemFromHistory=*/ true);
        if (!HistoryCompleter::historyExistsFor(QLatin1String(HistoryKey))) {
            auto completer = static_cast<HistoryCompleter *>(m_directory->lineEdit()->completer());
            const QStringList legacyHistory = Core::ICore::settings()->value(
                        QLatin1String("Find/FindInFiles/directories")).toStringList();
            for (const QString &dir: legacyHistory)
                completer->addEntry(dir);
        }
        dirLabel->setBuddy(m_directory);
        gridLayout->addWidget(m_directory, row++, 1, 1, 2);

        const QList<QPair<QWidget *, QWidget *>> patternWidgets = createPatternWidgets();
        for (const QPair<QWidget *, QWidget *> &p : patternWidgets) {
            gridLayout->addWidget(p.first, row, 0, Qt::AlignRight);
            gridLayout->addWidget(p.second, row, 1, 1, 2);
            ++row;
        }
        m_configWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

        // validity
        auto updateValidity = [this] {
            setValid(currentSearchEngine()->isEnabled() && m_directory->isValid());
        };
        connect(this, &BaseFileFind::currentSearchEngineChanged, this, updateValidity);
        for (const SearchEngine *searchEngine : searchEngineVector)
            connect(searchEngine, &SearchEngine::enabledChanged, this, updateValidity);
        connect(m_directory.data(), &PathChooser::validChanged, this, updateValidity);
        updateValidity();
    }
    return m_configWidget;
}

FilePath FindInFiles::path() const
{
    return m_directory->filePath();
}

void FindInFiles::writeSettings(QSettings *settings)
{
    settings->beginGroup(QLatin1String("FindInFiles"));
    writeCommonSettings(settings);
    settings->endGroup();
}

void FindInFiles::readSettings(QSettings *settings)
{
    settings->beginGroup(QLatin1String("FindInFiles"));
    readCommonSettings(settings, "*.cpp,*.h", "*/.git/*,*/.cvs/*,*/.svn/*,*.autosave");
    settings->endGroup();
}

void FindInFiles::setDirectory(const FilePath &directory)
{
    m_directory->setFilePath(directory);
}

void FindInFiles::setBaseDirectory(const FilePath &directory)
{
    m_directory->setBaseDirectory(directory);
}

FilePath FindInFiles::directory() const
{
    return m_directory->filePath();
}

void FindInFiles::findOnFileSystem(const QString &path)
{
    QTC_ASSERT(m_instance, return);
    const QFileInfo fi(path);
    const QString folder = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    m_instance->setDirectory(FilePath::fromString(folder));
    Find::openFindDialog(m_instance);
}

FindInFiles *FindInFiles::instance()
{
    return m_instance;
}
