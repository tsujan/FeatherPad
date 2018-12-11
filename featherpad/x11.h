/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014 <tsujan2000@gmail.com>
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

#ifndef X11_H
#define X11_H

#if defined Q_WS_X11 || defined Q_OS_LINUX || defined Q_OS_FREEBSD || Q_OS_OPENBSD
#include <X11/Xlib.h>
#endif

namespace FeatherPad {

long fromDesktop();
long onWhichDesktop (Window window);
bool isWindowShaded (Window window);
void unshadeWindow (Window window);

}

#endif // X11_H
