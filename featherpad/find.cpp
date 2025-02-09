/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2022 <tsujan2000@gmail.com>
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

#include "fpwin.h"
#include "ui_fp.h"
#include <QTextDocumentFragment>

namespace FeatherPad {

/* This order is preserved everywhere for selections:
   current line -> replacement -> found matches -> selection highlights -> column highlight -> bracket matches */

void FPwin::find (bool forward)
{
    if (!isReady()) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    TextEdit *textEdit = tabPage->textEdit();
    QString txt = tabPage->searchEntry();
    bool newSrch = false;
    if (textEdit->getSearchedText() != txt)
    {
        textEdit->setSearchedText (txt);
        newSrch = true;
    }

    disconnect (textEdit, &TextEdit::resized, this, &FPwin::hlight);
    disconnect (textEdit, &TextEdit::updateRect, this, &FPwin::hlight);
    disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);

    if (txt.isEmpty())
    {
        /* remove all yellow and green highlights */
        QList<QTextEdit::ExtraSelection> es;
        textEdit->setGreenSel (es); // not needed
        if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
            es.prepend (textEdit->currentLineSelection());
        es.append (textEdit->getBlueSel());
        es.append (textEdit->getColSel());
        es.append (textEdit->getRedSel());
        textEdit->setExtraSelections (es);
        return;
    }

    QTextDocument::FindFlags searchFlags = getSearchFlags();
    QTextDocument::FindFlags newFlags = searchFlags;
    if (!forward)
        newFlags = searchFlags | QTextDocument::FindBackward;
    QTextCursor start = textEdit->textCursor();
    QTextCursor found = textEdit->finding (txt, start, newFlags, tabPage->matchRegex());

    if (found.isNull())
    {
        if (!forward)
            start.movePosition (QTextCursor::End, QTextCursor::MoveAnchor);
        else
            start.movePosition (QTextCursor::Start, QTextCursor::MoveAnchor);
        found = textEdit->finding (txt, start, newFlags, tabPage->matchRegex());
    }

    if (!found.isNull())
    {
        start.setPosition (found.anchor());
        /* this is needed for selectionChanged() to be emitted */
        if (newSrch) textEdit->setTextCursor (start);
        start.setPosition (found.position(), QTextCursor::KeepAnchor);
        textEdit->skipSelectionHighlighting();
        textEdit->setTextCursor (start);
    }
    /* matches highlights should come here, after the text area is
       scrolled and even when no match is found (it may be added later) */
    hlight();
    connect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
    connect (textEdit, &TextEdit::updateRect, this, &FPwin::hlight);
    connect (textEdit, &TextEdit::resized, this, &FPwin::hlight);
}
/*************************/
// Highlight found matches in the visible part of the text.
void FPwin::hlight() const
{
    /* When FeatherPad's window is being closed, it's possible that, in a moment,
       the current index is positive but the current widget is null. So, the latter
       should be checked, not the former. */
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    TextEdit *textEdit = tabPage->textEdit();

    const QString txt = textEdit->getSearchedText();
    if (txt.isEmpty()) return;

    /* prepend green highlights */
    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();

    /* first put the start cursor at the top left corner... */
    QPoint Point (0, 0);
    QTextCursor start = textEdit->cursorForPosition (Point);
    /* ... then move it backward by the search text length */
    int startPos = start.position() - (!tabPage->matchRegex() ? txt.length() : 0);
    if (startPos >= 0)
        start.setPosition (startPos);
    else
        start.setPosition (0);

    /* put the end cursor at the bottom right corner... */
    Point = QPoint (textEdit->geometry().width(), textEdit->geometry().height());
    QTextCursor end = textEdit->cursorForPosition (Point);
    int endLimit = end.anchor();
    /* ... and move it forward by the text length */
    int endPos = end.position() + (!tabPage->matchRegex() ? txt.length() : 0);
    end.movePosition (QTextCursor::End);
    if (endPos <= end.position())
        end.setPosition (endPos);

    /* don't waste time if the searched text is larger that the available space */
    if (tabPage->matchRegex() || end.position() - start.position() >= txt.length())
    {
        QColor color = QColor (textEdit->hasDarkScheme()
                                   ? QColor (255, 255, 0,
                                             /* a quadratic equation for darkValue -> opacity: 0 -> 90,  27 -> 75, 50 -> 65 */
                                             static_cast<int>(static_cast<double>(textEdit->getDarkValue() * (textEdit->getDarkValue() - 257)) / 414) + 90)
                                   : Qt::yellow);
        QTextDocument::FindFlags searchFlags = getSearchFlags();
        QTextCursor found;
        while (!(found = textEdit->finding (txt, start, searchFlags, tabPage->matchRegex(), endLimit)).isNull())
        {
            QTextEdit::ExtraSelection extra;
            extra.format.setBackground (color);
            extra.cursor = found;
            es.append (extra);
            start.setPosition (found.position());
        }
    }

    /* also prepend the current line highlight,
       so that it always comes first when it exists */
    if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
        es.prepend (textEdit->currentLineSelection());
    /* append blue and red highlights */
    es.append (textEdit->getBlueSel());
    es.append (textEdit->getColSel());
    es.append (textEdit->getRedSel());
    textEdit->setExtraSelections (es);
}
/*************************/
void FPwin::searchFlagChanged()
{
    if (!isReady()) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    /* deselect text for consistency */
    TextEdit *textEdit = tabPage->textEdit();
    QTextCursor start = textEdit->textCursor();
    if (start.hasSelection())
    {
        start.setPosition (start.anchor());
        textEdit->setTextCursor (start);
    }

    hlight();
}
/*************************/
QTextDocument::FindFlags FPwin::getSearchFlags() const
{
    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    QTextDocument::FindFlags searchFlags = QTextDocument::FindFlags();
    if (tabPage != nullptr)
    {
        if (tabPage->matchWhole())
            searchFlags = QTextDocument::FindWholeWords;
        if (tabPage->matchCase())
            searchFlags |= QTextDocument::FindCaseSensitively;
    }
    return searchFlags;
}

}
