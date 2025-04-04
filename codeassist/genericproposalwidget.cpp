// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "genericproposalwidget.h"
#include "genericproposalmodel.h"
#include "assistproposalitem.h"
#include "codeassistant.h"

#include "completionsettings.h"
#include "texteditorconstants.h"
#include "codeassist/assistproposaliteminterface.h"

#include "utils/algorithm.h"
#include "utils/faketooltip.h"
#include "utils/hostosinfo.h"

#include <QRect>
#include <QLatin1String>
#include <QAbstractListModel>
#include <QPointer>
#include <QDebug>
#include <QTimer>
#include <QApplication>
#include <QVBoxLayout>
#include <QListView>
#include <QAbstractItemView>
#include <QScreen>
#include <QScrollBar>
#include <QKeyEvent>
#include <QLabel>
#include <QStyledItemDelegate>

using namespace Utils;

namespace TextEditor {

// ------------
// ModelAdapter
// ------------
class ModelAdapter : public QAbstractListModel
{
    Q_OBJECT

public:
    ModelAdapter(GenericProposalModelPtr completionModel, QWidget *parent);

    int rowCount(const QModelIndex &) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    enum UserRoles{ FixItRole = Qt::UserRole, DetailTextFormatRole };
private:
    GenericProposalModelPtr m_completionModel;
};

ModelAdapter::ModelAdapter(GenericProposalModelPtr completionModel, QWidget *parent)
    : QAbstractListModel(parent)
    , m_completionModel(completionModel)
{}

int ModelAdapter::rowCount(const QModelIndex &index) const
{
    return index.isValid() ? 0 : m_completionModel->size();
}

QVariant ModelAdapter::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_completionModel->size())
        return QVariant();

    if (role == Qt::DisplayRole) {
        const QString text = m_completionModel->text(index.row());
        const int lineBreakPos = text.indexOf('\n');
        if (lineBreakPos < 0)
            return text;
        return QString(text.left(lineBreakPos) + QLatin1String(" (...)"));
    } else if (role == Qt::DecorationRole) {
        return m_completionModel->icon(index.row());
    } else if (role == Qt::WhatsThisRole) {
        return m_completionModel->detail(index.row());
    } else if (role == DetailTextFormatRole) {
        return m_completionModel->detailFormat(index.row());
    } else if (role == FixItRole) {
        return m_completionModel->proposalItem(index.row())->requiresFixIts();
    }

    return QVariant();
}

// ------------------------
// GenericProposalInfoFrame
// ------------------------
class GenericProposalInfoFrame : public FakeToolTip
{
public:
    GenericProposalInfoFrame(QWidget *parent = nullptr)
        : FakeToolTip(parent), m_label(new QLabel(this))
    {
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(m_label);

        // Limit horizontal width
        m_label->setSizePolicy(QSizePolicy::Fixed, m_label->sizePolicy().verticalPolicy());

        m_label->setForegroundRole(QPalette::ToolTipText);
        m_label->setBackgroundRole(QPalette::ToolTipBase);
    }

    void setText(const QString &text)
    {
        m_label->setText(text);
    }

    void setTextFormat(Qt::TextFormat format)
    {
        m_label->setTextFormat(format);
    }

    // Workaround QTCREATORBUG-11653
    void calculateMaximumWidth()
    {
        const QRect screenGeometry = screen()->availableGeometry();
        const int xOnScreen = this->pos().x() - screenGeometry.x();
        const QMargins widgetMargins = contentsMargins();
        const QMargins layoutMargins = layout()->contentsMargins();
        const int margins = widgetMargins.left() + widgetMargins.right()
                + layoutMargins.left() + layoutMargins.right();
        m_label->setMaximumWidth(qMax(0, screenGeometry.width() - xOnScreen - margins));
    }

private:
    QLabel *m_label;
};

// -----------------------
// GenericProposalListView
// -----------------------
class GenericProposalListView : public QListView
{
    friend class ProposalItemDelegate;
public:
    GenericProposalListView(QWidget *parent);

    QSize calculateSize() const;
    QPoint infoFramePos() const;

    int rowSelected() const { return currentIndex().row(); }
    bool isFirstRowSelected() const { return rowSelected() == 0; }
    bool isLastRowSelected() const { return rowSelected() == model()->rowCount() - 1; }
    void selectRow(int row) { setCurrentIndex(model()->index(row, 0)); }
    void selectFirstRow() { selectRow(0); }
    void selectLastRow() { selectRow(model()->rowCount() - 1); }
};

class ProposalItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ProposalItemDelegate(GenericProposalListView *parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_parent(parent)
    {
    }

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const QIcon fixItIcon;

        QStyledItemDelegate::paint(painter, option, index);

        if (m_parent->model()->data(index, ModelAdapter::FixItRole).toBool()) {
            const QRect itemRect = m_parent->rectForIndex(index);
            const QScrollBar *verticalScrollBar = m_parent->verticalScrollBar();

            const int x = m_parent->width() - itemRect.height() - (verticalScrollBar->isVisible()
                                                                   ? verticalScrollBar->width()
                                                                   : 0);
            const int iconSize = itemRect.height() - 5;
            fixItIcon.paint(painter, QRect(x, itemRect.y() - m_parent->verticalOffset(),
                                           iconSize, iconSize));
        }
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size(QStyledItemDelegate::sizeHint(option, index));
        if (m_parent->model()->data(index, ModelAdapter::FixItRole).toBool())
            size.setWidth(size.width() + m_parent->rectForIndex(index).height() - 5);
        return size;
    }
private:
    GenericProposalListView *m_parent;
};

GenericProposalListView::GenericProposalListView(QWidget *parent)
    : QListView(parent)
{
    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    setItemDelegate(new ProposalItemDelegate(this));
}

QSize GenericProposalListView::calculateSize() const
{
    static const int maxVisibleItems = 10;

    // Determine size by calculating the space of the visible items
    const int visibleItems = qMin(model()->rowCount(), maxVisibleItems);
    const int firstVisibleRow = verticalScrollBar()->value();

    QSize shint;
    for (int i = 0; i < visibleItems; ++i) {
        QSize tmp = sizeHintForIndex(model()->index(i + firstVisibleRow, 0));
        if (shint.width() < tmp.width())
            shint = tmp;
    }
    shint.rheight() *= visibleItems;

    return shint;
}

QPoint GenericProposalListView::infoFramePos() const
{
    const QRect &r = rectForIndex(currentIndex());
    QPoint p((parentWidget()->mapToGlobal(
                    parentWidget()->rect().topRight())).x() + 3,
            mapToGlobal(r.topRight()).y() - verticalOffset()
            );
    return p;
}

// ----------------------------
// GenericProposalWidgetPrivate
// ----------------------------
class GenericProposalWidgetPrivate : public QObject
{
    Q_OBJECT

public:
    GenericProposalWidgetPrivate(QWidget *completionWidget);

    const QWidget *m_underlyingWidget = nullptr;
    GenericProposalListView *m_completionListView;
    GenericProposalModelPtr m_model;
    QRect m_displayRect;
    bool m_isSynchronized = true;
    bool m_explicitlySelected = false;
    AssistReason m_reason = IdleEditor;
    AssistKind m_kind = Completion;
    bool m_justInvoked = false;
    QPointer<GenericProposalInfoFrame> m_infoFrame;
    QTimer m_infoTimer;
    CodeAssistant *m_assistant = nullptr;
    bool m_autoWidth = true;

    void handleActivation(const QModelIndex &modelIndex);
    void maybeShowInfoTip();
};

GenericProposalWidgetPrivate::GenericProposalWidgetPrivate(QWidget *completionWidget)
    : m_completionListView(new GenericProposalListView(completionWidget))
{
    m_completionListView->setIconSize(QSize(16, 16));
    connect(m_completionListView, &QAbstractItemView::activated,
            this, &GenericProposalWidgetPrivate::handleActivation);

    m_infoTimer.setInterval(Constants::COMPLETION_ASSIST_TOOLTIP_DELAY);
    m_infoTimer.setSingleShot(true);
    connect(&m_infoTimer, &QTimer::timeout, this, &GenericProposalWidgetPrivate::maybeShowInfoTip);
}

void GenericProposalWidgetPrivate::handleActivation(const QModelIndex &modelIndex)
{
    static_cast<GenericProposalWidget *>
            (m_completionListView->parent())->notifyActivation(modelIndex.row());
}

