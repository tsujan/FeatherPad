/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2024 <tsujan2000@gmail.com>
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

namespace FeatherPad {

void FPwin::removeGreenSel()
{
    /* remove green highlights, considering the selection order, namely,
       current line -> replacement -> found matches -> selection highlights -> column highlight -> bracket matches */
    int count = ui->tabWidget->count();
    for (int i = 0; i < count; ++i)
    {
        TextEdit *textEdit = qobject_cast< TabPage *>(ui->tabWidget->widget (i))->textEdit();
        QList<QTextEdit::ExtraSelection> es = textEdit->extraSelections();
        if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
        {
            if (!es.isEmpty())
                es.removeFirst();
        }
        int n = textEdit->getGreenSel().count();
        while (n > 0 && !es.isEmpty())
        {
            es.removeFirst();
            --n;
        }
        if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
            es.prepend (textEdit->currentLineSelection());
        textEdit->setGreenSel (QList<QTextEdit::ExtraSelection>());
        textEdit->setExtraSelections (es);
    }
}
/*************************/
void FPwin::replaceDock()
{
    if (!isReady()) return;

    if (!ui->dockReplace->isVisible())
    {
        int count = ui->tabWidget->count();
        for (int i = 0; i < count; ++i) // replace dock needs searchbar
            qobject_cast< TabPage *>(ui->tabWidget->widget (i))->setSearchBarVisible (true);
        ui->dockReplace->setWindowTitle (tr ("Replacement"));
        ui->dockReplace->setVisible (true);
        ui->dockReplace->raise();
        ui->dockReplace->activateWindow();
        if (!ui->lineEditFind->hasFocus())
            ui->lineEditFind->setFocus();
        return;
    }

    ui->dockReplace->setVisible (false);
    // dockVisibilityChanged(false) is automatically called here
}
/*************************/
// When the dock becomes invisible, clear the replacing text and remove only green highlights.
// Although it doesn't concern us, when docking or undocking, the widget first becomes invisible
// for a moment and then visible again.
void FPwin::dockVisibilityChanged (bool visible)
{
    if (visible || isMinimized()) return;

    removeGreenSel();

    /* return focus to the document and remove the title
       (other titles will be removed by tabSwitch()) */
    if (ui->tabWidget->count() > 0)
    {
        TextEdit *textEdit = qobject_cast< TabPage *>(ui->tabWidget->currentWidget())->textEdit();
        textEdit->setFocus();
        textEdit->setReplaceTitle (QString());
    }
}
/*************************/
// Resize the floating dock widget to its minimum size.
void FPwin::resizeDock (bool topLevel)
{
    if (topLevel)
        ui->dockReplace->resize (ui->dockReplace->minimumWidth(),
                                 ui->dockReplace->minimumHeight());
}
/*************************/
void FPwin::replace()
{
    if (!isReady()) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    TextEdit *textEdit = tabPage->textEdit();
    if (textEdit->isReadOnly()) return;

    textEdit->setReplaceTitle (QString());
    ui->dockReplace->setWindowTitle (tr ("Replacement"));

    QString txtFind = ui->lineEditFind->text();
    if (txtFind.isEmpty()) return;

    const QString txtReplace = ui->lineEditReplace->text();

    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();
    /* remove previous green highlights if the replacing text is changed */
    if (!es.isEmpty() && es.first().cursor.selectedText() != txtReplace)
    {
        textEdit->setGreenSel (QList<QTextEdit::ExtraSelection>());
        es.clear();
    }

    bool lineNumShown (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible());

    QTextDocument::FindFlags searchFlags = getSearchFlags();

    /* for covering regex capturing groups */
    QString realTxtReplace;
    QRegularExpression regexFind;
    if (tabPage->matchRegex())
    {
        regexFind = QRegularExpression (txtFind, (searchFlags & QTextDocument::FindCaseSensitively)
                                                    ? QRegularExpression::NoPatternOption
                                                    : QRegularExpression::CaseInsensitiveOption);
    }

    QTextCursor start = textEdit->textCursor();
    QTextCursor tmp = start;
    QTextCursor found;
    if (QObject::sender() == ui->toolButtonNext)
        found = textEdit->finding (txtFind, start, searchFlags, tabPage->matchRegex());
    else// if (QObject::sender() == ui->toolButtonPrv)
        found = textEdit->finding (txtFind, start, searchFlags | QTextDocument::FindBackward, tabPage->matchRegex());
    QColor color = QColor (textEdit->hasDarkScheme() ? Qt::darkGreen : Qt::green);
    int pos;
    if (!found.isNull())
    {
        pos = found.anchor();
        start.setPosition (pos);
        start.setPosition (found.position(), QTextCursor::KeepAnchor);
        textEdit->skipSelectionHighlighting();
        textEdit->setTextCursor (start);
        if (tabPage->matchRegex())
        { // also, cover capturing groups
            realTxtReplace = found.selectedText().replace (regexFind, txtReplace);
            textEdit->insertPlainText (realTxtReplace);
        }
        else
            textEdit->insertPlainText (txtReplace);

        start = textEdit->textCursor(); // now it is at the end of txtReplace
        tmp.setPosition (pos);
        tmp.setPosition (start.position(), QTextCursor::KeepAnchor);
        QTextEdit::ExtraSelection extra;
        extra.format.setBackground (color);
        extra.cursor = tmp;
        es.append (extra);

        if (QObject::sender() != ui->toolButtonNext)
        {
            /* With the cursor at the end of the replacing text, if the backward replacement
               is repeated and the text is matched again (which is especially possible with
               regex), the replacement won't proceed. So, the cursor should be moved. */
            start.setPosition (start.position() - (tabPage->matchRegex()
                                                    ? realTxtReplace.length()
                                                    : txtReplace.length()));
            textEdit->setTextCursor (start);
        }
    }
    textEdit->setGreenSel (es);
    if (lineNumShown)
        es.prepend (textEdit->currentLineSelection());
    /* append blue and red highlights */
    es.append (textEdit->getBlueSel());
    es.append (textEdit->getColSel());
    es.append (textEdit->getRedSel());
    textEdit->setExtraSelections (es);
    /* add yellow highlights (perhaps with corrections) */
    hlight();
}
/*************************/
void FPwin::replaceAll()
{
    if (!isReady()) return;

    TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget());
    if (tabPage == nullptr) return;

