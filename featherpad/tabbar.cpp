/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2019 <tsujan2000@gmail.com>
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

#include <QPointer>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QIcon>
#include <QApplication>
#include <QToolTip>
#include "tabbar.h"

namespace FeatherPad {

TabBar::TabBar (QWidget *parent)
    : QTabBar (parent)
{
    setMouseTracking (true);
    setElideMode (Qt::ElideMiddle); // works with minimumTabSizeHint()
    hideSingle_ = false;
    locked_ = false;
    dragStarted_ = false; // not needed
    noTabDND_ = false;
}
/*************************/
void TabBar::mousePressEvent (QMouseEvent *event)
{
    dragStarted_ = false;
    dragStartPosition_ = QPoint();

    if (locked_)
    {
        event->ignore();
        return;
    }
    QTabBar::mousePressEvent (event);

    if (event->button() == Qt::LeftButton) {
        if (tabAt (event->pos()) > -1)
            dragStartPosition_ = event->pos();
        else if (event->type() == QEvent::MouseButtonDblClick && count() > 0)
            emit addEmptyTab();
    }
}
/*************************/
void TabBar::mouseReleaseEvent (QMouseEvent *event)
{
    QTabBar::mouseReleaseEvent (event);
    if (event->button() == Qt::MiddleButton)
    {
        int index = tabAt (event->pos());
        if (index > -1)
            emit tabCloseRequested (index);
        else
            emit hideTabBar();
    }
}
/*************************/
void TabBar::mouseMoveEvent (QMouseEvent *event)
{
    if (!dragStarted_ && !dragStartPosition_.isNull()
        && (event->pos() - dragStartPosition_).manhattanLength() >= QApplication::startDragDistance())
    {
      dragStarted_ = true;
    }

    if (!noTabDND_
        && (event->buttons() & Qt::LeftButton)
        && dragStarted_
        && !window()->geometry().contains (event->globalPos()))
    {
        int index = currentIndex();
        if (index == -1)
        {
            event->accept();
            return;
        }

        QPointer<QDrag> drag = new QDrag (this);
        QMimeData *mimeData = new QMimeData;
        QByteArray array = QString::number (index).toUtf8();
        mimeData->setData ("application/featherpad-tab", array);
        drag->setMimeData (mimeData);
        QPixmap px = QIcon (":icons/tab.svg").pixmap (22, 22);
        drag->setPixmap (px);
        drag->setHotSpot (QPoint (px.width()/2, px.height()));
        int N = count();
        Qt::DropAction dragged = drag->exec (Qt::MoveAction);
        if (dragged != Qt::MoveAction)
        {
            /* A tab is dropped outside all windows. WARNING: Under Enlightenment,
               this may be Qt::CopyAction, not IgnoreAction (an E bug). */
            if (N > 1)
                emit tabDetached();
            else
                finishMouseMoveEvent();
        }
        else // a tab is dropped into another window
        {
            /* WARNING: Theoretically, only FeatherPad should accept its tab drops
               but another app (like Dolphin) may incorrectly accept any drop. So,
               if no tab is removed here, we release the mouse as a workaround. */
            if (count() == N)
                releaseMouse();
        }
        event->accept();
        drag->deleteLater();
    }
    else
    {
        QTabBar::mouseMoveEvent (event);
        int index = tabAt (event->pos());
        if (index > -1)
            QToolTip::showText (event->globalPos(), tabToolTip (index));
        else
            QToolTip::hideText();
    }
}
/*************************/
// Don't show tooltip with setTabToolTip().
bool TabBar::event (QEvent *event)
{
#ifndef QT_NO_TOOLTIP
    if (event->type() == QEvent::ToolTip)
        return QWidget::event (event);
    else
       return QTabBar::event (event);
#else
    return QTabBar::event (event);
#endif
}
/*************************/
void TabBar::wheelEvent (QWheelEvent *event)
{
    if (!locked_)
        QTabBar::wheelEvent (event);
    else
        event->ignore();
}
/*************************/
void TabBar::tabRemoved (int/* index*/)
{
    if (hideSingle_ && count() == 1)
        hide();
}
/*************************/
void TabBar::tabInserted (int/* index*/)
{
    if (hideSingle_)
    {
        if (count() == 1)
            hide();
        else if (count() == 2)
            show();
    }
}
/*************************/
void TabBar::finishMouseMoveEvent()
{
    QMouseEvent finishingEvent (QEvent::MouseMove, QPoint(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    mouseMoveEvent (&finishingEvent);
}
/*************************/
void TabBar::releaseMouse()
{
    QMouseEvent releasingEvent (QEvent::MouseButtonRelease, QPoint(), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    mouseReleaseEvent (&releasingEvent);
}
/*************************/
QSize TabBar::tabSizeHint(int index) const
{
    switch (shape()) {
    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        return QSize (QTabBar::tabSizeHint (index).width(),
                      qMin (height()/2, QTabBar::tabSizeHint (index).height()));
    default:
        return QSize (qMin (width()/2, QTabBar::tabSizeHint (index).width()),
                      QTabBar::tabSizeHint (index).height());
    }
}
/*************************/
// Set minimumTabSizeHint to tabSizeHint
// to keep tabs from shrinking with eliding.
QSize TabBar::minimumTabSizeHint(int index) const
{
    return tabSizeHint (index);
}

}