void GenericProposalWidgetPrivate::maybeShowInfoTip()
{
    const QModelIndex &current = m_completionListView->currentIndex();
    if (!current.isValid())
        return;

    const QString &infoTip = current.data(Qt::WhatsThisRole).toString();
    if (infoTip.isEmpty()) {
        delete m_infoFrame.data();
        m_infoTimer.setInterval(200);
        return;
    }

    if (m_infoFrame.isNull())
        m_infoFrame = new GenericProposalInfoFrame(m_completionListView);

    m_infoFrame->move(m_completionListView->infoFramePos());
    m_infoFrame->setTextFormat(
        current.data(ModelAdapter::DetailTextFormatRole).value<Qt::TextFormat>());
    m_infoFrame->setText(infoTip);
    m_infoFrame->calculateMaximumWidth();
    m_infoFrame->adjustSize();
    m_infoFrame->show();
    m_infoFrame->raise();

    m_infoTimer.setInterval(0);
}

// ------------------------
// GenericProposalWidget
// ------------------------
GenericProposalWidget::GenericProposalWidget()
    : d(new GenericProposalWidgetPrivate(this))
{
    if (HostOsInfo::isMacHost()) {
        if (d->m_completionListView->horizontalScrollBar())
            d->m_completionListView->horizontalScrollBar()->setAttribute(Qt::WA_MacMiniSize);
        if (d->m_completionListView->verticalScrollBar())
            d->m_completionListView->verticalScrollBar()->setAttribute(Qt::WA_MacMiniSize);
    } else {
        // This improves the look with QGTKStyle.
        setFrameStyle(d->m_completionListView->frameStyle());
    }
    d->m_completionListView->setFrameStyle(QFrame::NoFrame);
    d->m_completionListView->setAttribute(Qt::WA_MacShowFocusRect, false);
    d->m_completionListView->setUniformItemSizes(true);
    d->m_completionListView->setSelectionBehavior(QAbstractItemView::SelectItems);
    d->m_completionListView->setSelectionMode(QAbstractItemView::SingleSelection);
    d->m_completionListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->m_completionListView->setMinimumSize(1, 1);
    connect(d->m_completionListView->verticalScrollBar(), &QAbstractSlider::valueChanged,
            this, &GenericProposalWidget::updatePositionAndSize);
    connect(d->m_completionListView->verticalScrollBar(), &QAbstractSlider::sliderPressed,
            this, &GenericProposalWidget::turnOffAutoWidth);
    connect(d->m_completionListView->verticalScrollBar(), &QAbstractSlider::sliderReleased,
            this, &GenericProposalWidget::turnOnAutoWidth);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(d->m_completionListView);

    d->m_completionListView->installEventFilter(this);

    setObjectName(QLatin1String("m_popupFrame"));
    setMinimumSize(1, 1);
}

GenericProposalWidget::~GenericProposalWidget()
{
    delete d;
}

void GenericProposalWidget::setAssistant(CodeAssistant *assistant)
{
    d->m_assistant = assistant;
}

void GenericProposalWidget::setReason(AssistReason reason)
{
    d->m_reason = reason;
    if (d->m_reason == ExplicitlyInvoked)
        d->m_justInvoked = true;
}

void GenericProposalWidget::setKind(AssistKind kind)
{
    d->m_kind = kind;
}

void GenericProposalWidget::setUnderlyingWidget(const QWidget *underlyingWidget)
{
    setFont(underlyingWidget->font());
    d->m_underlyingWidget = underlyingWidget;
}

void GenericProposalWidget::setModel(ProposalModelPtr model)
{
    d->m_model = model.staticCast<GenericProposalModel>();
    d->m_completionListView->setModel(new ModelAdapter(d->m_model, d->m_completionListView));

    connect(d->m_completionListView->selectionModel(), &QItemSelectionModel::currentChanged,
            &d->m_infoTimer, QOverload<>::of(&QTimer::start));
}

void GenericProposalWidget::setDisplayRect(const QRect &rect)
{
    d->m_displayRect = rect;
}

void GenericProposalWidget::setIsSynchronized(bool isSync)
{
    d->m_isSynchronized = isSync;
}

bool GenericProposalWidget::supportsModelUpdate(const Utils::Id &proposalId) const
{
    return proposalId == Constants::GENERIC_PROPOSAL_ID;
}

