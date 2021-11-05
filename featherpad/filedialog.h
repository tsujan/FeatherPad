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

#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QFileDialog>
#include <QTreeView>
#include <QTimer>
#include <QShortcut>
#include <QGuiApplication>

namespace FeatherPad {

static bool showHidden = false; // remember

/* We want to auto-scroll to selected file and
   remember whether hidden files should be shown. */
class FileDialog : public QFileDialog {
    Q_OBJECT
public:
    FileDialog (QWidget *parent, bool isNative = false) : QFileDialog (parent) {
        tView = nullptr;
        p = parent;
        native = isNative;
        setWindowModality (Qt::WindowModal);
        setViewMode (QFileDialog::Detail);
        if (!native)
        {
            setOption (QFileDialog::DontUseNativeDialog);
            if (showHidden)
                setFilter (filter() | QDir::Hidden);
            QShortcut *toggleHidden0 = new QShortcut (QKeySequence (tr ("Ctrl+H", "Toggle showing hidden files")), this);
            QShortcut *toggleHidden1 = new QShortcut (QKeySequence (tr ("Alt+.", "Toggle showing hidden files")), this);
            connect (toggleHidden0, &QShortcut::activated, this, &FileDialog::toggleHidden);
            connect (toggleHidden1, &QShortcut::activated, this, &FileDialog::toggleHidden);
        }
    }

    ~FileDialog() {
        showHidden = (filter() & QDir::Hidden);
    }

    void autoScroll() {
        if (native) return;
        tView = findChild<QTreeView *>("treeView");
        if (tView)
            connect (tView->model(), &QAbstractItemModel::layoutChanged, this, &FileDialog::scrollToSelection);
    }

protected:
    void showEvent(QShowEvent * event) {
        if (p && !native && QGuiApplication::platformName() != "wayland")
            QTimer::singleShot (0, this, &FileDialog::center);
        QFileDialog::showEvent (event);
    }

private slots:
    void scrollToSelection() {
        if (tView)
        {
            QModelIndexList iList = tView->selectionModel()->selectedIndexes();
            if (!iList.isEmpty())
                tView->scrollTo (iList.at (0), QAbstractItemView::PositionAtCenter);
        }
    }

    void center() {
        move (p->x() + p->width()/2 - width()/2,
              p->y() + p->height()/2 - height()/ 2);
    }

    void toggleHidden() {
        showHidden = !(filter() & QDir::Hidden);
        if (showHidden)
            setFilter (filter() | QDir::Hidden);
        else
            setFilter (filter() & ~QDir::Hidden);
    }

private:
    QTreeView *tView;
    QWidget *p;
    bool native;
};

}

#endif // FILEDIALOG_H
