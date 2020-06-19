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

#include <QGridLayout>
#include <QToolButton>
#include <QCompleter>
#include "searchbar.h"
#include "svgicons.h"

namespace FeatherPad {

static const int MAX_ROW_COUNT = 40;

SearchBar::SearchBar(QWidget *parent,
                     const QList<QKeySequence> &shortcuts,
                     Qt::WindowFlags f)
    : QFrame (parent, f)
{
    searchStarted_ = false;
    combo_ = new ComboBox (this);
    /* WARNING: If the minimum width is not set here, it will be set by the model,
                which can contain long texts and make the whole window very wide
                when the search history is shared. */
    combo_->setMinimumWidth (150);
    //combo_->setSizePolicy (QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    lineEdit_ = new LineEdit();
    lineEdit_->setPlaceholderText (tr ("Search..."));
    combo_->setLineEdit (lineEdit_);
    combo_->setInsertPolicy (QComboBox::NoInsert);
    combo_->setCompleter (nullptr); // disable auto-completion to keep history
    /* we add 1 to be cautiously in control of it; see searchStarted() */
    combo_->setMaxCount (MAX_ROW_COUNT + 1);

    shortcuts_ = shortcuts;
    QKeySequence nxtShortcut, prevShortcut, csShortcut, wholeShortcut, regexShortcut;
    if (shortcuts.size() >= 5)
    {
        nxtShortcut = shortcuts.at (0);
        prevShortcut = shortcuts.at (1);
        csShortcut = shortcuts.at (2);
        wholeShortcut = shortcuts.at (3);
        regexShortcut = shortcuts.at (4);
    }

    toolButton_nxt_ = new QToolButton (this);
    toolButton_prv_ = new QToolButton (this);
    toolButton_nxt_->setIcon (symbolicIcon::icon (":icons/go-down.svg"));
    toolButton_prv_->setIcon (symbolicIcon::icon (":icons/go-up.svg"));
    toolButton_nxt_->setAutoRaise (true);
    toolButton_prv_->setAutoRaise (true);
    toolButton_nxt_->setShortcut (nxtShortcut);
    toolButton_prv_->setShortcut (prevShortcut);
    toolButton_nxt_->setToolTip (tr ("Next") + " (" + nxtShortcut.toString (QKeySequence::NativeText) + ")");
    toolButton_prv_->setToolTip (tr ("Previous") + " (" + prevShortcut.toString (QKeySequence::NativeText) + ")");

    button_case_ = new QToolButton (this);
    button_case_->setIcon (symbolicIcon::icon (":icons/case.svg"));
    button_case_->setToolTip (tr ("Match Case") + " (" + csShortcut.toString (QKeySequence::NativeText) + ")");
    button_case_->setShortcut (csShortcut);
    button_case_->setCheckable (true);
    button_case_->setFocusPolicy (Qt::NoFocus);

    button_whole_ = new QToolButton (this);
    button_whole_->setIcon (symbolicIcon::icon (":icons/whole.svg"));
    button_whole_->setToolTip (tr ("Whole Word") + " (" + wholeShortcut.toString (QKeySequence::NativeText) + ")");
    button_whole_->setShortcut (wholeShortcut);
    button_whole_->setCheckable (true);
    button_whole_->setFocusPolicy (Qt::NoFocus);

    button_regex_ = new QToolButton (this);
    button_regex_->setIcon (symbolicIcon::icon (":icons/regex.svg"));
    button_regex_->setToolTip (tr ("Regular Expression") + " (" + regexShortcut.toString (QKeySequence::NativeText) + ")");
    button_regex_->setShortcut (regexShortcut);
    button_regex_->setCheckable (true);
    button_regex_->setFocusPolicy (Qt::NoFocus);

    /* there are shortcuts for forward/backward search */
    toolButton_nxt_->setFocusPolicy (Qt::NoFocus);
    toolButton_prv_->setFocusPolicy (Qt::NoFocus);

    QGridLayout *mainGrid = new QGridLayout;
    mainGrid->setHorizontalSpacing (3);
    mainGrid->setContentsMargins (2, 0, 2, 0);
    mainGrid->addWidget (combo_, 0, 0);
    mainGrid->addWidget (toolButton_nxt_, 0, 1);
    mainGrid->addWidget (toolButton_prv_, 0, 2);
    mainGrid->addItem (new QSpacerItem (6, 3), 0, 3);
    mainGrid->addWidget (button_case_, 0, 4);
    mainGrid->addWidget (button_whole_, 0, 5);
    mainGrid->addWidget (button_regex_, 0, 6);
    setLayout (mainGrid);

    connect (lineEdit_, &QLineEdit::returnPressed, this, &SearchBar::findForward);
    connect (toolButton_nxt_, &QAbstractButton::clicked, this, &SearchBar::findForward);
    connect (toolButton_prv_, &QAbstractButton::clicked, this, &SearchBar::findBackward);
    connect (button_case_, &QAbstractButton::clicked, this, &SearchBar::searchFlagChanged);
    connect (button_whole_, &QAbstractButton::clicked, [this] (bool checked) {
        button_regex_->setEnabled (!checked);
        emit searchFlagChanged();
    });
    connect (button_regex_, &QAbstractButton::clicked, [this] (bool checked) {
        button_whole_->setEnabled (!checked);
        if (checked)
            lineEdit_->setPlaceholderText (tr ("Search with regex..."));
        else
            lineEdit_->setPlaceholderText (tr ("Search..."));
        emit searchFlagChanged();
    });

    /* show the popup with Ctrl+Down/Up (because Alt+Down/Up is reserved) */
    connect (lineEdit_, &LineEdit::showComboPopup, [this] {
        combo_->showPopup();
    });
    /* the default behavior of up/down arrow key isn't good enough */
    connect (combo_, &ComboBox::moveInHistory, lineEdit_, [this] (bool up) {
        int count = combo_->count();
        if (count == 0) return;
        int index = combo_->findText (lineEdit_->text(), Qt::MatchExactly);
        if (index < 0)
            combo_->setCurrentIndex (0);
        else
        {
            if (up)
            {
                if (index > 0)
                    combo_->setCurrentIndex (index - 1);
            }
            else if (index < count - 1)
                combo_->setCurrentIndex (index + 1);
        }
    });
}
/*************************/
void SearchBar::setSearchModel (QStandardItemModel *model)
{
    if (model != nullptr)
    {
        combo_->setModel (model);

        /* Qt puts the first item of the new model's history into the line-edit but
           that can be confusing (we don't use clear() because we don't want undoing). */
        lineEdit_->setText (QString());

        /* If the history is changed elsewhere, the line-edit's text might change but that
           could be confusing. So, we restore the text (unfortunately, undo/redo will be reset). */
        connect (combo_->model(), &QAbstractItemModel::rowsAboutToBeRemoved, lineEdit_, [this] (const QModelIndex&, int, int) {
            if (!searchStarted_)
                searchText_ = lineEdit_->text();
        });
        connect (combo_->model(), &QAbstractItemModel::rowsRemoved, lineEdit_, [this] (const QModelIndex&, int, int) {
            /* may also happen when our maximum row count (= 40) has been reached */
            if (!searchStarted_ && lineEdit_->text() != searchText_)
                lineEdit_->setText (searchText_);
            searchText_.clear();
        });
        connect (combo_->model(), &QAbstractItemModel::rowsAboutToBeInserted, lineEdit_, [this] (const QModelIndex&, int, int) {
            if (!searchStarted_)
                searchText_ = lineEdit_->text();
        });
        connect (combo_->model(), &QAbstractItemModel::rowsInserted, lineEdit_, [this] (const QModelIndex&, int, int) {
            if (!searchStarted_ && lineEdit_->text() != searchText_)
                lineEdit_->setText (searchText_);
            searchText_.clear();
        });
    }
}
/*************************/
void SearchBar::searchStarted()
{
    searchStarted_ = true;
    const QString txt = lineEdit_->text();
    if (txt.isEmpty()) return;
    int index = combo_->findText (txt, Qt::MatchExactly);
    if (index != 0)
    {
        if (index > 0)
            combo_->removeItem (index);
        /* we set the maximum row count ourself while searchStarted_ is true (see setSearchModel()) */
        else if (combo_->count() == MAX_ROW_COUNT)
            combo_->removeItem (MAX_ROW_COUNT - 1);
        combo_->insertItem (0, txt);
    }
    combo_->setCurrentIndex (0);
    searchStarted_ = false;
}
/*************************/
void SearchBar::focusLineEdit()
{
    lineEdit_->setFocus();
    lineEdit_->selectAll();
}
/*************************/
bool SearchBar::lineEditHasFocus() const
{
    return lineEdit_->hasFocus();
}
/*************************/
QString SearchBar::searchEntry() const
{
    return lineEdit_->text();
}
/*************************/
void SearchBar::clearSearchEntry()
{
    lineEdit_->clear(); // doesn't remove the undo/redo history
}
/*************************/
void SearchBar::findForward()
{
    searchStarted();
    emit find (true);
}
/*************************/
void SearchBar::findBackward()
{
    searchStarted();
    emit find (false);
}
/*************************/
bool SearchBar::matchCase() const
{
    return button_case_->isChecked();
}
/*************************/
bool SearchBar::matchWhole() const
{
    return button_whole_->isChecked();
}
/*************************/
bool SearchBar::matchRegex() const
{
    return button_regex_->isChecked();
}
/*************************/
// Used only in a workaround (-> FPwin::updateShortcuts())
void SearchBar::updateShortcuts (bool disable)
{
    if (disable)
    {
        toolButton_nxt_->setShortcut (QKeySequence());
        toolButton_prv_->setShortcut (QKeySequence());
        button_case_->setShortcut (QKeySequence());
        button_whole_->setShortcut (QKeySequence());
        button_regex_->setShortcut (QKeySequence());
    }
    else if (shortcuts_.size() >= 5)
    {
        toolButton_nxt_->setShortcut (shortcuts_.at (0));
        toolButton_prv_->setShortcut (shortcuts_.at (1));
        button_case_->setShortcut (shortcuts_.at (2));
        button_whole_->setShortcut (shortcuts_.at (3));
        button_regex_->setShortcut (shortcuts_.at (4));
    }
}

void SearchBar::requestForSearch(QString text)
{
	lineEdit_->setText(text);
}

}
