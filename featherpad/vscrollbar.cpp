/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2021 <tsujan2000@gmail.com>
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

#include "vscrollbar.h"

namespace FeatherPad {

VScrollBar::VScrollBar (QWidget *parent) : QScrollBar (parent) {}
/*************************/
void VScrollBar::wheelEvent (QWheelEvent *event)
{
    if (!underMouse()
        || !event->spontaneous()
        || event->source() != Qt::MouseEventNotSynthesized
        /* Apparently, Qt's hover bug is never going to be fixed! */
        || !rect().contains (mapFromGlobal (QCursor::pos())))
    {
        QScrollBar::wheelEvent (event);
        return;
    }
    QPoint anglePoint = event->angleDelta();
    int delta = qAbs (anglePoint.x()) > qAbs (anglePoint.y()) ? anglePoint.x()
                                                              : anglePoint.y();

    /* wait until the angle delta reaches that of an ordinary mouse wheel */
    static int _effectiveDelta = 0;
    _effectiveDelta += delta;
    if (qAbs (_effectiveDelta) < 120)
        return;

    int step = (_effectiveDelta < 0 ? 1 : -1) * qMax (pageStep() / ((event->modifiers() & Qt::ShiftModifier) ? 2 : 1), 1);
    _effectiveDelta = 0;
    setSliderPosition (sliderPosition() + step);
}

}