void GenericProposalWidget::updateModel(ProposalModelPtr model)
{
    QString currentText;
    if (d->m_explicitlySelected)
        currentText = d->m_model->text(d->m_completionListView->currentIndex().row());
    d->m_model = model.staticCast<GenericProposalModel>();
    if (d->m_model->containsDuplicates())
        d->m_model->removeDuplicates();
    d->m_completionListView->setModel(new ModelAdapter(d->m_model, d->m_completionListView));
    connect(d->m_completionListView->selectionModel(), &QItemSelectionModel::currentChanged,
            &d->m_infoTimer, QOverload<>::of(&QTimer::start));
    int currentRow = -1;
    if (!currentText.isEmpty()) {
        currentRow = d->m_model->indexOf(
            Utils::equal(&AssistProposalItemInterface::text, currentText));
    }
    if (currentRow >= 0)
        d->m_completionListView->selectRow(currentRow);
    else
        d->m_explicitlySelected = false;
}

void GenericProposalWidget::showProposal(const QString &prefix)
{
    ensurePolished();
    if (d->m_model->containsDuplicates())
        d->m_model->removeDuplicates();
    if (!updateAndCheck(prefix))
        return;
    show();
    d->m_completionListView->setFocus();
}

void GenericProposalWidget::updateProposal(const QString &prefix)
{
    if (!isVisible())
        return;
    updateAndCheck(prefix);
}

void GenericProposalWidget::closeProposal()
{
    abort();
}

void GenericProposalWidget::notifyActivation(int index)
{
    abort();
    emit proposalItemActivated(d->m_model->proposalItem(index));
}

void GenericProposalWidget::abort()
{
    deleteLater();
    if (isVisible())
        close();
}

bool GenericProposalWidget::updateAndCheck(const QString &prefix)
{
    // Keep track in the case there has been an explicit selection.
    int preferredItemId = -1;
    if (d->m_explicitlySelected)
        preferredItemId =
                d->m_model->persistentId(d->m_completionListView->currentIndex().row());

    // Filter, sort, etc.
    if (!d->m_model->isPrefiltered(prefix)) {
        d->m_model->reset();
        if (!prefix.isEmpty())
            d->m_model->filter(prefix);
    }
    if (!d->m_model->hasItemsToPropose(prefix, d->m_reason)) {
        d->m_completionListView->reset();
        abort();
        return false;
    }
    if (d->m_model->isSortable(prefix))
        d->m_model->sort(prefix);
    d->m_completionListView->reset();

    // Try to find the previously explicit selection (if any). If we can find the item set it
    // as the current. Otherwise (it might have been filtered out) select the first row.
    if (d->m_explicitlySelected) {
        Q_ASSERT(preferredItemId != -1);
        for (int i = 0; i < d->m_model->size(); ++i) {
            if (d->m_model->persistentId(i) == preferredItemId) {
                d->m_completionListView->selectRow(i);
                break;
            }
        }
    }
    if (!d->m_completionListView->currentIndex().isValid()) {
        d->m_completionListView->selectFirstRow();
        if (d->m_explicitlySelected)
            d->m_explicitlySelected = false;
    }

    /*if (TextEditorSettings::completionSettings().m_partiallyComplete
            && d->m_kind == Completion
            && d->m_justInvoked
            && d->m_isSynchronized) {
        if (d->m_model->size() == 1) {
            AssistProposalItemInterface *item = d->m_model->proposalItem(0);
            if (item->implicitlyApplies()) {
                d->m_completionListView->reset();
                abort();
                emit proposalItemActivated(item);
                return false;
            }
        }
        if (d->m_model->supportsPrefixExpansion()) {
            const QString &proposalPrefix = d->m_model->proposalPrefix();
            if (proposalPrefix.length() > prefix.length())
                emit prefixExpanded(proposalPrefix);
        }
    }
*/
    if (d->m_justInvoked)
        d->m_justInvoked = false;

    updatePositionAndSize();
    return true;
}

void GenericProposalWidget::updatePositionAndSize()
{
    if (!d->m_autoWidth)
        return;

    const QSize &shint = d->m_completionListView->calculateSize();
    const int fw = frameWidth();
    const int width = shint.width() + fw * 2 + 30;
    const int height = shint.height() + fw * 2;

    // Determine the position, keeping the popup on the screen
    const QRect screen = d->m_underlyingWidget->screen()->availableGeometry();

    QPoint pos = d->m_displayRect.bottomLeft();
    pos.rx() -= 16 + fw;    // Space for the icons
    if (pos.y() + height > screen.bottom())
        pos.setY(qMax(0, d->m_displayRect.top() - height));
    if (pos.x() + width > screen.right())
        pos.setX(qMax(0, screen.right() - width));
    setGeometry(pos.x(), pos.y(), qMin(width, screen.width()), qMin(height, screen.height()));
}

