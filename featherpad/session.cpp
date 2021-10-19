/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2020 <tsujan2000@gmail.com>
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

#include "singleton.h"
#include "ui_fp.h"

#include "session.h"
#include "ui_sessionDialog.h"
#include <QFileInfo>

namespace FeatherPad {

// Since we don't want extra prompt dialogs, we make the
// session dialog behave like a prompt dialog when needed.
SessionDialog::SessionDialog (QWidget *parent):QDialog (parent), ui (new Ui::SessionDialog)
{
    ui->setupUi (this);
    parent_ = parent;
    setObjectName ("sessionDialog");
    ui->promptLabel->setStyleSheet ("QLabel {background-color: #7d0000; color: white; border-radius: 3px; margin: 2px; padding: 5px;}");
    ui->listWidget->setSizeAdjustPolicy (QAbstractScrollArea::AdjustToContents);
    ui->listWidget->setContextMenuPolicy (Qt::CustomContextMenu);
    filterTimer_ = nullptr;

    connect (ui->listWidget, &QListWidget::itemDoubleClicked, [=]{openSessions();});
    connect (ui->listWidget, &QListWidget::itemSelectionChanged, this, &SessionDialog::selectionChanged);
    connect (ui->listWidget, &QWidget::customContextMenuRequested, this, &SessionDialog::showContextMenu);
    connect (ui->listWidget->itemDelegate(), &QAbstractItemDelegate::commitData, this, &SessionDialog::OnCommittingName);

    QSettings settings ("featherpad", "fp");
    settings.beginGroup ("sessions");
    allItems_ = settings.allKeys();
    settings.endGroup();
    if (allItems_.count() > 0)
    {
        /* use ListWidgetItem to add items with a natural sorting */
        for (const auto &item : qAsConst (allItems_))
        {
            ListWidgetItem *lwi = new ListWidgetItem (item, ui->listWidget);
            ui->listWidget->addItem (lwi);
        }
        ui->listWidget->setCurrentRow (0);
        QTimer::singleShot (0, ui->listWidget, QOverload<>::of(&QWidget::setFocus));
    }
    else
    {
        onEmptinessChanged (true);
        QTimer::singleShot (0, ui->lineEdit, QOverload<>::of(&QWidget::setFocus));
    }

    ui->listWidget->installEventFilter (this);

    connect (ui->saveBtn, &QAbstractButton::clicked, this, &SessionDialog::saveSession);
    connect (ui->lineEdit, &QLineEdit::returnPressed, this, &SessionDialog::saveSession);
    /* we don't want to open a session by pressing Enter inside the line-edit */
    connect (ui->lineEdit, &LineEdit::receivedFocus, [=]{ui->openBtn->setDefault (false);});
    connect (ui->lineEdit, &QLineEdit::textEdited, [=](const QString &text){ui->saveBtn->setEnabled (!text.isEmpty());});
    connect (ui->openBtn, &QAbstractButton::clicked, this, &SessionDialog::openSessions);
    connect (ui->actionOpen, &QAction::triggered, this, &SessionDialog::openSessions);
    connect (ui->clearBtn, &QAbstractButton::clicked, [=]{showPrompt (CLEAR);});
    connect (ui->removeBtn, &QAbstractButton::clicked, [=]{showPrompt (REMOVE);});
    connect (ui->actionRemove, &QAction::triggered, [=]{showPrompt (REMOVE);});

    connect (ui->actionRename, &QAction::triggered, this, &SessionDialog::renameSession);

    connect (ui->cancelBtn, &QAbstractButton::clicked, this, &SessionDialog::closePrompt);
    connect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::closePrompt);

    connect (ui->closeButton, &QAbstractButton::clicked, this, &QDialog::close);

    connect (ui->filterLineEdit, &QLineEdit::textChanged, this, &SessionDialog::filter);

    /* for the tooltip mess in Qt 5.12 */
    const auto widgets = findChildren<QWidget*>();
    for (QWidget *w : widgets)
    {
        QString tip = w->toolTip();
        if (!tip.isEmpty())
        {
            w->setToolTip ("<p style='white-space:pre'>" + tip + "</p>");
        }
    }

