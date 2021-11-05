/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2017-2021 <tsujan2000@gmail.com>
 *
 * FeatherPad is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FeatherPad is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @license GPL-3.0+ <https://spdx.org/licenses/GPL-3.0+.html>
 */

#include <QApplication>
#include <QGridLayout>
#include <QToolTip>
#include <QScrollBar>
#include "sidepane.h"

namespace FeatherPad {

bool ListWidgetItem::operator<(const QListWidgetItem &other) const {
    /* treat dot as a separator for a natural sorting */
    const QString txt1 = text();
    const QString txt2 = other.text();
    int start1 = 0, start2 = 0;
    for (;;)
    {
        int end1 = txt1.indexOf (QLatin1Char ('.'), start1);
        int end2 = txt2.indexOf (QLatin1Char ('.'), start2);
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
        QStringRef part1 = txt1.midRef (start1, end1 - start1);
        QStringRef part2 = txt2.midRef (start2, end2 - start2);
#else
        QString part1 = end1 == -1 ? txt1.sliced (start1, txt1.size() - start1)
                                   : txt1.sliced (start1, end1 - start1);
        QString part2 = end2 == -1 ? txt2.sliced (start2, txt2.size() - start2)
                                   : txt2.sliced (start2, end2 - start2);
#endif
        int comp = collator_.compare (part1, part2);
        if (comp == 0)
            comp = part1.size() - part2.size(); // a workaround for QCollator's bug
        if (comp != 0)
            return comp < 0;
        if (end1 == -1 || end2 == -1)
            return end1 < end2;
        start1 = end1 + 1;
        start2 = end2 + 1;
    }
}
/*************************/
ListWidget::ListWidget (QWidget *parent) : QListWidget (parent)
{
    setAutoScroll (false); // Qt's autoscrolling has bugs; see ListWidget::scrollToCurrentItem()
    setMouseTracking (true); // for instant tooltips
    locked_ = false;

    /* try to have a current item as far as possible and announce it */
    connect (this, &QListWidget::currentItemChanged, [this] (QListWidgetItem *current, QListWidgetItem *previous) {
        if (current == previous) return;
        if (current == nullptr)
        {
            if (count() == 0) return;
            if (previous != nullptr)
            {
                int prevRow = row (previous);
                if (prevRow < count() - 1)
                    setCurrentRow (prevRow + 1);
                else if (prevRow != 0)
                    setCurrentRow (0);
            }
            else setCurrentRow (0);
        }
        else
        {
            emit currentItemUpdated (current);
            scrollToCurrentItem();
            /* this is sometimes needed because, with filtering, Qt may give the
               focus to a hidden item (which is fine) but select a visible one */
            QTimer::singleShot (0, this, [this]() {
                auto index = currentIndex();
                if (index.isValid())
                    selectionModel()->select (index, QItemSelectionModel::ClearAndSelect);
            });
        }
    });
}
/*************************/
// To prevent deselection by Ctrl + left click; see "qabstractitemview.cpp".
QItemSelectionModel::SelectionFlags ListWidget::selectionCommand (const QModelIndex &index, const QEvent *event) const
{
    Qt::KeyboardModifiers keyModifiers = Qt::NoModifier;
    if (event)
    {
        switch (event->type()) {
            case QEvent::MouseButtonDblClick:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseMove:
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
                keyModifiers = (static_cast<const QInputEvent*>(event))->modifiers();
                break;
            default:
                keyModifiers = QApplication::keyboardModifiers();
        }
    }
    if (selectionMode() == QAbstractItemView::SingleSelection && (keyModifiers & Qt::ControlModifier) && selectionModel()->isSelected (index))
        return QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows;
    else
        return QListWidget::selectionCommand (index, event);
}
/*************************/
// QListView::scrollTo() doesn't work fine because it doesn't
// consider the current scrollbar, after some items are hidden.
void ListWidget::scrollToCurrentItem()
{
    QModelIndex index = currentIndex();
    if (!index.isValid()) return;
    const QRect rect = visualRect (index);
    const QRect area = viewport()->rect();
    if (area.contains (rect)) return;

    bool above (rect.top() < area.top());
    bool below (rect.bottom() > area.bottom());

    int verticalValue = verticalScrollBar()->value();
    QRect r = rect.adjusted (-spacing(), -spacing(), spacing(), spacing());
    if (above)
        verticalValue += r.top();
    else if (below)
        verticalValue += qMin (r.top(), r.bottom() - area.height() + 1);
    verticalScrollBar()->setValue (verticalValue);
}
/*************************/
void ListWidget::mousePressEvent (QMouseEvent *event)
{
    if (locked_)
    {
        event->ignore();
        return;
    }
    if (selectionMode() == QAbstractItemView::SingleSelection)
    {
        if (event->button() == Qt::MiddleButton)
        {
            QModelIndex index = indexAt (event->pos());
            if (QListWidgetItem *item = itemFromIndex (index)) // index is checked for its validity in QListWidget::itemFromIndex()
                emit closeItem (item);
            else
                emit closeSidePane();
            return;
        }
        else if (event->button() == Qt::RightButton)
            return;
    }
    QListWidget::mousePressEvent (event);
}
/*************************/
void ListWidget::mouseMoveEvent (QMouseEvent *event)
{
    QListWidget::mouseMoveEvent (event);
    if (event->button() == Qt::NoButton && !(event->buttons() & Qt::LeftButton))
    {
        if (QListWidgetItem *item = itemAt (event->pos()))
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
            QToolTip::showText (event->globalPos(), item->toolTip());
#else
            QToolTip::showText (event->globalPosition().toPoint(), item->toolTip());
#endif
        else
            QToolTip::hideText();
    }
}
/*************************/
QListWidgetItem *ListWidget::getItemFromIndex (const QModelIndex &index) const
{
    return itemFromIndex (index);
}
/*************************/
void ListWidget::rowsInserted (const QModelIndex &parent, int start, int end)
{
    emit rowsAreInserted (start, end);
    QListView::rowsInserted (parent, start, end);
}
/*************************/
SidePane::SidePane (QWidget *parent)
    : QWidget (parent)
{
    filterTimer_ = nullptr;

    QGridLayout *mainGrid = new QGridLayout;
    mainGrid->setVerticalSpacing (4);
    mainGrid->setContentsMargins (0, 0, 0, 0);
    lw_ = new ListWidget (this);
    lw_->setSortingEnabled (true);
    lw_->setSelectionMode (QAbstractItemView::SingleSelection);
    lw_->setContextMenuPolicy (Qt::CustomContextMenu);
    mainGrid->addWidget (lw_, 0, 0);
    le_ = new LineEdit (this);
    le_->setPlaceholderText (tr ("Filter..."));
    mainGrid->addWidget (le_, 1, 0);
    setLayout (mainGrid);

    connect (le_, &QLineEdit::textChanged, this, &SidePane::filter);
    connect (lw_, &ListWidget::rowsAreInserted, this, &SidePane::onRowsInserted);
    connect (lw_, &ListWidget::itemChanged, [this] (QListWidgetItem *item) {
        if (item->text().contains (le_->text(), Qt::CaseInsensitive))
            item->setHidden (false);
        else
            item->setHidden (true);
    });

    lw_->installEventFilter (this);
}
/*************************/
SidePane::~SidePane()
{
    if (filterTimer_)
    {
        disconnect (filterTimer_, &QTimer::timeout, this, &SidePane::reallyApplyFilter);
        filterTimer_->stop();
        delete filterTimer_;
    }
}
/*************************/
bool SidePane::eventFilter (QObject *watched, QEvent *event)
{
    if (watched == lw_ && event->type() == QEvent::KeyPress)
    { // when a text is typed inside the list, type it inside the filter line-edit too
        if (QKeyEvent *ke = static_cast<QKeyEvent*>(event))
        {
            if (ke->key() < Qt::Key_Home || ke->key() > Qt::Key_PageDown)
            {
                le_->pressKey (ke);
                return true; // don't change the selection
            }
        }
    }
    return QWidget::eventFilter (watched, event);
}
/*************************/
void SidePane::filter (const QString&/*text*/)
{
    if (!filterTimer_)
    {
        filterTimer_ = new QTimer();
        filterTimer_->setSingleShot (true);
        connect (filterTimer_, &QTimer::timeout, this, &SidePane::reallyApplyFilter);
    }
    filterTimer_->start (200);
}
/*************************/
// Simply hide items that don't contain the filter string.
// Clearing items isn't an option because they are tracked
// for their correspondence with tab pages.
void SidePane::reallyApplyFilter()
{
    for (int i = lw_->count() - 1; i >= 0; --i)
    { // from end to start for the scrollbar to have a correct position
        QListWidgetItem *wi = lw_->item (i);
        if (wi->text().contains (le_->text(), Qt::CaseInsensitive))
            wi->setHidden (false);
        else
            wi->setHidden (true);
    }
    lw_->scrollToCurrentItem();
}
/*************************/
void SidePane::onRowsInserted (int start, int end)
{
    for (int i = start; i <= end; ++i)
    {
        if (lw_->item (i) && !lw_->item (i)->text().contains (le_->text(), Qt::CaseInsensitive))
            lw_->item (i)->setHidden (true);
    }
}
/*************************/
void SidePane::lockPane (bool lock)
{
    lw_ ->lockListWidget (lock);
    le_->setEnabled (!lock);
}

}
