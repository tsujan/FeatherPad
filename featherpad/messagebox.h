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

#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <QMessageBox>
#include <QAbstractButton>
#include <QGridLayout>
#include <QRegularExpression>

#include <algorithm>

namespace FeatherPad {

/* QMessageBox::setButtonText() is obsolete while we want custom texts, especially
   for translation. We also want to set the width according to the informative text. */
class MessageBox : public QMessageBox {
    Q_OBJECT
public:
    MessageBox (QWidget *parent = nullptr) : QMessageBox (parent) {}
    MessageBox (Icon icon, const QString &title,
                const QString &text,
                StandardButtons buttons = NoButton,
                QWidget *parent = nullptr,
                Qt::WindowFlags f = Qt::Dialog/* | Qt::MSWindowsFixedSizeDialogHint*/) : QMessageBox (icon, title, text, buttons, parent, f) {}

    void changeButtonText (QMessageBox::StandardButton btn, const QString &text) {
        if (QAbstractButton *abtn = button (btn))
            abtn->setText (text);
    }

    void setInformativeText (const QString &text) {
        QMessageBox::setInformativeText (text);

        if (!text.isEmpty()) // set an appropriate width
        {
            if (QGridLayout *lo = qobject_cast< QGridLayout *>(layout()))
            {
                int tw = 0;
                QString t (text);
                /* we suppose that <p> isn't used inside the text */
                t.remove (QRegularExpression ("</{0,1}center>|</{0,1}b>|</{0,1}i>|</{0,1}p>|</a>|<a\\s+href\\s*=\\s*[A-Za-z0-9_%@:'\\.\\?\\=]+>"));
                t.replace ("<br>", "\n");
                QStringList l = t.split ('\n'); // deal with newlines
                for (int i = 0; i < l.size(); i++)
                    tw = std::max (tw, QFontMetrics (font()).horizontalAdvance (l[i]));

                lo->setColumnMinimumWidth (lo->columnCount() - 1, tw + 10);
            }
        }
    }
};

}

#endif // MESSAGEBOX_H
