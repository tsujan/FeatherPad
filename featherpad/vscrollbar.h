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

#ifndef VSCROLLBAR_H
#define VSCROLLBAR_H

#include <QScrollBar>
#include <QWheelEvent>

namespace FeatherPad {

/* We want faster mouse wheel scrolling
   when the mouse cursor is on the scrollbar. */
class VScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    VScrollBar (QWidget *parent = nullptr);

protected:
    void wheelEvent (QWheelEvent *event) override;
};

}

#endif // VSCROLLBAR_H
