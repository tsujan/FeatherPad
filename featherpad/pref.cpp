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

#include "singleton.h"
#include "ui_fp.h"

#include "pref.h"
#include "ui_predDialog.h"

#include <QScreen>
#include <QWindow>
#include <QWhatsThis>
#include <QKeySequenceEdit>
#include <QFileInfo>

namespace FeatherPad {

static QHash<QString, QString> OBJECT_NAMES;
static QHash<QString, QString> DEFAULT_SHORTCUTS;

Delegate::Delegate (QObject *parent)
    : QStyledItemDelegate (parent)
{
}

QWidget* Delegate::createEditor (QWidget *parent,
                                 const QStyleOptionViewItem& /*option*/,
                                 const QModelIndex& /*index*/) const
{
    return new QKeySequenceEdit (parent);
}
/*************************/
bool Delegate::eventFilter (QObject *object, QEvent *event)
{
    QWidget *editor = qobject_cast<QWidget*>(object);
    if (editor && event->type() == QEvent::KeyPress)
    {
        int k = static_cast<QKeyEvent *>(event)->key();
        if (k == Qt::Key_Return || k == Qt::Key_Enter)
        {
            emit QAbstractItemDelegate::commitData (editor);
            emit QAbstractItemDelegate::closeEditor (editor);
            return true;
        }
    }
    return QStyledItemDelegate::eventFilter (object, event);
}
/*************************/
PrefDialog::PrefDialog (QWidget *parent)
    : QDialog (parent), ui (new Ui::PrefDialog)
{
    ui->setupUi (this);
    parent_ = parent;
    setWindowModality (Qt::WindowModal);
    ui->promptLabel->hide();
    promptTimer_ = nullptr;

    Delegate *del = new Delegate (ui->tableWidget);
    ui->tableWidget->setItemDelegate (del);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode (QHeaderView::Stretch);
    ui->tableWidget->horizontalHeader()->setSectionsClickable (false);
    ui->tableWidget->sortByColumn (0, Qt::AscendingOrder);

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    sysIcons_ = config.getSysIcon();
    iconless_ = config.getIconless();
    darkBg_ = config.getDarkColScheme();
    darkColValue_ = config.getDarkBgColorValue();
    lightColValue_ = config.getLightBgColorValue();
    recentNumber_ = config.getRecentFilesNumber();
    showWhiteSpace_ = config.getShowWhiteSpace();
    showEndings_ = config.getShowEndings();
    vLineDistance_ = config.getVLineDistance();
    textTabSize_ = config.getTextTabSize();
    tabInsertSpaces_ = config.getTabInsertSpaces();

    /**************
     *** Window ***
     **************/

    ui->winSizeBox->setChecked (config.getRemSize());
    connect (ui->winSizeBox, &QCheckBox::stateChanged, this, &PrefDialog::prefSize);
    if (ui->winSizeBox->isChecked())
    {
        ui->spinX->setEnabled (false);
        ui->spinY->setEnabled (false);
        ui->mLabel->setEnabled (false);
        ui->sizeLable->setEnabled (false);
    }
    QSize ag;
    if (parent != nullptr)
    {
        if (QWindow *win = parent->windowHandle())
        {
            if (QScreen *sc = win->screen())
                ag = sc->availableVirtualGeometry().size();
        }
    }
    if (!ag.isValid()) ag = QSize(0, 0);
    ui->spinX->setMaximum (ag.width());
    ui->spinY->setMaximum (ag.height());
    ui->spinX->setValue (config.getStartSize().width());
    ui->spinY->setValue (config.getStartSize().height());
    /* old-fashioned: connect (ui->spinX, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),... */
    connect (ui->spinX, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefStartSize);
    connect (ui->spinY, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefStartSize);

    ui->winPosBox->setChecked (config.getRemPos());
    connect (ui->winPosBox, &QCheckBox::stateChanged, this, &PrefDialog::prefPos);

    ui->iconBox->setChecked (!config.getSysIcon());
    ui->iconBox->setEnabled (!config.getIconless());
    connect (ui->iconBox, &QCheckBox::stateChanged, this, &PrefDialog::prefIcon);
    ui->iconlessBox->setChecked (config.getIconless());
    connect (ui->iconlessBox, &QCheckBox::stateChanged, this, &PrefDialog::prefIconless);

    ui->toolbarBox->setChecked (config.getNoToolbar());
    connect (ui->toolbarBox, &QCheckBox::stateChanged, this, &PrefDialog::prefToolbar);
    ui->menubarBox->setChecked (config.getNoMenubar());
    connect (ui->menubarBox, &QCheckBox::stateChanged, this, &PrefDialog::prefMenubar);

    ui->searchbarBox->setChecked (config.getHideSearchbar());
    connect (ui->searchbarBox, &QCheckBox::stateChanged, this, &PrefDialog::prefSearchbar);

    ui->statusBox->setChecked (config.getShowStatusbar());
    connect (ui->statusBox, &QCheckBox::stateChanged, this, &PrefDialog::prefStatusbar);

    ui->statusCursorsBox->setChecked (config.getShowCursorPos());
    connect (ui->statusCursorsBox, &QCheckBox::stateChanged, this, &PrefDialog::prefStatusCursor);

    // no ccombo onnection because of mouse wheel; config is set at closeEvent() instead
    ui->tabCombo->setCurrentIndex (config.getTabPosition());

    ui->tabBox->setChecked (config.getTabWrapAround());
    connect (ui->tabBox, &QCheckBox::stateChanged, this, &PrefDialog::prefTabWrapAround);

    ui->singleTabBox->setChecked (config.getHideSingleTab());
    connect (ui->singleTabBox, &QCheckBox::stateChanged, this, &PrefDialog::prefHideSingleTab);

    ui->windowBox->setChecked (config.getOpenInWindows());
    connect (ui->windowBox, &QCheckBox::stateChanged, this, &PrefDialog::prefOpenInWindows);

    ui->nativeDialogBox->setChecked (config.getNativeDialog());
    connect (ui->nativeDialogBox, &QCheckBox::stateChanged, this, &PrefDialog::prefNativeDialog);

    ui->sidePaneBox->setChecked (config.getSidePaneMode());
    ui->sidePaneSizeBox->setChecked (config.getRemSplitterPos());
    connect (ui->sidePaneBox, &QCheckBox::stateChanged, this, &PrefDialog::prefSidePaneMode);
    connect (ui->sidePaneSizeBox, &QCheckBox::stateChanged, this, &PrefDialog::prefSplitterPos);

    ui->lastTabBox->setChecked (config.getCloseWithLastTab());
    connect (ui->lastTabBox, &QCheckBox::stateChanged, this, &PrefDialog::prefCloseWithLastTab);

    /************
     *** Text ***
     ************/

    ui->fontBox->setChecked (config.getRemFont());
    connect (ui->fontBox, &QCheckBox::stateChanged, this, &PrefDialog::prefFont);

    ui->wrapBox->setChecked (config.getWrapByDefault());
    connect (ui->wrapBox, &QCheckBox::stateChanged, this, &PrefDialog::prefWrap);

    ui->indentBox->setChecked (config.getIndentByDefault());
    connect (ui->indentBox, &QCheckBox::stateChanged, this, &PrefDialog::prefIndent);

    ui->autoBracketBox->setChecked (config.getAutoBracket());
    connect (ui->autoBracketBox, &QCheckBox::stateChanged, this, &PrefDialog::prefAutoBracket);

    ui->lineBox->setChecked (config.getLineByDefault());
    connect (ui->lineBox, &QCheckBox::stateChanged, this, &PrefDialog::prefLine);

    ui->syntaxBox->setChecked (config.getSyntaxByDefault());
    connect (ui->syntaxBox, &QCheckBox::stateChanged, this, &PrefDialog::prefSyntax);

    ui->enforceSyntaxBox->setChecked (config.getShowLangSelector());
    ui->enforceSyntaxBox->setEnabled (config.getSyntaxByDefault());

    ui->whiteSpaceBox->setChecked (config.getShowWhiteSpace());
    connect (ui->whiteSpaceBox, &QCheckBox::stateChanged, this, &PrefDialog::prefWhiteSpace);

    ui->vLineBox->setChecked (vLineDistance_ >= 10);
    connect (ui->vLineBox, &QCheckBox::stateChanged, this, &PrefDialog::prefVLine);
    ui->vLineSpin->setEnabled (vLineDistance_ >= 10);
    ui->vLineSpin->setValue (qAbs (vLineDistance_));
    connect (ui->vLineSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefVLineDistance);

    ui->endingsBox->setChecked (config.getShowEndings());
    connect (ui->endingsBox, &QCheckBox::stateChanged, this, &PrefDialog::prefEndings);

    ui->colBox->setChecked (config.getDarkColScheme());
    connect (ui->colBox, &QCheckBox::stateChanged, this, &PrefDialog::prefDarkColScheme);
    if (!ui->colBox->isChecked())
    {
        ui->colorValueSpin->setMinimum (230);
        ui->colorValueSpin->setMaximum (255);
        ui->colorValueSpin->setValue (config.getLightBgColorValue());
    }
    else
    {
        ui->colorValueSpin->setMinimum (0);
        ui->colorValueSpin->setMaximum (50);
        ui->colorValueSpin->setValue (config.getDarkBgColorValue());
    }
    connect (ui->colorValueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefColValue);

    ui->thickCursorBox->setChecked (config.getThickCursor());

    ui->dateEdit->setText (config.getDateFormat());

    ui->lastLineBox->setChecked (config.getAppendEmptyLine());
    connect (ui->lastLineBox, &QCheckBox::stateChanged, this, &PrefDialog::prefAppendEmptyLine);

    ui->tabInsertSpacesBox->setChecked (tabInsertSpaces_);
    connect (ui->tabInsertSpacesBox, &QCheckBox::stateChanged, this, &PrefDialog::prefTabInsertSpaces);

    ui->trailingSpacesBox->setChecked (config.getRemoveTrailingSpaces());
    connect (ui->trailingSpacesBox, &QCheckBox::stateChanged, this, &PrefDialog::prefRemoveTrailingSpaces);

    ui->scrollBox->setChecked (config.getScrollJumpWorkaround());
    connect (ui->scrollBox, &QCheckBox::stateChanged, this, &PrefDialog::prefScrollJumpWorkaround);

    ui->skipNonTextBox->setChecked (config.getSkipNonText());
    connect (ui->skipNonTextBox, &QCheckBox::stateChanged, this, &PrefDialog::prefSkipNontext);

    ui->spinBox->setValue (config.getMaxSHSize());
    connect (ui->spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefMaxSHSize);

    ui->inertiaBox->setChecked (config.getInertialScrolling());
    connect (ui->inertiaBox, &QCheckBox::stateChanged, this, &PrefDialog::prefInertialScrolling);

    ui->textTabSpin->setValue (textTabSize_);
    connect (ui->textTabSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefTextTabSize);

    /*************
     *** Files ***
     *************/

    ui->exeBox->setChecked (config.getExecuteScripts());
    connect (ui->exeBox, &QCheckBox::stateChanged, this, &PrefDialog::prefExecute);
    ui->commandEdit->setText (config.getExecuteCommand());
    ui->commandEdit->setEnabled (config.getExecuteScripts());
    ui->commandLabel->setEnabled (config.getExecuteScripts());
    connect (ui->commandEdit, &QLineEdit::textEdited, this, &PrefDialog::prefCommand);

    ui->recentSpin->setValue (config.getRecentFilesNumber());
    ui->recentSpin->setSuffix(" " + (ui->recentSpin->value() > 1 ? tr ("files") : tr ("file")));
    connect (ui->recentSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefRecentFilesNumber);

    ui->lastFilesBox->setChecked (config.getSaveLastFilesList());
    connect (ui->lastFilesBox, &QCheckBox::stateChanged, this, &PrefDialog::prefSaveLastFilesList);

    ui->openedButton->setChecked (config.getRecentOpened());
    // no QButtonGroup connection because we want to see if we should clear the recent list at the end

    ui->autoSaveBox->setChecked (config.getAutoSave());
    ui->autoSaveSpin->setValue (config.getAutoSaveInterval());
    ui->autoSaveSpin->setEnabled (ui->autoSaveBox->isChecked());
    connect (ui->autoSaveBox, &QCheckBox::stateChanged, this, &PrefDialog::prefAutoSave);

    ui->unmodifiedSaveBox->setChecked (config.getSaveUnmodified());

    /*****************
     *** Shortcuts ***
     *****************/

    if (FPwin *win = static_cast<FPwin *>(parent_))
    {
        if (DEFAULT_SHORTCUTS.isEmpty())
        { // NOTE: Shortcut strings hould be in the PortableText format.
            const auto defaultShortcuts = win->defaultShortcuts();
            QHash<QAction*, QKeySequence>::const_iterator iter = defaultShortcuts.constBegin();
            while (iter != defaultShortcuts.constEnd())
            {
                const QString name = iter.key()->objectName();
                DEFAULT_SHORTCUTS.insert (name, iter.value().toString());
                OBJECT_NAMES.insert (iter.key()->text().remove ("&"), name);
                ++ iter;
            }
        }
    }

    QHash<QString, QString> ca = config.customShortcutActions();

    QList<QString> keys = ca.keys();
    QHash<QString, QString>::const_iterator iter = OBJECT_NAMES.constBegin();
    while (iter != OBJECT_NAMES.constEnd())
    {
        shortcuts_.insert (iter.key(),
                           keys.contains (iter.value()) ? ca.value (iter.value())
                                                        : DEFAULT_SHORTCUTS.value (iter.value()));
        ++ iter;
    }

    QList<QString> val = shortcuts_.values();
    for (int i = 0; i < val.size(); ++i)
    {
        if (val.indexOf (val.at (i), i + 1) > -1)
        {
            showPrompt (tr ("Warning: Ambiguous shortcut detected!"), false);
            break;
        }
    }

    ui->tableWidget->setRowCount (shortcuts_.size());
    ui->tableWidget->setSortingEnabled (false);
    int index = 0;
    QHash<QString, QString>::const_iterator it = shortcuts_.constBegin();
    while (it != shortcuts_.constEnd())
    {
        QTableWidgetItem *item = new QTableWidgetItem (it.key());
        item->setFlags (item->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
        ui->tableWidget->setItem (index, 0, item);
        /* shortcut texts should added in the NativeText format */
        ui->tableWidget->setItem (index, 1, new QTableWidgetItem (QKeySequence (it.value(), QKeySequence::PortableText)
                                                                  .toString (QKeySequence::NativeText)));
        ++ it;
        ++ index;
    }
    ui->tableWidget->setSortingEnabled (true);
    ui->tableWidget->setCurrentCell (0, 1);
    connect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
    connect (ui->defaultButton, &QAbstractButton::clicked, this, &PrefDialog::restoreDefaultShortcuts);
    ui->defaultButton->setDisabled (ca.isEmpty());

    /*************
     *** Other ***
     *************/

    connect (ui->closeButton, &QAbstractButton::clicked, this, &QDialog::close);
    connect (ui->helpButton, &QAbstractButton::clicked, this, &PrefDialog::showWhatsThis);
    connect (this, &QDialog::rejected, this, &PrefDialog::onClosing);

    /* set tooltip as "whatsthis" */
    const auto widgets = findChildren<QWidget*>();
    for (QWidget *w : widgets)
    {
        QString tip = w->toolTip();
        if (!tip.isEmpty())
        {
            w->setWhatsThis (tip.replace ('\n', ' ').replace ("  ", "\n\n"));
            /* for the tooltip mess in Qt 5.12 */
            w->setToolTip ("<p style='white-space:pre'>" + w->toolTip() + "</p>");
        }
    }

    resize (sizeHint() + QSize (style()->pixelMetric(QStyle::PM_ScrollBarExtent), 0));
}
/*************************/
PrefDialog::~PrefDialog()
{
    if (promptTimer_)
    {
        promptTimer_->stop();
        delete promptTimer_;
    }
    delete ui; ui = nullptr;
}
/*************************/
void PrefDialog::closeEvent (QCloseEvent *event)
{
    onClosing();
    event->accept();
}
/*************************/
void PrefDialog::onClosing()
{
    prefShortcuts();
    prefTabPosition();
    prefRecentFilesKind();
    prefApplyAutoSave();
    prefApplySyntax();
    prefApplyDateFormat();
    prefTextTab();
    prefSaveUnmodified();
    prefThickCursor();
}
/*************************/
void PrefDialog::showPrompt (const QString& str, bool temporary)
{
    static const QString style ("QLabel {background-color: #7d0000; color: white; border-radius: 3px; margin: 2px; padding: 5px;}");
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (!str.isEmpty())
    { // show the provided message
        ui->promptLabel->setText ("<b>" + str + "</b>");
        ui->promptLabel->setStyleSheet (style);
        if (temporary) // show it temporarily
        {
            if (!promptTimer_)
            {
                promptTimer_ = new QTimer();
                promptTimer_->setSingleShot (true);
                connect (promptTimer_, &QTimer::timeout, [this] {
                    if (!prevtMsg_.isEmpty()
                        && ui->tabWidget->currentIndex() == 3) // Shortcuts page
                    { // show the previous message if it exists
                        ui->promptLabel->setText (prevtMsg_);
                        ui->promptLabel->setStyleSheet (style);
                    }
                    else showPrompt();
                });
            }
            promptTimer_->start (3300);
        }
        else
            prevtMsg_ = "<b>" + str + "</b>";
    }
    else if (sysIcons_ != config.getSysIcon()
            || iconless_ != config.getIconless()
            || recentNumber_ != config.getRecentFilesNumber())
    {
        ui->promptLabel->setText ("<b>" + tr ("Application restart is needed for changes to take effect.") + "</b>");
        ui->promptLabel->setStyleSheet (style);
    }
    else if (darkBg_ != config.getDarkColScheme()
             || (darkBg_ && darkColValue_ != config.getDarkBgColorValue())
             || (!darkBg_ && lightColValue_ != config.getLightBgColorValue())
             || showWhiteSpace_ != config.getShowWhiteSpace()
             || showEndings_ != config.getShowEndings()
             || textTabSize_ != config.getTextTabSize()
             || (vLineDistance_ * config.getVLineDistance() < 0
             || (vLineDistance_ > 0 && vLineDistance_ != config.getVLineDistance()))
             || tabInsertSpaces_ != config.getTabInsertSpaces())
    {
        ui->promptLabel->setText ("<b>" + tr ("Window reopening is needed for changes to take effect.") + "</b>");
        ui->promptLabel->setStyleSheet (style);
    }
    else
    {
        if (prevtMsg_.isEmpty()) // clear prompt
        {
            ui->promptLabel->clear();
            ui->promptLabel->setStyleSheet ("QLabel {margin: 2px; padding: 5px;}");
        }
        else // show the previous message
        {
            ui->promptLabel->setText (prevtMsg_);
            ui->promptLabel->setStyleSheet (style);
        }
    }
    ui->promptLabel->show();
}
/*************************/
void PrefDialog::showWhatsThis()
{
    QWhatsThis::enterWhatsThisMode();
}
/*************************/
void PrefDialog::prefSize (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
    {
        config.setRemSize (true);
        ui->spinX->setEnabled (false);
        ui->spinY->setEnabled (false);
        ui->mLabel->setEnabled (false);
        ui->sizeLable->setEnabled (false);
    }
    else if (checked == Qt::Unchecked)
    {
        config.setRemSize (false);
        ui->spinX->setEnabled (true);
        ui->spinY->setEnabled (true);
        ui->mLabel->setEnabled (true);
        ui->sizeLable->setEnabled (true);
    }
}
/*************************/
void PrefDialog::prefPos (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setRemPos (true);
    else if (checked == Qt::Unchecked)
        config.setRemPos (false);
}
/*************************/
void PrefDialog::prefIcon (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setSysIcon (false);
    else if (checked == Qt::Unchecked)
        config.setSysIcon (true);

    showPrompt();
}
/*************************/
void PrefDialog::prefIconless (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
    {
        qApp->setAttribute (Qt::AA_DontShowIconsInMenus, true);
        config.setIconless (true);
        ui->iconBox->setEnabled (false);
    }
    else if (checked == Qt::Unchecked)
    {
        qApp->setAttribute (Qt::AA_DontShowIconsInMenus, false);
        config.setIconless (false);
        ui->iconBox->setEnabled (true);
    }

    showPrompt();
}
/*************************/
void PrefDialog::prefToolbar (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
    {
        if (ui->menubarBox->checkState() == Qt::Checked)
            ui->menubarBox->setCheckState (Qt::Unchecked);
        config.setNoToolbar (true);
        for (int i = 0; i < singleton->Wins.count(); ++i)
            singleton->Wins.at (i)->ui->mainToolBar->setVisible (false);
    }
    else if (checked == Qt::Unchecked)
    {
        config.setNoToolbar (false);
        for (int i = 0; i < singleton->Wins.count(); ++i)
            singleton->Wins.at (i)->ui->mainToolBar->setVisible (true);
    }
}
/*************************/
void PrefDialog::prefMenubar (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
    {
        if (ui->toolbarBox->checkState() == Qt::Checked)
            ui->toolbarBox->setCheckState (Qt::Unchecked);
        config.setNoMenubar (true);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            singleton->Wins.at (i)->ui->menuBar->setVisible (false);
            singleton->Wins.at (i)->ui->actionMenu->setVisible (true);
        }
    }
    else if (checked == Qt::Unchecked)
    {
        config.setNoMenubar (false);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            singleton->Wins.at (i)->ui->menuBar->setVisible (true);
            singleton->Wins.at (i)->ui->actionMenu->setVisible (false);
        }
    }
}
/*************************/
void PrefDialog::prefSearchbar (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setHideSearchbar (true);
    else if (checked == Qt::Unchecked)
        config.setHideSearchbar (false);
}
/*************************/
void PrefDialog::prefStatusbar (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    bool showCurPos = config.getShowCursorPos();
    if (checked == Qt::Checked)
    {
        config.setShowStatusbar (true);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *win = singleton->Wins.at (i);

            if (!win->ui->statusBar->isVisible())
            {
                /* here we can't use docProp() directly
                   because we don't want to count words */
                if (TabPage *tabPage = qobject_cast<TabPage*>(win->ui->tabWidget->currentWidget()))
                {
                    TextEdit *textEdit = tabPage->textEdit();
                    win->statusMsgWithLineCount (textEdit->document()->blockCount());
                    for (int j = 0; j < win->ui->tabWidget->count(); ++j)
                    {
                        TextEdit *thisTextEdit = qobject_cast< TabPage *>(win->ui->tabWidget->widget (j))->textEdit();
                        connect (thisTextEdit, &QPlainTextEdit::blockCountChanged, win, &FPwin::statusMsgWithLineCount);
                        connect (thisTextEdit, &QPlainTextEdit::selectionChanged, win, &FPwin::statusMsg);
                        if (showCurPos)
                            connect (thisTextEdit, &QPlainTextEdit::cursorPositionChanged, win, &FPwin::showCursorPos);
                    }
                    win->ui->statusBar->setVisible (true);
                    if (showCurPos)
                    {
                        win->addCursorPosLabel();
                        win->showCursorPos();
                    }
                    if (QToolButton *wordButton = win->ui->statusBar->findChild<QToolButton *>("wordButton"))
                    {
                        wordButton->setVisible (true);
                        if (textEdit->getWordNumber() != -1 // when words are already counted
                            || textEdit->document()->isEmpty())
                        {
                            win->updateWordInfo();
                        }
                    }
                }
            }
            /* no need for this menu item anymore */
            win->ui->actionDoc->setVisible (false);
        }
    }
    else if (checked == Qt::Unchecked)
    {
        config.setShowStatusbar (false);
        for (int i = 0; i < singleton->Wins.count(); ++i)
            singleton->Wins.at (i)->ui->actionDoc->setVisible (true);
    }
}
/*************************/
void PrefDialog::prefStatusCursor (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
    {
        config.setShowCursorPos (true);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *win = singleton->Wins.at (i);
            int count = win->ui->tabWidget->count();
            if (count > 0 && win->ui->statusBar->isVisible())
            {
                win->addCursorPosLabel();
                win->showCursorPos();
                for (int j = 0; j < count; ++j)
                {
                    TextEdit *textEdit = qobject_cast< TabPage *>(win->ui->tabWidget->widget (j))->textEdit();
                    connect (textEdit, &QPlainTextEdit::cursorPositionChanged, win, &FPwin::showCursorPos);
                }
            }
        }
    }
    else if (checked == Qt::Unchecked)
    {
        config.setShowCursorPos (false);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *win = singleton->Wins.at (i);
            if (QLabel *posLabel = win->ui->statusBar->findChild<QLabel *>("posLabel"))
            {
                int count = win->ui->tabWidget->count();
                if (count > 0 && win->ui->statusBar->isVisible())
                {
                    for (int j = 0; j < count; ++j)
                    {
                        TextEdit *textEdit = qobject_cast< TabPage *>(win->ui->tabWidget->widget (j))->textEdit();
                        disconnect (textEdit, &QPlainTextEdit::cursorPositionChanged, win, &FPwin::showCursorPos);
                    }
                }
                posLabel->deleteLater();
            }
        }
    }
}
/*************************/
void PrefDialog::prefTabPosition()
{
    int index = ui->tabCombo->currentIndex();
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    config.setTabPosition (index);
    if (singleton->Wins.at (0)->ui->tabWidget->tabPosition() != static_cast<QTabWidget::TabPosition>(index))
    {
        for (int i = 0; i < singleton->Wins.count(); ++i)
            singleton->Wins.at (i)->ui->tabWidget->setTabPosition (static_cast<QTabWidget::TabPosition>(index));
    }
}
/*************************/
void PrefDialog::prefFont (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
    {
        config.setRemFont (true);
        // get the document font of the current window
        if (FPwin *win = static_cast<FPwin *>(parent_))
        {
            if (TabPage *tabPage = qobject_cast<TabPage*>(win->ui->tabWidget->currentWidget()))
                config.setFont (tabPage->textEdit()->getDefaultFont());
        }
    }
    else if (checked == Qt::Unchecked)
    {
        config.setRemFont (false);
        // return to our default font
        config.setFont (QFont ("Monospace", 9));
    }
}
/*************************/
void PrefDialog::prefWrap (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setWrapByDefault (true);
    else if (checked == Qt::Unchecked)
        config.setWrapByDefault (false);
}
/*************************/
void PrefDialog::prefIndent (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setIndentByDefault (true);
    else if (checked == Qt::Unchecked)
        config.setIndentByDefault (false);
}
/*************************/
void PrefDialog::prefAutoBracket (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
    {
        config.setAutoBracket (true);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            int count = singleton->Wins.at (i)->ui->tabWidget->count();
            for (int j = 0; j < count; ++j)
            {
                qobject_cast< TabPage *>(singleton->Wins.at (i)->ui->tabWidget->widget (j))
                        ->textEdit()->setAutoBracket (true);
            }
        }
    }
    else if (checked == Qt::Unchecked)
    {
        config.setAutoBracket (false);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            int count = singleton->Wins.at (i)->ui->tabWidget->count();
            for (int j = 0; j < count; ++j)
            {
                qobject_cast< TabPage *>(singleton->Wins.at (i)->ui->tabWidget->widget (j))
                        ->textEdit()->setAutoBracket (false);
            }
        }
    }
}
/*************************/
void PrefDialog::prefLine (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
    {
        config.setLineByDefault (true);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *thisWin = singleton->Wins.at (i);
            thisWin->ui->actionLineNumbers->setChecked (true);
            thisWin->ui->actionLineNumbers->setDisabled (true);
        }
    }
    else if (checked == Qt::Unchecked)
    {
        config.setLineByDefault (false);
        for (int i = 0; i < singleton->Wins.count(); ++i)
            singleton->Wins.at (i)->ui->actionLineNumbers->setEnabled (true);
    }
}
/*************************/
void PrefDialog::prefSyntax (int checked)
{
    /* only set the state of the syntax enforcing checkbox */
    if (checked == Qt::Checked)
        ui->enforceSyntaxBox->setEnabled (true);
    else if (checked == Qt::Unchecked)
        ui->enforceSyntaxBox->setEnabled (false);
}
/*************************/
void PrefDialog::prefApplySyntax()
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();

    bool langBtnExists (config.getShowLangSelector() && config.getSyntaxByDefault());
    bool addLnagBtn (!langBtnExists && ui->enforceSyntaxBox->isChecked() && ui->syntaxBox->isChecked());
    bool removeLangBtn (langBtnExists && (!ui->enforceSyntaxBox->isChecked() || ! ui->syntaxBox->isChecked()));

    config.setSyntaxByDefault (ui->syntaxBox->isChecked());
    config.setShowLangSelector (ui->enforceSyntaxBox->isChecked());

    if (addLnagBtn || removeLangBtn)
    { // add or remove all language buttons based on the new settings
        for (int i = 0; i < singleton->Wins.count(); ++i)
            singleton->Wins.at (i)->addRemoveLangBtn (addLnagBtn);
    }
}
/*************************/
void PrefDialog::prefApplyDateFormat()
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    QString format = ui->dateEdit->text();
    /* if "\n" is typed in the line-edit, interpret
       it as a newline because we're on Linux */
    if (!format.isEmpty())
        format.replace ("\\n", "\n");
    config.setDateFormat (format);
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        int count = singleton->Wins.at (i)->ui->tabWidget->count();
        for (int j = 0; j < count; ++j)
        {
            qobject_cast< TabPage *>(singleton->Wins.at (i)->ui->tabWidget->widget (j))
                    ->textEdit()->setDateFormat (format);
        }
    }
}
/*************************/
void PrefDialog::prefWhiteSpace (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setShowWhiteSpace (true);
    else if (checked == Qt::Unchecked)
        config.setShowWhiteSpace (false);

    showPrompt();
}
/*************************/
void PrefDialog::prefVLine (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    int dsitance = qMax (qMin (ui->vLineSpin->value(), 999), 10);
    if (checked == Qt::Checked)
    {
        config.setVLineDistance (dsitance);
        ui->vLineSpin->setEnabled (true);
    }
    else if (checked == Qt::Unchecked)
    {
        config.setVLineDistance (-1 * dsitance);
        ui->vLineSpin->setEnabled (false);
    }

    showPrompt();
}
/*************************/
void PrefDialog::prefVLineDistance (int value)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    int dsitance = qMax (qMin (value, 999), 10);
    config.setVLineDistance (dsitance);
    showPrompt();
}
/*************************/
void PrefDialog::prefEndings (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setShowEndings (true);
    else if (checked == Qt::Unchecked)
        config.setShowEndings (false);

    showPrompt();
}
/*************************/
void PrefDialog::prefDarkColScheme (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    disconnect (ui->colorValueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefColValue);
    if (checked == Qt::Checked)
    {
        config.setDarkColScheme (true);
        ui->colorValueSpin->setMinimum (0);
        ui->colorValueSpin->setMaximum (50);
        ui->colorValueSpin->setValue (config.getDarkBgColorValue());
    }
    else if (checked == Qt::Unchecked)
    {
        config.setDarkColScheme (false);
        ui->colorValueSpin->setMinimum (230);
        ui->colorValueSpin->setMaximum (255);
        ui->colorValueSpin->setValue (config.getLightBgColorValue());
    }
    connect (ui->colorValueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefColValue);

    showPrompt();
}
/*************************/
void PrefDialog::prefColValue (int value)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (!ui->colBox->isChecked())
        config.setLightBgColorValue (value);
    else
        config.setDarkBgColorValue (value);

    showPrompt();
}
/*************************/
void PrefDialog::prefThickCursor()
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    bool thick (ui->thickCursorBox->isChecked());
    config.setThickCursor (thick);
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        int count = singleton->Wins.at (i)->ui->tabWidget->count();
        for (int j = 0; j < count; ++j)
        {
            TextEdit *textedit = qobject_cast< TabPage *>(singleton->Wins.at (i)->ui->tabWidget->widget (j))
                                 ->textEdit();
            if (j == 0 && textedit->getThickCursor() == thick)
                return;
            textedit->setThickCursor (thick);
        }
    }
}
/*************************/
void PrefDialog::prefAppendEmptyLine (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
        config.setAppendEmptyLine (true);
    else if (checked == Qt::Unchecked)
        config.setAppendEmptyLine (false);
}
/*************************/
void PrefDialog::prefTabInsertSpaces (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
        config.setTabInsertSpaces (true);
    else if (checked == Qt::Unchecked)
        config.setTabInsertSpaces (false);
    showPrompt();
}
/*************************/
void PrefDialog::prefRemoveTrailingSpaces (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
        config.setRemoveTrailingSpaces (true);
    else if (checked == Qt::Unchecked)
        config.setRemoveTrailingSpaces (false);
}
/*************************/
void PrefDialog::prefScrollJumpWorkaround (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
    {
        config.setScrollJumpWorkaround (true);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            int count = singleton->Wins.at (i)->ui->tabWidget->count();
            for (int j = 0; j < count; ++j)
            {
                qobject_cast< TabPage *>(singleton->Wins.at (i)->ui->tabWidget->widget (j))
                        ->textEdit()->setScrollJumpWorkaround (true);
            }
        }
    }
    else if (checked == Qt::Unchecked)
    {
        config.setScrollJumpWorkaround (false);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            int count = singleton->Wins.at (i)->ui->tabWidget->count();
            for (int j = 0; j < count; ++j)
            {
                qobject_cast< TabPage *>(singleton->Wins.at (i)->ui->tabWidget->widget (j))
                        ->textEdit()->setScrollJumpWorkaround (false);
            }
        }
    }
}
/*************************/
void PrefDialog::prefSkipNontext (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setSkipNonText (true);
    else if (checked == Qt::Unchecked)
        config.setSkipNonText (false);
}
/*************************/
void PrefDialog::prefTabWrapAround (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setTabWrapAround (true);
    else if (checked == Qt::Unchecked)
        config.setTabWrapAround (false);
}
/*************************/
void PrefDialog::prefHideSingleTab (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
    {
        config.setHideSingleTab (true);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *win = singleton->Wins.at (i);
            TabBar *tabBar = win->ui->tabWidget->tabBar();
            if (!win->hasSidePane())
                tabBar->hideSingle (true);
            if (win->ui->tabWidget->count() == 1)
                tabBar->hide();
        }
    }
    else if (checked == Qt::Unchecked)
    {
        config.setHideSingleTab (false);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *win = singleton->Wins.at (i);
            TabBar *tabBar = win->ui->tabWidget->tabBar();
            tabBar->hideSingle (false);
            if (!win->hasSidePane() && win->ui->tabWidget->count() == 1)
                tabBar->show();
        }
    }
}
/*************************/
void PrefDialog::prefMaxSHSize (int value)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.setMaxSHSize (value);
}
/*************************/
void PrefDialog::prefInertialScrolling (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
    {
        config.setInertialScrolling (true);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *win = singleton->Wins.at (i);
            for (int j = 0; j < win->ui->tabWidget->count(); ++j)
                qobject_cast< TabPage *>(win->ui->tabWidget->widget (j))->textEdit()->setInertialScrolling (true);
        }
    }
    else if (checked == Qt::Unchecked)
    {
        config.setInertialScrolling (false);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *win = singleton->Wins.at (i);
            for (int j = 0; j < win->ui->tabWidget->count(); ++j)
                qobject_cast< TabPage *>(win->ui->tabWidget->widget (j))->textEdit()->setInertialScrolling (false);
        }
    }
}
/*************************/
void PrefDialog::prefExecute (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
    {
        config.setExecuteScripts (true);
        ui->commandEdit->setEnabled (true);
        ui->commandLabel->setEnabled (true);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *win = singleton->Wins.at (i);
            if (TabPage *tabPage = qobject_cast<TabPage*>(win->ui->tabWidget->currentWidget()))
            {
                TextEdit *textEdit = tabPage->textEdit();
                if (win->isScriptLang (textEdit->getProg()) && QFileInfo (textEdit->getFileName()).isExecutable())
                    win->ui->actionRun->setVisible (true);
            }
        }
    }
    else if (checked == Qt::Unchecked)
    {
        config.setExecuteScripts (false);
        ui->commandEdit->setEnabled (false);
        ui->commandLabel->setEnabled (false);
        for (int i = 0; i < singleton->Wins.count(); ++i)
            singleton->Wins.at (i)->ui->actionRun->setVisible (false);
    }
}
/*************************/
void PrefDialog::prefCommand (const QString& command)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.setExecuteCommand (command);
}
/*************************/
void PrefDialog::prefRecentFilesNumber (int value)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.setRecentFilesNumber (value); // doesn't take effect until the next session
    ui->recentSpin->setSuffix(" " + (value > 1 ? tr ("files") : tr ("file")));

    showPrompt();
}
/*************************/
void PrefDialog::prefSaveLastFilesList (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setSaveLastFilesList (true);
    else if (checked == Qt::Unchecked)
        config.setSaveLastFilesList (false);
}
/*************************/
void PrefDialog::prefRecentFilesKind()
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    bool openedKind = ui->openedButton->isChecked();
    if (config.getRecentOpened() != openedKind)
    {
        config.setRecentOpened (openedKind);
        config.clearRecentFiles();
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            singleton->Wins.at (i)->ui->menuOpenRecently->setTitle (openedKind
                                                                    ? tr ("&Recently Opened")
                                                                    : tr ("Recently &Modified"));
        }
    }
}
/*************************/
void PrefDialog::prefStartSize (int value)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    QSize startSize = config.getStartSize();
    if (QObject::sender() == ui->spinX)
        startSize.setWidth (value);
    else if (QObject::sender() == ui->spinY)
        startSize.setHeight (value);
    config.setStartSize (startSize);
}
/*************************/
void PrefDialog::prefOpenInWindows (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
        config.setOpenInWindows (true);
    else if (checked == Qt::Unchecked)
        config.setOpenInWindows (false);
}
/*************************/
void PrefDialog::prefNativeDialog (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
        config.setNativeDialog (true);
    else if (checked == Qt::Unchecked)
        config.setNativeDialog (false);
}
/*************************/
void PrefDialog::prefSidePaneMode (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
        config.setSidePaneMode (true);
    else if (checked == Qt::Unchecked)
        config.setSidePaneMode (false);
}
/*************************/
void PrefDialog::prefSplitterPos (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
        config.setRemSplitterPos (true);
    else if (checked == Qt::Unchecked)
        config.setRemSplitterPos (false);
}
/*************************/
// NOTE: Custom shortcuts will be saved in the PortableText format.
void PrefDialog::onShortcutChange (QTableWidgetItem *item)
{
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();

    QString txt = item->text();
    bool invalid (txt.isEmpty());
    if (!invalid)
    {
        /* the QKeySequenceEdit text is in the NativeText format but it should be
           converted into the PortableText format, both for saving and checking for "+" */
        QKeySequence keySeq (txt);
        txt = keySeq.toString();
        if (!txt.contains ("+"))
        {
            invalid = true;
            /* make and exception for Fx keys */
            for (int i = Qt::Key_F1; i <= Qt::Key_F35; ++i)
            {
                if (QKeySequence::ExactMatch == keySeq.matches (QKeySequence (i)))
                {
                    invalid = false;
                    break;
                }
            }
        }
    }

    QString desc = ui->tableWidget->item (ui->tableWidget->currentRow(), 0)->text();
    if (invalid
        || (config.reservedShortcuts().contains (txt)
            /* unless its (hard-coded) default shortcut is typed */
            && DEFAULT_SHORTCUTS.value (OBJECT_NAMES.value (desc)) != txt))
    {
        if (invalid)
            showPrompt (tr ("The typed shortcut was not valid."), true);
        else
            showPrompt (tr ("The typed shortcut was reserved."), true);
        disconnect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
        item->setText (shortcuts_.value (desc));
        connect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
    }
    else
    {
        if (!shortcuts_.keys (txt).isEmpty())
            showPrompt (tr ("Warning: Ambiguous shortcut detected!"), false);
        else if (ui->promptLabel->isVisible())
        {
            prevtMsg_ = QString();
            showPrompt();
        }
        shortcuts_.insert (desc, txt);
        newShortcuts_.insert (OBJECT_NAMES.value (desc), txt);
        /* also set the state of the Default button */
        QHash<QString, QString>::const_iterator it = shortcuts_.constBegin();
        while (it != shortcuts_.constEnd())
        {
            if (DEFAULT_SHORTCUTS.value (OBJECT_NAMES.value (it.key())) != it.value())
            {
                ui->defaultButton->setEnabled (true);
                return;
            }
            ++it;
        }
        ui->defaultButton->setEnabled (false);
    }
}
/*************************/
void PrefDialog::restoreDefaultShortcuts()
{
    if (newShortcuts_.isEmpty()
        && static_cast<FPsingleton*>(qApp)->getConfig().customShortcutActions().isEmpty())
    { // do nothing if there's no custom shortcut
        return;
    }

    disconnect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
    int cur = ui->tableWidget->currentColumn() == 0
                  ? 0
                  : ui->tableWidget->currentRow();
    ui->tableWidget->setSortingEnabled (false);
    newShortcuts_ = DEFAULT_SHORTCUTS;
    int index = 0;
    QMutableHashIterator<QString, QString> it (shortcuts_);
    while (it.hasNext())
    {
        it.next();
        ui->tableWidget->item (index, 0)->setText (it.key());
        QString s = DEFAULT_SHORTCUTS.value (OBJECT_NAMES.value (it.key()));
        ui->tableWidget->item (index, 1)->setText (s);
        it.setValue (s);
        ++ index;
    }
    ui->tableWidget->setSortingEnabled (true);
    ui->tableWidget->setCurrentCell (cur, 1);
    connect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);

    ui->defaultButton->setEnabled (false);
    if (ui->promptLabel->isVisible())
    {
        prevtMsg_ = QString();
        showPrompt();
    }
}
/*************************/
void PrefDialog::prefShortcuts()
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    QHash<QString, QString>::const_iterator it = newShortcuts_.constBegin();
    while (it != newShortcuts_.constEnd())
    {
        if (DEFAULT_SHORTCUTS.value (it.key()) == it.value())
            config.removeShortcut (it.key());
        else
            config.setActionShortcut (it.key(), it.value());
        ++it;
    }
    /* update the shortcuts for all windows
       (the current window will update them on closing this dialog) */
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        FPwin *win = singleton->Wins.at (i);
        if (win != parent_)
            win->updateCustomizableShortcuts();
    }
}
/*************************/
void PrefDialog::prefAutoSave (int checked)
{
    /* don't do anything other than enabling/disabling the spinbox */
    if (checked == Qt::Checked)
        ui->autoSaveSpin->setEnabled (true);
    else if (checked == Qt::Unchecked)
        ui->autoSaveSpin->setEnabled (false);
}
/*************************/
void PrefDialog::prefSaveUnmodified()
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    config.setSaveUnmodified (ui->unmodifiedSaveBox->isChecked());
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        FPwin *win = singleton->Wins.at (i);
        for (int j = 0; j < win->ui->tabWidget->count(); ++j)
        {
            TextEdit *textEdit = qobject_cast< TabPage *>(win->ui->tabWidget->widget (j))->textEdit();
            if (ui->unmodifiedSaveBox->isChecked())
            {
                disconnect (textEdit->document(), &QTextDocument::modificationChanged, win->ui->actionSave, &QAction::setEnabled);
                if (!textEdit->isReadOnly() && !textEdit->isUneditable())
                    win->ui->actionSave->setEnabled (true);
            }
            else
            {
                win->ui->actionSave->setEnabled (textEdit->document()->isModified());
                connect (textEdit->document(), &QTextDocument::modificationChanged, win->ui->actionSave, &QAction::setEnabled);
            }
        }
    }
}
/*************************/
void PrefDialog::prefApplyAutoSave()
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    bool as = ui->autoSaveBox->isChecked();
    int interval = ui->autoSaveSpin->value();
    if (config.getAutoSave() != as || interval != config.getAutoSaveInterval())
    {
        config.setAutoSave (as);
        config.setAutoSaveInterval (interval);
        for (int i = 0; i < singleton->Wins.count(); ++i)
            singleton->Wins.at (i)->startAutoSaving (as, interval);
    }
}
/*************************/
void PrefDialog::prefTextTabSize (int value)
{ // textTabSize_ is updated but the config isn't
    if (value >= 2 && value <= 10)
    {
        textTabSize_ = value;
        showPrompt();
    }
}
/*************************/
void PrefDialog::prefTextTab()
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.setTextTabSize (textTabSize_);
}
/*************************/
void PrefDialog::prefCloseWithLastTab (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setCloseWithLastTab (true);
    else if (checked == Qt::Unchecked)
        config.setCloseWithLastTab (false);
}

}
