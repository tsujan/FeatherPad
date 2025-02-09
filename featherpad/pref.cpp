/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2023 <tsujan2000@gmail.com>
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
#include "ui_prefDialog.h"
#include "filedialog.h"

#include <QScreen>
#include <QWindow>
#include <QWhatsThis>
#include <QFileInfo>
#include <QFileDialog>
#include <QColorDialog>
#include <QWheelEvent>

#include <cmath>

#if (QT_VERSION >= QT_VERSION_CHECK(6,7,0))
#define CHECKBOX_CHANGED QCheckBox::checkStateChanged
#else
#define CHECKBOX_CHANGED QCheckBox::stateChanged
#endif

namespace FeatherPad {

static QHash<QString, QString> OBJECT_NAMES;
static QHash<QString, QString> DEFAULT_SHORTCUTS;
/*************************/
FPKeySequenceEdit::FPKeySequenceEdit (QWidget *parent) : QKeySequenceEdit (parent) {}

void FPKeySequenceEdit::keyPressEvent (QKeyEvent *event)
{ // also a workaround for a Qt bug that makes Meta a non-modifier
    clear(); // no multiple shortcuts
    int k = event->key();
    if ((event->modifiers() != Qt::NoModifier && event->modifiers() != Qt::KeypadModifier))
    {
        if (k == Qt::Key_Super_L || k == Qt::Key_Super_R)
            return;
    }
    /* don't create a shortcut without modifier because
       this is a text editor but make exceptions for Fx keys */
    else if (k < Qt::Key_F1 || k > Qt::Key_F35)
        return;
    QKeySequenceEdit::keyPressEvent (event);
}
/*************************/
Delegate::Delegate (QObject *parent) : QStyledItemDelegate (parent) {}

QWidget* Delegate::createEditor (QWidget *parent,
                                 const QStyleOptionViewItem& /*option*/,
                                 const QModelIndex& /*index*/) const
{
    return new FPKeySequenceEdit (parent);
}
/*************************/
bool Delegate::eventFilter (QObject *object, QEvent *event)
{
    FPKeySequenceEdit *editor = qobject_cast<FPKeySequenceEdit*>(object);
    if (editor && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        int k = ke->key();
        if (k == Qt::Key_Return || k == Qt::Key_Enter)
        {
            emit QAbstractItemDelegate::commitData (editor);
            emit QAbstractItemDelegate::closeEditor (editor);
            return true;
        }
        /* treat Tab and Backtab like other keys */
        if (k == Qt::Key_Tab || k ==  Qt::Key_Backtab)
        {
            editor->pressKey (ke);
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
    ui->promptLabel->setStyleSheet ("QLabel {background-color: #7d0000; color: white; border-radius: 3px; margin: 2px; padding: 5px;}");
    ui->promptLabel->hide();
    promptTimer_ = nullptr;

    Delegate *del = new Delegate (ui->tableWidget);
    ui->tableWidget->setItemDelegate (del);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode (QHeaderView::Stretch);
    ui->tableWidget->horizontalHeader()->setSectionsClickable (true);
    ui->tableWidget->sortByColumn (0, Qt::AscendingOrder);
    ui->tableWidget->setToolTip (tr ("Press a modifier key to clear a shortcut\nin the editing mode."));

    ui->syntaxTableWidget->horizontalHeader()->setSectionResizeMode (0, QHeaderView::Stretch);
    ui->syntaxTableWidget->horizontalHeader()->setSectionResizeMode (1, QHeaderView::ResizeToContents);
    ui->syntaxTableWidget->horizontalHeader()->setSectionsClickable (false);
    ui->syntaxTableWidget->sortByColumn (0, Qt::AscendingOrder);
    ui->syntaxTableWidget->setToolTip (tr ("Double click a color to change it."));

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    darkBg_ = config.getDarkColScheme();
    darkColValue_ = config.getDarkBgColorValue();
    lightColValue_ = config.getLightBgColorValue();
    recentNumber_ = config.getRecentFilesNumber();
    showWhiteSpace_ = config.getShowWhiteSpace();
    showEndings_ = config.getShowEndings();
    textMargin_ = config.getTextMargin();
    vLineDistance_ = config.getVLineDistance();
    textTabSize_ = config.getTextTabSize();
    saveUnmodified_ = config.getSaveUnmodified();
    sharedSearchHistory_ = config.getSharedSearchHistory();
    selHighlighting_ = config.getSelectionHighlighting();
    pastePaths_ = config.getPastePaths();
    whiteSpaceValue_ = config.getWhiteSpaceValue();
    curLineHighlight_ = config.getCurLineHighlight();
    disableMenubarAccel_ = config.getDisableMenubarAccel();
    sysIcons_ = config.getSysIcons();

    /**************
     *** Window ***
     **************/

    ui->winSizeBox->setChecked (config.getRemSize());
    connect (ui->winSizeBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefSize);
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
                ag = sc->availableGeometry().size();
        }
    }
    if (ag.isEmpty()) ag = QSize (std::max (700, config.getStartSize().width()), std::max (500, config.getStartSize().height()));
    ui->spinX->setMaximum (ag.width());
    ui->spinY->setMaximum (ag.height());
    ui->spinX->setValue (config.getStartSize().width());
    ui->spinY->setValue (config.getStartSize().height());
    /* old-fashioned: connect (ui->spinX, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),... */
    connect (ui->spinX, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefStartSize);
    connect (ui->spinY, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefStartSize);
    if (auto le = ui->spinX->findChild<QLineEdit*>())
    {
        connect (le, &QLineEdit::textChanged, this, [this, config] (const QString &txt) {
            if (txt == ui->spinX->suffix())
                ui->spinX->setValue (config.getDefaultStartSize().width());
        });
    }
    if (auto le = ui->spinY->findChild<QLineEdit*>())
    {
        connect (le, &QLineEdit::textChanged, this, [this, config] (const QString &txt) {
            if (txt == ui->spinY->suffix())
                ui->spinY->setValue (config.getDefaultStartSize().height());
        });
    }

    ui->winPosBox->setChecked (config.getRemPos());
    connect (ui->winPosBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefPos);

    ui->toolbarBox->setChecked (config.getNoToolbar());
    connect (ui->toolbarBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefToolbar);
    ui->menubarBox->setChecked (config.getNoMenubar());
    connect (ui->menubarBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefMenubar);

    ui->menubarTitleBox->setChecked (config.getMenubarTitle());
    connect (ui->menubarTitleBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefMenubarTitle);

    ui->searchbarBox->setChecked (config.getHideSearchbar());
    connect (ui->searchbarBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefSearchbar);

    ui->searchHistoryBox->setChecked (sharedSearchHistory_);
    connect (ui->searchHistoryBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefSearchHistory);

    ui->statusBox->setChecked (config.getShowStatusbar());
    connect (ui->statusBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefStatusbar);

    ui->statusCursorsBox->setChecked (config.getShowCursorPos());
    connect (ui->statusCursorsBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefStatusCursor);

    // no ccombo onnection because of mouse wheel; config is set at closeEvent() instead
    ui->tabCombo->setCurrentIndex (config.getTabPosition());

    ui->tabBox->setChecked (config.getTabWrapAround());
    connect (ui->tabBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefTabWrapAround);

    ui->singleTabBox->setChecked (config.getHideSingleTab());
    connect (ui->singleTabBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefHideSingleTab);

    ui->windowBox->setChecked (config.getOpenInWindows());
    ui->windowBox->setEnabled (!static_cast<FPsingleton*>(qApp)->isStandAlone());
    connect (ui->windowBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefOpenInWindows);

    ui->nativeDialogBox->setChecked (config.getNativeDialog());
    connect (ui->nativeDialogBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefNativeDialog);

    ui->sidePaneBox->setChecked (config.getSidePaneMode());
    ui->sidePaneSizeBox->setChecked (config.getRemSplitterPos());
    connect (ui->sidePaneBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefSidePaneMode);
    connect (ui->sidePaneSizeBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefSplitterPos);

    ui->lastTabBox->setChecked (config.getCloseWithLastTab());
    connect (ui->lastTabBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefCloseWithLastTab);

    ui->accelBox->setChecked (disableMenubarAccel_);
    connect (ui->accelBox, &CHECKBOX_CHANGED, this, &PrefDialog::disableMenubarAccel);

    ui->iconBox->setChecked (sysIcons_);
    connect (ui->iconBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefIcon);

    /************
     *** Text ***
     ************/

    ui->fontBox->setChecked (config.getRemFont());
    connect (ui->fontBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefFont);

    ui->wrapBox->setChecked (config.getWrapByDefault());
    connect (ui->wrapBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefWrap);

    ui->indentBox->setChecked (config.getIndentByDefault());
    connect (ui->indentBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefIndent);

    ui->autoBracketBox->setChecked (config.getAutoBracket());
    connect (ui->autoBracketBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefAutoBracket);

    ui->autoReplaceBox->setChecked (config.getAutoReplace());
    connect (ui->autoReplaceBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefAutoReplace);

    ui->lineBox->setChecked (config.getLineByDefault());
    connect (ui->lineBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefLine);

    ui->syntaxBox->setChecked (config.getSyntaxByDefault());
    connect (ui->syntaxBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefSyntax);

    ui->enforceSyntaxBox->setChecked (config.getShowLangSelector());
    ui->enforceSyntaxBox->setEnabled (config.getSyntaxByDefault());

    ui->whiteSpaceBox->setChecked (config.getShowWhiteSpace());
    connect (ui->whiteSpaceBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefWhiteSpace);

    ui->vLineBox->setChecked (vLineDistance_ >= 10);
    connect (ui->vLineBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefVLine);
    ui->vLineSpin->setEnabled (vLineDistance_ >= 10);
    ui->vLineSpin->setValue (std::abs (vLineDistance_));
    connect (ui->vLineSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefVLineDistance);
    if (auto le = ui->vLineSpin->findChild<QLineEdit*>())
    {
        connect (le, &QLineEdit::textChanged, this, [this, config] (const QString &txt) {
            if (txt.isEmpty())
                ui->vLineSpin->setValue (config.getDefaultVLineDistance());
        });
    }

    ui->endingsBox->setChecked (config.getShowEndings());
    connect (ui->endingsBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefEndings);

    ui->marginBox->setChecked (config.getTextMargin());
    connect (ui->marginBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefTextMargin);

    ui->colBox->setChecked (config.getDarkColScheme());
    connect (ui->colBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefDarkColScheme);
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

    ui->selHighlightBox->setChecked (selHighlighting_);

    ui->dateEdit->setText (config.getDateFormat());

    ui->lastLineBox->setChecked (config.getAppendEmptyLine());
    connect (ui->lastLineBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefAppendEmptyLine);

    ui->trailingSpacesBox->setChecked (config.getRemoveTrailingSpaces());
    connect (ui->trailingSpacesBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefRemoveTrailingSpaces);

    ui->skipNonTextBox->setChecked (config.getSkipNonText());
    connect (ui->skipNonTextBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefSkipNontext);

    ui->pastePathsBox->setChecked (pastePaths_);

    ui->spinBox->setValue (config.getMaxSHSize());
    connect (ui->spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefMaxSHSize);
    if (auto le = ui->spinBox->findChild<QLineEdit*>())
    {
        connect (le, &QLineEdit::textChanged, this, [this, config] (const QString &txt) {
            if (txt == ui->spinBox->suffix())
                ui->spinBox->setValue (config.getDefaultMaxSHSize());
        });
    }

    ui->inertiaBox->setChecked (config.getInertialScrolling());
    connect (ui->inertiaBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefInertialScrolling);

    ui->textTabSpin->setValue (textTabSize_);
    connect (ui->textTabSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefTextTabSize);

    ui->dictEdit->setText (config.getDictPath());
    connect (ui->dictButton, &QAbstractButton::clicked, this, &PrefDialog::addDict);
    connect (ui->dictEdit, &QLineEdit::editingFinished, this, &PrefDialog::addDict);
    ui->spellBox->setChecked (!config.getSpellCheckFromStart());
    connect (ui->spellBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefSpellCheck);

    /*************
     *** Files ***
     *************/

    ui->exeBox->setChecked (config.getExecuteScripts());
    connect (ui->exeBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefExecute);
    ui->commandEdit->setText (config.getExecuteCommand());
    ui->commandEdit->setEnabled (config.getExecuteScripts());
    ui->commandLabel->setEnabled (config.getExecuteScripts());
    connect (ui->commandEdit, &QLineEdit::textEdited, this, &PrefDialog::prefCommand);

    ui->recentSpin->setValue (config.getRecentFilesNumber());
    ui->recentSpin->setSuffix (" " + (ui->recentSpin->value() > 1 ? tr ("files") : tr ("file")));
    connect (ui->recentSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::prefRecentFilesNumber);
    if (auto le = ui->recentSpin->findChild<QLineEdit*>())
    {
        connect (le, &QLineEdit::textChanged, this, [this, config] (const QString &txt) {
            if (txt == ui->recentSpin->suffix())
                ui->recentSpin->setValue (config.getDefaultRecentFilesNumber());
        });
    }

    ui->lastFilesBox->setChecked (config.getSaveLastFilesList());
    connect (ui->lastFilesBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefSaveLastFilesList);

    ui->openedButton->setChecked (config.getRecentOpened());
    // no QButtonGroup connection because we want to see if we should clear the recent list at the end

    ui->autoSaveBox->setChecked (config.getAutoSave());
    ui->autoSaveSpin->setValue (config.getAutoSaveInterval());
    ui->autoSaveSpin->setEnabled (ui->autoSaveBox->isChecked());
    if (auto le = ui->autoSaveSpin->findChild<QLineEdit*>())
    {
        connect (le, &QLineEdit::textChanged, this, [this] (const QString &txt) {
            if (txt == ui->autoSaveSpin->suffix())
                ui->autoSaveSpin->setValue (ui->autoSaveSpin->minimum());
        });
    }
    connect (ui->autoSaveBox, &CHECKBOX_CHANGED, this, &PrefDialog::prefAutoSave);

    ui->unmodifiedSaveBox->setChecked (saveUnmodified_);

    /*****************
     *** Shortcuts ***
     *****************/

    if (FPwin *win = static_cast<FPwin *>(parent_))
    {
        if (DEFAULT_SHORTCUTS.isEmpty())
        { // NOTE: Shortcut strings should be in the PortableText format.
            const auto defaultShortcuts = win->defaultShortcuts();
            QHash<QAction*, QKeySequence>::const_iterator iter = defaultShortcuts.constBegin();
            while (iter != defaultShortcuts.constEnd())
            {
                const QString name = iter.key()->objectName();
                DEFAULT_SHORTCUTS.insert (name, iter.value().toString());
                OBJECT_NAMES.insert (iter.key()->text().remove (QRegularExpression ("\\s*\\(&[a-zA-Z0-9]\\)\\s*")).remove ("&"), name);
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
        if (!val.at (i).isEmpty() && val.indexOf (val.at (i), i + 1) > -1)
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
        /* shortcut texts should be added in the NativeText format */
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

    /*********************
     *** Syntax Colors ***
     *********************/

    static QHash<QString, QString> syntaxNames;
    if (syntaxNames.isEmpty())
    { // it's a shame that QObject::tr() doesn't work in FeatherPad::Config
        syntaxNames.insert ("function", tr ("Functions, URLs,…"));
        syntaxNames.insert ("BuiltinFunction", tr ("Built-in Functions"));
        syntaxNames.insert ("comment", tr ("Comments"));
        syntaxNames.insert ("quote", tr ("Quotations"));
        syntaxNames.insert ("type", tr ("Types"));
        syntaxNames.insert ("keyWord", tr ("Key Words"));
        syntaxNames.insert ("number", tr ("Numbers"));
        syntaxNames.insert ("regex", tr ("Regular Expressions, Code Blocks,…"));
        syntaxNames.insert ("xmlElement", tr ("Document Blocks, XML/HTML Elements,…"));
        syntaxNames.insert ("cssValue", tr ("Markdown Headings, CSS Values,…"));
        syntaxNames.insert ("other", tr ("Extra Elements"));
    }

    ui->syntaxTableWidget->setSortingEnabled (false);
    origSyntaxColors_ = !config.customSyntaxColors().isEmpty()
                            ? config.customSyntaxColors()
                            : config.getDarkColScheme() ? config.darkSyntaxColors()
                                                        : config.lightSyntaxColors();
    ui->syntaxTableWidget->setRowCount (origSyntaxColors_.size());
    index = 0;
    QHash<QString, QColor>::const_iterator sIter = origSyntaxColors_.constBegin();
    while (sIter != origSyntaxColors_.constEnd())
    {
        QTableWidgetItem *item = new QTableWidgetItem (syntaxNames.value (sIter.key()));
        item->setData (Qt::UserRole, sIter.key()); // remember syntax independently of translations
        item->setFlags (item->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
        ui->syntaxTableWidget->setItem (index, 0, item);

        QWidget *container = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout (container);
        layout->setAlignment (Qt::AlignCenter);
        layout->setContentsMargins (3, 3, 3, 3);
        QLabel *label = new QLabel();
        label->setMinimumWidth (100);
        label->setSizePolicy (QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        QColor col = sIter.value();
        label->setStyleSheet (QString ("QLabel {background-color: rgb(%1, %2, %3);}")
                              .arg (col.red()).arg (col.green()).arg (col.blue()));
        layout->addWidget (label);
        container->setLayout (layout);
        ui->syntaxTableWidget->setCellWidget(index, 1, container);
        ++ sIter;
        ++ index;
    }
    ui->syntaxTableWidget->setSortingEnabled (true);
    ui->syntaxTableWidget->setCurrentCell (0, 1);
    connect (ui->syntaxTableWidget, &QTableWidget::cellDoubleClicked, this, &PrefDialog::changeSyntaxColor);
    ui->whiteSpaceSpin->setMinimum (config.getMinWhiteSpaceValue());
    ui->whiteSpaceSpin->setMaximum (config.getMaxWhiteSpaceValue());
    ui->whiteSpaceSpin->setValue (whiteSpaceValue_);
    connect (ui->whiteSpaceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::changeWhitespaceValue);
    if (auto le = ui->whiteSpaceSpin->findChild<QLineEdit*>())
    {
        connect (le, &QLineEdit::textChanged, this, [this, config] (const QString &txt) {
            if (txt.isEmpty())
                ui->whiteSpaceSpin->setValue (config.getDefaultWhiteSpaceValue());
        });
    }
    connect (ui->defaultSyntaxButton, &QAbstractButton::clicked, this, &PrefDialog::restoreDefaultSyntaxColors);
    ui->defaultSyntaxButton->setDisabled (config.customSyntaxColors().isEmpty()
                                          && whiteSpaceValue_ == config.getDefaultWhiteSpaceValue()
                                          && curLineHighlight_ == -1);
    /* also, context menus for changing syntax colors */
    ui->syntaxTableWidget->setContextMenuPolicy (Qt::CustomContextMenu);
    connect (ui->syntaxTableWidget, &QWidget::customContextMenuRequested, [this] (const QPoint& p) {
        QModelIndex index = ui->syntaxTableWidget->indexAt (p);
        if (!index.isValid() || index.column() != 1) return;
        QMenu menu;
        QAction *action = menu.addAction (tr ("Select Syntax Color"));
        connect (action, &QAction::triggered, this, [this, index] {
            changeSyntaxColor (index.row(), 1);
        });
        menu.exec (ui->syntaxTableWidget->viewport()->mapToGlobal (p));
    });

    /* this isn't about syntax but comes here because
       it has different values for light and dark color schemes */
    ui->curLineSpin->setMinimum (config.getMinCurLineHighlight() - 1); // for the special text
    ui->curLineSpin->setMaximum (config.getMaxCurLineHighlight());
    ui->curLineSpin->setValue (curLineHighlight_);
    connect (ui->curLineSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::changeCurLineHighlight);
    if (auto le = ui->curLineSpin->findChild<QLineEdit*>())
    {
        connect (le, &QLineEdit::textChanged, this, [this] (const QString &txt) {
            if (txt.isEmpty())
                ui->curLineSpin->setValue (ui->curLineSpin->minimum());
        });
    }

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

        /* don't let spin and combo boxes accept wheel events when not focused */
        if (qobject_cast<QSpinBox *>(w) || qobject_cast<QComboBox *>(w))
        {
            w->setFocusPolicy (Qt::StrongFocus);
            w->installEventFilter (this);
        }
    }

    if (parent != nullptr)
        ag -= parent->window()->frameGeometry().size() - parent->window()->geometry().size();
    if (config.getPrefSize().isEmpty())
    {
        resize (QSize (sizeHint().width() + style()->pixelMetric (QStyle::PM_ScrollBarExtent), size().height())
                .boundedTo(ag));
    }
    else
        resize (config.getPrefSize().boundedTo(ag));
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
    prefSelHighlight();
    prefPastePaths();

    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.setPrefSize (size());
    config.writeConfig();
}
/*************************/
void PrefDialog::showPrompt (const QString& str, bool temporary)
{
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (!str.isEmpty())
    { // show the provided message
        ui->promptLabel->setText ("<b>" + str + "</b>");
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
                    }
                    else showPrompt();
                });
            }
            promptTimer_->start (5000);
        }
        else
            prevtMsg_ = "<b>" + str + "</b>";
    }
    else if (recentNumber_ != config.getRecentFilesNumber()
             || sharedSearchHistory_ != config.getSharedSearchHistory())
    {
        ui->promptLabel->setText ("<b>" + tr ("Application restart is needed for changes to take effect.") + "</b>");
    }
    else if (darkBg_ != config.getDarkColScheme()
             || (darkBg_ && darkColValue_ != config.getDarkBgColorValue())
             || (!darkBg_ && lightColValue_ != config.getLightBgColorValue())
             || showWhiteSpace_ != config.getShowWhiteSpace()
             || showEndings_ != config.getShowEndings()
             || textMargin_ != config.getTextMargin()
             || textTabSize_ != config.getTextTabSize()
             || (vLineDistance_ * config.getVLineDistance() < 0
                 || (vLineDistance_ > 0 && vLineDistance_ != config.getVLineDistance()))
             || whiteSpaceValue_ != config.getWhiteSpaceValue()
             || disableMenubarAccel_ != config.getDisableMenubarAccel()
             || sysIcons_ != config.getSysIcons()
             || curLineHighlight_ != config.getCurLineHighlight()
             || origSyntaxColors_ != (!config.customSyntaxColors().isEmpty()
                                          ? config.customSyntaxColors()
                                          : config.getDarkColScheme()
                                                ? config.darkSyntaxColors()
                                                : config.lightSyntaxColors()))
    {
        ui->promptLabel->setText ("<b>" + tr ("Window reopening is needed for changes to take effect.") + "</b>");
    }
    else
    {
        if (prevtMsg_.isEmpty()) // clear prompt
        {
            ui->promptLabel->clear();
            ui->promptLabel->hide();
            return;
        }
        else // show the previous message
            ui->promptLabel->setText (prevtMsg_);
    }
    ui->promptLabel->show();
}
/*************************/
void PrefDialog::showWhatsThis()
{
    QWhatsThis::enterWhatsThisMode();
}
/*************************/
bool PrefDialog::eventFilter (QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Wheel
       && (qobject_cast<QSpinBox *>(object) || qobject_cast<QComboBox *>(object))
       && !qobject_cast<QWidget *>(object)->hasFocus())
    {
        /* Don't let unfocused spin and combo boxes accept wheel events;
           let the parent widget scroll instead.
           NOTE: Sending the wheel event to the parent widget wasn't
                 needed with Qt5, but the behavior has changed in Qt6. */
        if (auto p = qobject_cast<QWidget *>(object)->parentWidget())
        {
            if (QWheelEvent *we = static_cast<QWheelEvent *>(event))
                QCoreApplication::sendEvent (p, we);
        }
        return true;
    }
    return QDialog::eventFilter (object, event);
}
/*************************/
void PrefDialog::prefSize (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
    {
        config.setRemSize (true);
        if (FPwin *win = static_cast<FPwin *>(parent_))
        {
            config.setWinSize (win->size());
            config.setIsMaxed (win->isMaximized());
            config.setIsFull (win->isFullScreen());
        }
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
    {
        config.setRemPos (true);
        if (FPwin *win = static_cast<FPwin *>(parent_))
            config.setWinPos (win->geometry().topLeft());
    }
    else if (checked == Qt::Unchecked)
        config.setRemPos (false);
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
            singleton->Wins.at (i)->menubarTitle (false);
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
            if (config.getMenubarTitle())
                singleton->Wins.at (i)->menubarTitle (true, true);
        }
    }
}
/*************************/
void PrefDialog::prefMenubarTitle (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
    {
        config.setMenubarTitle (true);
        if (!config.getNoMenubar())
        {
            for (int i = 0; i < singleton->Wins.count(); ++i)
                singleton->Wins.at (i)->menubarTitle (true, true);
        }
    }
    else if (checked == Qt::Unchecked)
    {
        config.setMenubarTitle (false);
        if (!config.getNoMenubar())
        {
            for (int i = 0; i < singleton->Wins.count(); ++i)
                singleton->Wins.at (i)->menubarTitle (false);
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
void PrefDialog::prefSearchHistory (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setSharedSearchHistory (true);
    else if (checked == Qt::Unchecked)
        config.setSharedSearchHistory (false);

    showPrompt();
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
                        connect (thisTextEdit, &TextEdit::selChanged, win, &FPwin::statusMsg);
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
        config.resetFont();
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
        if (!config.getAutoBracket())
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
    }
    else if (checked == Qt::Unchecked)
    {
        if (config.getAutoBracket())
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
}
/*************************/
void PrefDialog::prefAutoReplace (int checked)
{
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    if (checked == Qt::Checked)
    {
        if (!config.getAutoReplace())
        {
            config.setAutoReplace (true);
            for (int i = 0; i < singleton->Wins.count(); ++i)
            {
                int count = singleton->Wins.at (i)->ui->tabWidget->count();
                for (int j = 0; j < count; ++j)
                {
                    qobject_cast< TabPage *>(singleton->Wins.at (i)->ui->tabWidget->widget (j))
                        ->textEdit()->setAutoReplace (true);
                }
            }
        }
    }
    else if (checked == Qt::Unchecked)
    {
        if (config.getAutoReplace())
        {
            config.setAutoReplace (false);
            for (int i = 0; i < singleton->Wins.count(); ++i)
            {
                int count = singleton->Wins.at (i)->ui->tabWidget->count();
                for (int j = 0; j < count; ++j)
                {
                    qobject_cast< TabPage *>(singleton->Wins.at (i)->ui->tabWidget->widget (j))
                        ->textEdit()->setAutoReplace (false);
                }
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
    int dsitance = std::clamp (ui->vLineSpin->value(), 10, 999);
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
    int dsitance = std::clamp (value, 10, 999);
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
void PrefDialog::prefTextMargin (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setTextMargin (true);
    else if (checked == Qt::Unchecked)
        config.setTextMargin (false);

    showPrompt();
}
/*************************/
void PrefDialog::prefDarkColScheme (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();

    prefCustomSyntaxColors_.clear(); // forget customized syntax colors

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

    /* There are different syntax color settings for light and dark color schemes.
       So, the syntax colors should be read again. */
    config.readSyntaxColors();

    /* these values should be reset before the prompt is shown */
    whiteSpaceValue_ = config.getWhiteSpaceValue();
    curLineHighlight_ = config.getCurLineHighlight();
    origSyntaxColors_ = !config.customSyntaxColors().isEmpty()
                            ? config.customSyntaxColors()
                            : config.getDarkColScheme() ? config.darkSyntaxColors()
                                                        : config.lightSyntaxColors();
    showPrompt();

    /* update the state of default button */
    ui->defaultSyntaxButton->setEnabled (!config.customSyntaxColors().isEmpty()
                                         || config.getWhiteSpaceValue() != config.getDefaultWhiteSpaceValue()
                                         || config.getCurLineHighlight() != -1);
    /* update row colors */
    for (int i = 0; i < ui->syntaxTableWidget->rowCount(); ++i)
    {
        if (QTableWidgetItem *item = ui->syntaxTableWidget->item (i, 0))
        {
            QString syntax = item->data (Qt::UserRole).toString();
            QColor col;
            if (!config.customSyntaxColors().isEmpty()) // customization was done before
                col = config.customSyntaxColors().value (syntax);
            else // no custom syntax color
            {
                if (config.getDarkColScheme())
                    col = config.darkSyntaxColors().value (syntax);
                else
                    col = config.lightSyntaxColors().value (syntax);
            }
            if (const auto cw = ui->syntaxTableWidget->cellWidget (i, 1))
            {
                if (cw->layout())
                {
                    if (const auto label = qobject_cast<QLabel*>(cw->layout()->itemAt (0)->widget()))
                    {
                        label->setStyleSheet (QString ("QLabel {background-color: rgb(%1, %2, %3);}")
                                              .arg (col.red()).arg (col.green()).arg (col.blue()));
                    }
                }
            }
        }
    }
    /* update whiteSpace spin box */
    disconnect (ui->whiteSpaceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::changeWhitespaceValue);
    ui->whiteSpaceSpin->setMinimum (config.getMinWhiteSpaceValue());
    ui->whiteSpaceSpin->setMaximum (config.getMaxWhiteSpaceValue());
    ui->whiteSpaceSpin->setValue (config.getWhiteSpaceValue());
    connect (ui->whiteSpaceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::changeWhitespaceValue);

    /* also, update current line spin box */
    disconnect (ui->curLineSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::changeCurLineHighlight);
    ui->curLineSpin->setMinimum (config.getMinCurLineHighlight() - 1);
    ui->curLineSpin->setMaximum (config.getMaxCurLineHighlight());
    ui->curLineSpin->setValue (config.getCurLineHighlight());
    connect (ui->curLineSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::changeCurLineHighlight);
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
void PrefDialog::prefSelHighlight()
{
    bool selHighlighting = ui->selHighlightBox->isChecked();
    if (selHighlighting == selHighlighting_)
        return;
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    config.setSelectionHighlighting (selHighlighting);
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        int count = singleton->Wins.at (i)->ui->tabWidget->count();
        for (int j = 0; j < count; ++j)
        {
            qobject_cast< TabPage *>(singleton->Wins.at (i)->ui->tabWidget->widget (j))
                ->textEdit()->setSelectionHighlighting (selHighlighting);
        }
    }
}
/*************************/
void PrefDialog::prefPastePaths()
{
    bool pastePaths = ui->pastePathsBox->isChecked();
    if (pastePaths == pastePaths_)
        return;
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    Config& config = singleton->getConfig();
    config.setPastePaths (pastePaths);
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        int count = singleton->Wins.at (i)->ui->tabWidget->count();
        for (int j = 0; j < count; ++j)
        {
            qobject_cast< TabPage *>(singleton->Wins.at (i)->ui->tabWidget->widget (j))
                ->textEdit()->setPastePaths (pastePaths);
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
    QString desc = ui->tableWidget->item (ui->tableWidget->currentRow(), 0)->text();

    QString txt = item->text();
    if (!txt.isEmpty())
    {
        /* the QKeySequenceEdit text is in the NativeText format but it
           should be converted into the PortableText format for saving */
        QKeySequence keySeq (txt);
        txt = keySeq.toString();
    }

    if (!txt.isEmpty() && config.reservedShortcuts().contains (txt)
        /* unless its (hard-coded) default shortcut is typed */
        && DEFAULT_SHORTCUTS.value (OBJECT_NAMES.value (desc)) != txt)
    {
        showPrompt (tr ("The typed shortcut was reserved."), true);
        disconnect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
        item->setText (shortcuts_.value (desc));
        connect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
    }
    else
    {
        shortcuts_.insert (desc, txt);
        newShortcuts_.insert (OBJECT_NAMES.value (desc), txt);

        /* check for ambiguous shortcuts */
        bool ambiguous = false;
        QList<QString> val = shortcuts_.values();
        for (int i = 0; i < val.size(); ++i)
        {
            if (!val.at (i).isEmpty() && val.indexOf (val.at (i), i + 1) > -1)
            {
                showPrompt (tr ("Warning: Ambiguous shortcut detected!"), false);
                ambiguous = true;
                break;
            }
        }
        if (!ambiguous && ui->promptLabel->isVisible())
        {
            prevtMsg_ = QString();
            showPrompt();
        }

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
    if (ui->unmodifiedSaveBox->isChecked() == saveUnmodified_)
        return; // do nothing when it isn't changed
    config.setSaveUnmodified (!saveUnmodified_);
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        FPwin *win = singleton->Wins.at (i);
        if (TabPage *tabPage = qobject_cast<TabPage*>(win->ui->tabWidget->currentWidget()))
        {
            TextEdit *textEdit = tabPage->textEdit();
            if (!saveUnmodified_) // meaans that unmodified docs can be saved now
            {
                if (!textEdit->isReadOnly() && !textEdit->isUneditable())
                    win->ui->actionSave->setEnabled (true);
            }
            else
                win->ui->actionSave->setEnabled (textEdit->document()->isModified());
        }
        for (int j = 0; j < win->ui->tabWidget->count(); ++j)
        {
            TextEdit *textEdit = qobject_cast< TabPage *>(win->ui->tabWidget->widget (j))->textEdit();
            if (!saveUnmodified_)
                disconnect (textEdit->document(), &QTextDocument::modificationChanged, win, &FPwin::enableSaving);
            else
                connect (textEdit->document(), &QTextDocument::modificationChanged, win, &FPwin::enableSaving);
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
/*************************/
void PrefDialog::prefSpellCheck (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setSpellCheckFromStart (false);
    else if (checked == Qt::Unchecked)
        config.setSpellCheckFromStart (true);
}
/*************************/
void PrefDialog::addDict()
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (QObject::sender() == ui->dictEdit)
    {
        config.setDictPath (ui->dictEdit->text());
        return;
    }
    FileDialog dialog (this, config.getNativeDialog());
    dialog.setAcceptMode (QFileDialog::AcceptOpen);
    dialog.setWindowTitle (tr ("Add dictionary..."));
    dialog.setFileMode (QFileDialog::ExistingFile);
    dialog.setNameFilter (tr ("Hunspell Dictionary Files") + " (*.dic)");
    QString path = ui->dictEdit->text();
    if (path.isEmpty())
    {
        path = "/usr/share/hunspell";
        if (!QFileInfo (path).isDir())
            path = "/usr/local/share/hunspell";
    }

    if (QFileInfo (path).isDir())
        dialog.setDirectory (path);
    else if (QFile::exists (path))
    {
        dialog.setDirectory (path.section ("/", 0, -2));
        dialog.selectFile (path);
        dialog.autoScroll();
    }

    if (dialog.exec())
    {
        const QStringList files = dialog.selectedFiles();
        if (!files.isEmpty())
        {
            ui->dictEdit->setText (files.at (0));
            config.setDictPath (files.at (0));
        }
    }
}
/*************************/
void PrefDialog::restoreDefaultSyntaxColors()
{
    prefCustomSyntaxColors_.clear();
    ui->defaultSyntaxButton->setDisabled (true);
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.setCustomSyntaxColors (prefCustomSyntaxColors_);
    config.setWhiteSpaceValue (config.getDefaultWhiteSpaceValue());
    config.setCurLineHighlight (-1);
    /* update row colors */
    for (int i = 0; i < ui->syntaxTableWidget->rowCount(); ++i)
    {
        if (QTableWidgetItem *item = ui->syntaxTableWidget->item (i, 0))
        {
            QString syntax = item->data (Qt::UserRole).toString();
            QColor col;
            if (config.getDarkColScheme())
                col = config.darkSyntaxColors().value (syntax);
            else
                col = config.lightSyntaxColors().value (syntax);
            if (const auto cw = ui->syntaxTableWidget->cellWidget (i, 1))
            {
                if (cw->layout())
                {
                    if (const auto label = qobject_cast<QLabel*>(cw->layout()->itemAt (0)->widget()))
                    {
                        label->setStyleSheet (QString ("QLabel {background-color: rgb(%1, %2, %3);}")
                                              .arg (col.red()).arg (col.green()).arg (col.blue()));
                    }
                }
            }
        }
    }
    /* update whiteSpace value */
    disconnect (ui->whiteSpaceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::changeWhitespaceValue);
    ui->whiteSpaceSpin->setValue (config.getWhiteSpaceValue());
    connect (ui->whiteSpaceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::changeWhitespaceValue);

    /* also, update current line bg value */
    disconnect (ui->curLineSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::changeCurLineHighlight);
    ui->curLineSpin->setValue (config.getMinCurLineHighlight() - 1);
    connect (ui->curLineSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PrefDialog::changeCurLineHighlight);

    showPrompt();
}
/*************************/
void PrefDialog::changeSyntaxColor (int row, int column)
{
    if (column != 1) return;
    if (const auto cw = ui->syntaxTableWidget->cellWidget (row, column))
    {
        if (cw->layout())
        {
            if (const auto label = qobject_cast<QLabel*>(cw->layout()->itemAt (0)->widget()))
            {
                QColor prevColor = label->palette().color (QPalette::Window);
                QColor color = QColorDialog::getColor (prevColor,
                                                       this,
                                                       tr ("Select Syntax Color"));
                if (color.isValid() && color != prevColor)
                {
                    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
                    if (QTableWidgetItem *item = ui->syntaxTableWidget->item (row, 0))
                    {
                        QString syntax = item->data (Qt::UserRole).toString();

                        if (prefCustomSyntaxColors_.isEmpty()) // first customization in Preferences
                        {
                            if (!config.customSyntaxColors().isEmpty())
                                prefCustomSyntaxColors_ = config.customSyntaxColors();
                            else if (config.getDarkColScheme())
                                prefCustomSyntaxColors_ = config.darkSyntaxColors();
                            else
                                prefCustomSyntaxColors_ = config.lightSyntaxColors();
                        }
                        prefCustomSyntaxColors_.remove (syntax); // will be added correctly below
                        auto colors = prefCustomSyntaxColors_.values();
                        int ws = config.getWhiteSpaceValue();
                        colors << QColor (ws, ws, ws);
                        colors << (config.getDarkColScheme() ? QColor (Qt::white) : QColor (Qt::black));
                        /* modify the color if it already exists */
                        int r = color.red(); int g = color.green(); int b = color.blue();
                        while (colors.contains (color))
                        {
                            color = QColor (r > 127 ? color.red() - 1 : color.red() + 1,
                                            g > 127 ? color.green() - 1 : color.green() + 1,
                                            b > 127 ? color.blue() - 1 : color.blue() + 1);
                        }
                        prefCustomSyntaxColors_.insert (syntax, color);
                        /* also, set the row color */
                        label->setStyleSheet (QString ("QLabel {background-color: rgb(%1, %2, %3);}")
                                              .arg (color.red()).arg (color.green()).arg (color.blue()));
                    }

                    /* apply customization immediately for the user to be able to check it in a new window */
                    if (prefCustomSyntaxColors_ ==
                        (config.getDarkColScheme() ? config.darkSyntaxColors()
                                                   : config.lightSyntaxColors())) // no customization
                    {
                        prefCustomSyntaxColors_.clear();
                        ui->defaultSyntaxButton->setEnabled (config.getWhiteSpaceValue() != config.getDefaultWhiteSpaceValue()
                                                             || config.getCurLineHighlight() != -1);
                    }
                    else
                        ui->defaultSyntaxButton->setEnabled (true);
                    config.setCustomSyntaxColors (prefCustomSyntaxColors_);
                    showPrompt();
                }
            }
        }
    }
}
/*************************/
void PrefDialog::changeWhitespaceValue (int value)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.setWhiteSpaceValue (value); // takes care of repeated colors
    ui->defaultSyntaxButton->setEnabled (!config.customSyntaxColors().isEmpty()
                                         || config.getWhiteSpaceValue() != config.getDefaultWhiteSpaceValue()
                                         || config.getCurLineHighlight() != -1);
    showPrompt();
}
/*************************/
void PrefDialog::changeCurLineHighlight (int value)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    config.setCurLineHighlight (value);
    ui->defaultSyntaxButton->setEnabled (!config.customSyntaxColors().isEmpty()
                                         || config.getWhiteSpaceValue() != config.getDefaultWhiteSpaceValue()
                                         || config.getCurLineHighlight() != -1);
    showPrompt();
}
/*************************/
void PrefDialog::disableMenubarAccel (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setDisableMenubarAccel (true);
    else if (checked == Qt::Unchecked)
        config.setDisableMenubarAccel (false);

    showPrompt();
}
/*************************/
void PrefDialog::prefIcon (int checked)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (checked == Qt::Checked)
        config.setSysIcons (true);
    else if (checked == Qt::Unchecked)
        config.setSysIcons (false);

    showPrompt();
}

}