    resize (QSize (parent_->size().width()/2, 3*parent_->size().height()/4));
}
/*************************/
SessionDialog::~SessionDialog()
{
    if (filterTimer_)
    {
        disconnect (filterTimer_, &QTimer::timeout, this, &SessionDialog::reallyApplyFilter);
        filterTimer_->stop();
        delete filterTimer_;
    }
    delete ui; ui = nullptr;
}
/*************************/
bool SessionDialog::eventFilter (QObject *watched, QEvent *event)
{
    if (watched == ui->listWidget && event->type() == QEvent::KeyPress)
    { // when a text is typed inside the list, type it inside the filter line-edit too
        if (QKeyEvent *ke = static_cast<QKeyEvent*>(event))
        {
            ui->filterLineEdit->pressKey (ke);
            return false;
        }
    }
    return QDialog::eventFilter (watched, event);
}
/*************************/
void SessionDialog::showContextMenu (const QPoint &p)
{
    QModelIndex index = ui->listWidget->indexAt (p);
    if (!index.isValid()) return;
    ui->listWidget->selectionModel()->select (index, QItemSelectionModel::ClearAndSelect);

    QMenu menu;
    menu.addAction (ui->actionOpen);
    menu.addAction (ui->actionRemove);
    menu.addSeparator();
    menu.addAction (ui->actionRename);
    menu.exec (ui->listWidget->mapToGlobal (p));
}
/*************************/
void SessionDialog::saveSession()
{
    if (ui->lineEdit->text().isEmpty()) return;

    bool hasFile (false);
    if (ui->windowBox->isChecked())
    {
        FPwin *win = static_cast<FPwin *>(parent_);
        for (int i = 0; i < win->ui->tabWidget->count(); ++i)
        {
            if (!qobject_cast<TabPage*>(win->ui->tabWidget->widget (i))
                ->textEdit()->getFileName().isEmpty())
            {
                hasFile = true;
                break;
            }
        }
    }
    else
    {
        FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *win = singleton->Wins.at (i);
            for (int j = 0; j < win->ui->tabWidget->count(); ++j)
            {
                if (!qobject_cast<TabPage*>(win->ui->tabWidget->widget (j))
                    ->textEdit()->getFileName().isEmpty())
                {
                    hasFile = true;
                    break;
                }
            }
            if (hasFile) break;
        }
    }

    if (!hasFile)
        showPrompt (tr ("Nothing saved.<br>No file was opened."));
    else if (allItems_.contains (ui->lineEdit->text()))
        showPrompt (NAME);
    else
        reallySaveSession();
}
/*************************/
void SessionDialog::reallySaveSession()
{
    QList<QListWidgetItem*> sameItems = ui->listWidget->findItems (ui->lineEdit->text(), Qt::MatchExactly);
    for (int i = 0; i < sameItems.count(); ++i)
        delete ui->listWidget->takeItem (ui->listWidget->row (sameItems.at (i)));

    QStringList files;
    if (ui->windowBox->isChecked())
    {
        FPwin *win = static_cast<FPwin *>(parent_);
        for (int i = 0; i < win->ui->tabWidget->count(); ++i)
        {
            TextEdit *textEdit = qobject_cast<TabPage*>(win->ui->tabWidget->widget (i))->textEdit();
            if (!textEdit->getFileName().isEmpty())
            {
                files << textEdit->getFileName();
                textEdit->setSaveCursor (true);
            }
        }
    }
    else
    {
        FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *win = singleton->Wins.at (i);
            for (int j = 0; j < win->ui->tabWidget->count(); ++j)
            {
                TextEdit *textEdit = qobject_cast<TabPage*>(win->ui->tabWidget->widget (j))->textEdit();
                if (!textEdit->getFileName().isEmpty())
                {
                    files << textEdit->getFileName();
                    textEdit->setSaveCursor (true);
                }
            }
        }
    }
    /* there's always an opened file here */
    allItems_ << ui->lineEdit->text();
    allItems_.removeDuplicates();
    QRegularExpression exp (ui->filterLineEdit->text(), QRegularExpression::CaseInsensitiveOption);
    if (allItems_.filter (exp).contains (ui->lineEdit->text()))
    {
        ListWidgetItem *lwi = new ListWidgetItem (ui->lineEdit->text(), ui->listWidget);
        ui->listWidget->addItem (lwi);
    }
    onEmptinessChanged (false);
    QSettings settings ("featherpad", "fp");
    settings.beginGroup ("sessions");
    settings.setValue (ui->lineEdit->text(), files);
    settings.endGroup();
}
/*************************/
void SessionDialog::openSessions()
{
    QList<QListWidgetItem*> items = ui->listWidget->selectedItems();
    int count = items.count();
    if (count == 0) return;

    QSettings settings ("featherpad", "fp");
    settings.beginGroup ("sessions");
    QStringList files;
    for (int i = 0; i < count; ++i)
        files += settings.value (items.at (i)->text()).toStringList();
    settings.endGroup();

    if (!files.isEmpty())
    {
        if (FPwin *win = static_cast<FPwin *>(parent_))
        {
            Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
            int broken = 0;
            bool multiple (files.count() > 1 || win->isLoading());
            for (int i = 0; i < files.count(); ++i)
            {
                if (!QFileInfo (files.at (i)).isFile())
                {
                    /* first, clean up the cursor config file */
                    config.removeCursorPos (files.at (i));

                    ++broken;
                    continue;
                }
                win->newTabFromName (files.at (i),
                                     1, // to save the cursor position
                                     0, // irrelevant
                                     multiple);
            }
            if (broken > 0)
            {
                if (broken == files.count())
                    showPrompt (tr ("No file exists or can be opened."));
                else
                    showPrompt (tr ("Not all files exist or can be opened."));
            }
        }
    }
}
/*************************/
// These slots are called for processes to have time to be completed,
// especially for the returnPressed signal of the line-edit to be emiited.
void SessionDialog::showMainPage()
{
    if (!rename_.newName.isEmpty() && !rename_.oldName.isEmpty()) // renaming cancelled
    {
        if (QListWidgetItem* cur = ui->listWidget->currentItem())
            cur->setText (rename_.oldName);
        rename_.newName = rename_.oldName = QString();
    }
    ui->stackedWidget->setCurrentIndex (0);
}
void SessionDialog::showPromptPage()
{
    ui->stackedWidget->setCurrentIndex (1);
    QTimer::singleShot (0, ui->confirmBtn, QOverload<>::of(&QWidget::setFocus));
}
/*************************/
void SessionDialog::showPrompt (const QString& message)
{
    disconnect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeAll);
    disconnect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeSelected);
    disconnect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallySaveSession);
    disconnect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallyRenameSession);

    if (message.isEmpty()) return;

    QTimer::singleShot (0, this, &SessionDialog::showPromptPage);

    ui->confirmBtn->setText (tr ("&OK"));
    ui->cancelBtn->setVisible (false);
    ui->promptLabel->setText ("<b>" + message + "</b>");
}
/*************************/
void SessionDialog::showPrompt (PROMPT prompt)
{
    disconnect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeAll);
    disconnect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeSelected);
    disconnect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallySaveSession);
    disconnect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallyRenameSession);

    QTimer::singleShot (0, this, &SessionDialog::showPromptPage);

    ui->confirmBtn->setText (tr ("&Yes"));
    ui->cancelBtn->setVisible (true);

    if (prompt == CLEAR)
    {
        ui->promptLabel->setText ("<b>" + tr ("Do you really want to remove all saved sessions?") + "</b>");
        connect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeAll);
    }
    else if (prompt == REMOVE)
    {
        if (ui->listWidget->selectedItems().count() > 1)
            ui->promptLabel->setText ("<b>" + tr ("Do you really want to remove the selected sessions?") + "</b>");
        else
            ui->promptLabel->setText ("<b>" + tr ("Do you really want to remove the selected session?") + "</b>");
        connect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::removeSelected);
    }
    else// if (prompt == NAME || prompt == RENAME)
    {
        ui->promptLabel->setText ("<b>" + tr ("A session with the same name exists.<br>Do you want to overwrite it?") + "</b>");
        if (prompt == NAME)
            connect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallySaveSession);
        else
            connect (ui->confirmBtn, &QAbstractButton::clicked, this, &SessionDialog::reallyRenameSession);
    }
}
/*************************/
void SessionDialog::closePrompt()
{
    ui->promptLabel->clear();
    QTimer::singleShot (0, this, &SessionDialog::showMainPage);
}
/*************************/
void SessionDialog::removeSelected()
{
    QList<QListWidgetItem*> items = ui->listWidget->selectedItems();
    int count = items.count();
    if (count == 0) return;

    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    QSettings settings ("featherpad", "fp");
    settings.beginGroup ("sessions");
    for (int i = 0; i < count; ++i)
    {
        /* first, clean up the cursor config file */
        QStringList files = settings.value (items.at (i)->text()).toStringList();
        for (int j = 0; j < files.count(); ++j)
            config.removeCursorPos (files.at (j));

        settings.remove (items.at (i)->text());
        allItems_.removeOne (items.at (i)->text());
        delete ui->listWidget->takeItem (ui->listWidget->row (items.at (i)));
    }
    settings.endGroup();

    if (config.savedCursorPos().isEmpty())
    {
        Settings curSettings ("featherpad", "fp_cursor_pos");
        curSettings.remove ("cursorPositions");
    }

    if (allItems_.count() == 0)
        onEmptinessChanged (true);
}
/*************************/
void SessionDialog::removeAll()
{
    /* first, clean up the cursor config file */
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.removeAllCursorPos();
    Settings curSettings ("featherpad", "fp_cursor_pos");
    curSettings.remove ("cursorPositions");

    ui->listWidget->clear();
    onEmptinessChanged (true);
    QSettings settings ("featherpad", "fp");
    settings.beginGroup ("sessions");
    settings.remove ("");
    settings.endGroup();
}
/*************************/
void SessionDialog::selectionChanged()
{
    bool noSel = ui->listWidget->selectedItems().isEmpty();
    ui->openBtn->setEnabled (!noSel);
    ui->removeBtn->setEnabled (!noSel);
    /* we want to open sessions by pressing Enter inside the list widget
       without connecting to QAbstractItemView::activated() */
    ui->openBtn->setDefault (true);
}
/*************************/
void SessionDialog::renameSession()
{
    if (QListWidgetItem* cur = ui->listWidget->currentItem())
    {
        rename_.oldName = cur->text();
        cur->setFlags (cur->flags() | Qt::ItemIsEditable);
        ui->listWidget->editItem (cur);
    }
}
/*************************/
void SessionDialog::OnCommittingName (QWidget* editor)
{
    if (QListWidgetItem* cur = ui->listWidget->currentItem())
        cur->setFlags (cur->flags() & ~Qt::ItemIsEditable);

    rename_.newName = reinterpret_cast<QLineEdit*>(editor)->text();
    if (rename_.newName.isEmpty()
        || rename_.newName == rename_.oldName)
    {
        rename_.newName = rename_.oldName = QString(); // reset
        return;
    }

    if (allItems_.contains (rename_.newName))
        showPrompt (RENAME);
    else
        reallyRenameSession();
}
/*************************/
void SessionDialog::reallyRenameSession()
{
    if (rename_.newName.isEmpty() || rename_.oldName.isEmpty()) // impossible
    {
        rename_.newName = rename_.oldName = QString();
        return;
    }

    QSettings settings ("featherpad", "fp");
    settings.beginGroup ("sessions");
    QStringList files = settings.value (rename_.oldName).toStringList();
    settings.remove (rename_.oldName);
    settings.setValue (rename_.newName, files);
    settings.endGroup();

    allItems_.removeOne (rename_.oldName);
    allItems_ << rename_.newName;
    allItems_.removeDuplicates();

    if (QListWidgetItem* cur = ui->listWidget->currentItem())
    {
        bool isFiltered (false);
        /* if the renamed item is filtered, remove it
           with all items that have the same name */
        if (!ui->filterLineEdit->text().isEmpty())
        {
            QRegularExpression exp (ui->filterLineEdit->text(), QRegularExpression::CaseInsensitiveOption);
            if (!allItems_.filter (exp).contains (rename_.newName))
                isFiltered = true;
        }
        /* if there's another item with the new name, remove it */
        QList<QListWidgetItem*> sameItems = ui->listWidget->findItems (rename_.newName, Qt::MatchExactly);
        for (int i = 0; i < sameItems.count(); ++i)
        {
            if (isFiltered || sameItems.at (i) != cur)
                delete ui->listWidget->takeItem (ui->listWidget->row (sameItems.at (i)));
        }
        if (!isFiltered)
            ui->listWidget->scrollToItem (cur);
    }

    rename_.newName = rename_.oldName = QString(); // reset
}
/*************************/
void SessionDialog::filter (const QString&/*text*/)
{
    if (!filterTimer_)
    {
        filterTimer_ = new QTimer();
        filterTimer_->setSingleShot (true);
        connect (filterTimer_, &QTimer::timeout, this, &SessionDialog::reallyApplyFilter);
    }
    filterTimer_->start (200);
}
/*************************/
void SessionDialog::reallyApplyFilter()
{
    /* first, get the selection */
    QStringList sel;
    QList<QListWidgetItem*> items = ui->listWidget->selectedItems();
    for (int i = 0; i < items.count(); ++i)
        sel << items.at (i)->text();
    /* then, clear the current list and add the filtered one */
    ui->listWidget->clear();
    QRegularExpression exp (ui->filterLineEdit->text(), QRegularExpression::CaseInsensitiveOption);
    const QStringList filtered = allItems_.filter (exp);
    for (const auto &item : filtered)
    {
        ListWidgetItem *lwi = new ListWidgetItem (item, ui->listWidget);
        ui->listWidget->addItem (lwi);
    }
    /* finally, restore the selection as far as possible */
    if (filtered.count() == 1)
        ui->listWidget->setCurrentRow (0);
    else if (!sel.isEmpty())
    {
        for (int i = 0; i < ui->listWidget->count(); ++i)
        {
            if (sel.contains (ui->listWidget->item (i)->text()))
                ui->listWidget->setCurrentRow (i, QItemSelectionModel::Select);
        }
    }
}
/*************************/
void SessionDialog::onEmptinessChanged (bool empty)
{
    ui->clearBtn->setEnabled (!empty);
    if (empty)
    {
        allItems_.clear();
        ui->filterLineEdit->clear();
    }
    ui->filterLineEdit->setEnabled (!empty);
}

}