    TextEdit *textEdit = tabPage->textEdit();
    if (textEdit->isReadOnly()) return;

    QString txtFind = ui->lineEditFind->text();
    if (txtFind.isEmpty()) return;

    const QString txtReplace = ui->lineEditReplace->text();

    QList<QTextEdit::ExtraSelection> es = textEdit->getGreenSel();
    /* remove previous green highlights if the replacing text is changed */
    if (!es.isEmpty() && es.first().cursor.selectedText() != txtReplace)
    {
        textEdit->setGreenSel (QList<QTextEdit::ExtraSelection>());
        es.clear();
    }

    QTextDocument::FindFlags searchFlags = getSearchFlags();

    QRegularExpression regexFind;
    if (tabPage->matchRegex())
    {
        regexFind = QRegularExpression (txtFind, (searchFlags & QTextDocument::FindCaseSensitively)
                                                    ? QRegularExpression::NoPatternOption
                                                    : QRegularExpression::CaseInsensitiveOption);
    }

    QTextCursor orig = textEdit->textCursor();
    orig.setPosition (orig.anchor());
    textEdit->setTextCursor (orig);
    QColor color = QColor (textEdit->hasDarkScheme() ? Qt::darkGreen : Qt::green);
    int pos; QTextCursor found;
    QTextCursor start = orig;
    start.beginEditBlock();
    start.setPosition (0);
    QTextCursor tmp = start;
    int count = 0;
    QTextEdit::ExtraSelection extra;
    extra.format.setBackground (color);

    makeBusy();
    while (!(found = textEdit->finding (txtFind, start, searchFlags, tabPage->matchRegex())).isNull())
    {
        start.setPosition (found.anchor());
        pos = found.anchor();
        start.setPosition (found.position(), QTextCursor::KeepAnchor);
        if (tabPage->matchRegex())
            start.insertText (found.selectedText().replace (regexFind, txtReplace));
        else
            start.insertText (txtReplace);

        if (count < 1000)
        {
            tmp.setPosition (pos);
            tmp.setPosition (start.position(), QTextCursor::KeepAnchor);
            extra.cursor = tmp;
            es.append (extra);
        }
        start.setPosition (start.position());
        ++count;
    }
    unbusy();

    textEdit->setGreenSel (es);
    start.endEditBlock();
    if ((ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible()))
        es.prepend (textEdit->currentLineSelection());
    es.append (textEdit->getBlueSel());
    es.append (textEdit->getColSel());
    es.append (textEdit->getRedSel());
    textEdit->setExtraSelections (es);
    hlight();

    QString title;
    if (count == 0)
        title = tr ("No Replacement");
    else if (count == 1)
        title =  tr ("One Replacement");
    else
        title = tr("%Ln Replacements", "", count);
    ui->dockReplace->setWindowTitle (title);
    textEdit->setReplaceTitle (title);
    if (count > 1000 && !txtReplace.isEmpty())
        showWarningBar ("<center><b><big>" + tr ("The first 1000 replacements are highlighted.") + "</big></b></center>");
}

}
