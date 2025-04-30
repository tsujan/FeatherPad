/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2025 <tsujan2000@gmail.com>
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

const char *TabBar::tabDropped = "_fpad_tab_dropped";

TabBar::TabBar (QWidget *parent)
    : QTabBar (parent)
{
    hideSingle_ = false;
    locked_ = false;
    dragStarted_ = false;
    noTabDND_ = false;

    setMouseTracking (true);
    setElideMode (Qt::ElideMiddle); // works with minimumTabSizeHint()
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
        if (tabAt (event->position().toPoint()) > -1)
            dragStartPosition_ = event->position().toPoint();
        else if (event->type() == QEvent::MouseButtonDblClick && count() > 0)
            emit addEmptyTab();
    }
}
/*************************/
void TabBar::mouseReleaseEvent (QMouseEvent *event)
{
    dragStarted_ = false;
    dragStartPosition_ = QPoint();

    QTabBar::mouseReleaseEvent (event);
    if (event->button() == Qt::MiddleButton)
    {
        int index = tabAt (event->position().toPoint());
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
        && (event->position().toPoint()
            - dragStartPosition_).manhattanLength() >= QApplication::startDragDistance())
    {
      dragStarted_ = true;
    }

    if (!noTabDND_
        && (event->buttons() & Qt::LeftButton)
        && dragStarted_
        && !window()->geometry().contains (event->globalPosition().toPoint()))
    {
        int index = currentIndex();
        if (index == -1)
        {
            event->accept();
            return;
        }

        /*
           NOTE:

           To be on the safe side (especially under Wayland), we detach or drop the tab
           only after finishing the DND; see "FPwin::dropEvent()" and the queued
           connection of "tabDetached()" in "fpwin.cpp".

           Also, it's important to release the mouse after DND but before tab removal;
           otherwise, the tabbar might not be updated properly. That's done in
           "FPwin::detachTab" and "FPwin::dropTab".
        */

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
            /* The drop hasn't been accepted (by any FeatherPad window).
               The tab will be detached if there's more than one tab. */
            if (N > 1)
                emit tabDetached();
            else
                finishMouseMoveEvent();
        }
        else
        {
            /* WARNING: Since another app can also accept this drop, we check
               the object property "_fpad_tab_dropped" (set by "FPwin::dropEvent")
               and detach the tab if it's missing. */
            if (property (TabBar::tabDropped).toBool())
                setProperty (TabBar::tabDropped, QVariant()); // reset
            else
            {
                if (N > 1)
                    emit tabDetached();
                else
                    finishMouseMoveEvent();
            }
        }
        event->accept();
        if (drag) // checking for nullity is needed with Clang under certain circumstances
            drag->deleteLater();
    }
    else
    { // "this" is for Wayland, when the window isn't active
        QTabBar::mouseMoveEvent (event);
        int index = tabAt (event->position().toPoint());
        if (index > -1)
            /* WARNING: For tabbars, event->globalPosition() may return a totally
                        wrong position with Qt6. */
            QToolTip::showText (QCursor::pos(), tabToolTip (index), this);
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
    dragStarted_ = false;
    dragStartPosition_ = QPoint();

    QMouseEvent finishingEvent (QEvent::MouseMove, QPointF(),
#if (QT_VERSION >= QT_VERSION_CHECK(6,4,0))
                                QCursor::pos(),
#endif
                                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    mouseMoveEvent (&finishingEvent);
}
/*************************/
void TabBar::releaseMouse()
{
    QMouseEvent releasingEvent (QEvent::MouseButtonRelease, QPointF(),
#if (QT_VERSION >= QT_VERSION_CHECK(6,4,0))
                                QCursor::pos(),
#endif
                                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
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
