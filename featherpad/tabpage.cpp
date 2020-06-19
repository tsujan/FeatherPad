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
#include "tabpage.h"

namespace FeatherPad {

TabPage::TabPage (int bgColorValue,
                  const QList<QKeySequence> &searchShortcuts,
                  QWidget *parent)
    : QWidget (parent)
{
    textEdit_ = new TextEdit (this, bgColorValue);
    searchBar_ = new SearchBar (this, searchShortcuts);

    QGridLayout *mainGrid = new QGridLayout;
    mainGrid->setVerticalSpacing (4);
    mainGrid->setContentsMargins (0, 0, 0, 0);
    mainGrid->addWidget (textEdit_, 0, 0);
    mainGrid->addWidget (searchBar_, 1, 0);
    setLayout (mainGrid);

    connect (searchBar_, &SearchBar::find, this, &TabPage::find);
    connect (searchBar_, &SearchBar::searchFlagChanged, this, &TabPage::searchFlagChanged);

    connect (textEdit_, &TextEdit::triggerHighlight, searchBar_, &SearchBar::requestForSearch);

}
/*************************/
void TabPage::setSearchBarVisible (bool visible)
{
    searchBar_->setVisible (visible);
}
/*************************/
bool TabPage::isSearchBarVisible() const
{
    return searchBar_->isVisible();
}
/*************************/
void TabPage::focusSearchBar()
{
    searchBar_->focusLineEdit();
}
/*************************/
bool TabPage::searchBarHasFocus() const
{
    return searchBar_->lineEditHasFocus();
}
/*************************/
QString TabPage::searchEntry() const
{
    return searchBar_->searchEntry();
}
/*************************/
void TabPage::clearSearchEntry()
{
    return searchBar_->clearSearchEntry();
}
/*************************/
bool TabPage::matchCase() const
{
    return searchBar_->matchCase();
}
/*************************/
bool TabPage::matchWhole() const
{
    return searchBar_->matchWhole();
}
/*************************/
bool TabPage::matchRegex() const
{
    return searchBar_->matchRegex();
}
/*************************/
void TabPage::updateShortcuts (bool disable)
{
    searchBar_->updateShortcuts (disable);
}

}