void GenericProposalWidget::turnOffAutoWidth()
{
    d->m_autoWidth = false;
}

void GenericProposalWidget::turnOnAutoWidth()
{
    d->m_autoWidth = true;
    updatePositionAndSize();
}

bool GenericProposalWidget::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::FocusOut) {
        abort();
        if (d->m_infoFrame)
            d->m_infoFrame->close();
        return true;
    } else if (e->type() == QEvent::ShortcutOverride) {
        auto ke = static_cast<QKeyEvent *>(e);
        switch (ke->key()) {
        case Qt::Key_N:
        case Qt::Key_P:
        case Qt::Key_BracketLeft:
            if (ke->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
                e->accept();
                return true;
            }
        }
    } else if (e->type() == QEvent::KeyPress) {
        auto ke = static_cast<QKeyEvent *>(e);
        switch (ke->key()) {
        case Qt::Key_Escape:
            abort();
            emit explicitlyAborted();
            e->accept();
            return true;

        case Qt::Key_BracketLeft:
            // vim-style behavior
            if (ke->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
                abort();
                emit explicitlyAborted();
                e->accept();
                return true;
            }
            break;

        case Qt::Key_N:
        case Qt::Key_P:
            // select next/previous completion
            if (ke->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
                d->m_explicitlySelected = true;
                int change = (ke->key() == Qt::Key_N) ? 1 : -1;
                int nrows = d->m_model->size();
                int row = d->m_completionListView->currentIndex().row();
                int newRow = (row + change + nrows) % nrows;
                if (newRow == row + change || !ke->isAutoRepeat())
                    d->m_completionListView->selectRow(newRow);
                return true;
            }
            break;

        case Qt::Key_Tab:
        case Qt::Key_Return:
        case Qt::Key_Enter:
            abort();
            activateCurrentProposalItem();
            return true;

        case Qt::Key_Up:
            d->m_explicitlySelected = true;
            if (!ke->isAutoRepeat() && d->m_completionListView->isFirstRowSelected()) {
                d->m_completionListView->selectLastRow();
                return true;
            }
            return false;

        case Qt::Key_Down:
            d->m_explicitlySelected = true;
            if (!ke->isAutoRepeat() && d->m_completionListView->isLastRowSelected()) {
                d->m_completionListView->selectFirstRow();
                return true;
            }
            return false;

        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            return false;

        case Qt::Key_Right:
        case Qt::Key_Left:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_Backspace:
            // We want these navigation keys to work in the editor.
            QApplication::sendEvent(const_cast<QWidget *>(d->m_underlyingWidget), e);
            if (isVisible())
                d->m_assistant->notifyChange();
            return true;

        default:
            // Only forward keys that insert text and refine the completion.
            if (ke->text().isEmpty() && !(ke == QKeySequence::Paste))
                return true;
            break;
        }

        if (ke->text().length() == 1
                && d->m_completionListView->currentIndex().isValid()
                && QApplication::focusWidget() == o) {
            const QChar &typedChar = ke->text().at(0);
            AssistProposalItemInterface *item =
                d->m_model->proposalItem(d->m_completionListView->currentIndex().row());
            if (item->prematurelyApplies(typedChar)
                    && (d->m_reason == ExplicitlyInvoked || item->text().endsWith(typedChar))) {
                abort();
                emit proposalItemActivated(item);
                return true;
            }
        }

        QApplication::sendEvent(const_cast<QWidget *>(d->m_underlyingWidget), e);

        return true;
    }
    return false;
}

bool GenericProposalWidget::activateCurrentProposalItem()
{
    if (d->m_completionListView->currentIndex().isValid()) {
        const int currentRow = d->m_completionListView->currentIndex().row();
        emit proposalItemActivated(d->m_model->proposalItem(currentRow));
        return true;
    }
    return false;
}

GenericProposalModelPtr GenericProposalWidget::model()
{
    return d->m_model;
}

} // namespace TextEditor

#include "genericproposalwidget.moc"
