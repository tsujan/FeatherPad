/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2025 <tsujan2000@gmail.com>
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

#ifndef FPWIN_H
#define FPWIN_H

#include <QMainWindow>
#include <QActionGroup>
#include <QElapsedTimer>
#include "highlighter/highlighter.h"
#include "textedit.h"
#include "tabpage.h"
#include "sidepane.h"
#include "config.h"

namespace FeatherPad {

namespace Ui {
class FPwin;
}

class WarningBar;

// A FeatherPad window.
class FPwin : public QMainWindow
{
    Q_OBJECT

public:
    explicit FPwin (QWidget *parent = nullptr);
    ~FPwin();

    void cleanUpOnTerminating (Config &config, bool isLastWin);

    bool isScriptLang (const QString& lang) const;

    bool isLoading() const {
        return (loadingProcesses_ > 0);
    }
    bool isReady() {
        if (loadingProcesses_ <= 0)
        {
            closeWarningBar();
            return true;
        }
        return false;
    }

    bool hasSidePane() const {
        return (sidePane_ != nullptr);
    }

    bool isLocked() const {
        return locked_;
    }

    void addCursorPosLabel();
    void addRemoveLangBtn (bool add);

    void showRootWarning();
    void updateCustomizableShortcuts (bool disable = false);

    void startAutoSaving (bool start, int interval = 1);

    QHash<QAction*, QKeySequence> defaultShortcuts() const {
        return defaultShortcuts_;
    }

    void menubarTitle (bool add = true, bool setTitle = false);

signals:
    void finishedLoading();

public slots:
    void newTabFromName (const QString& fileName,
                         int restoreCursor, /* ==  0 : Do not restore cursor.
                                               ==  1 : Restore cursor in a session file.
                                               == -1 : Restore cursor while opening last files with application startup.
                                               <  -1 : Move the cursor to the document end with command-line.
                                               >   1 : Move the cursor to the block restoreCursor-1 with command-line. */
                         int posInLine, // If restoreCursor > 1, this is the cursor position in the line.
                         bool multiple = false);
    void newTab();
    void statusMsg();
    void statusMsgWithLineCount (const int lines);
    void showCursorPos();
    void updateWordInfo (int position = -1, int charsRemoved = 0, int charsAdded = 0);
    void enableSaving (bool modified);

private slots:
    void newTabFromRecent();
    void clearRecentMenu();
    void updateRecenMenu();
    void closePage();
    void closeTabAtIndex (int tabIndex);
    void copyTabFileName();
    void copyTabFilePath();
    void closeAllPages();
    void closeNextPages();
    void closePreviousPages();
    void closeOtherPages();
    void fileOpen();
    void reload();
    void enforceEncoding (QAction *a);
    void cutText();
    void copyText();
    void pasteText();
    void toSoftTabs();
    void insertDate();
    void deleteText();
    void selectAllText();
    void upperCase();
    void lowerCase();
    void startCase();
    void showingEditMenu();
    void hidngEditMenu();
    void sortLines();
    void makeEditable();
    void undoing();
    void redoing();
    void onTabChanged (int index);
    void tabSwitch (int index);
    void fontDialog();
    void find (bool forward);
    void hlight() const;
    void searchFlagChanged();
    void showHideSearch();
    void showLN (bool checked);
    void toggleSyntaxHighlighting();
    void formatOnBlockChange (int) const;
    void formatOnTextChange (int, int charsRemoved, int charsAdded) const;
    void formatTextRect() const;
    void toggleWrapping();
    void toggleIndent();
    void replace();
    void replaceAll();
    void dockVisibilityChanged (bool visible);
    void replaceDock();
    void resizeDock (bool topLevel);
    void jumpTo();
    void setMax (const int max);
    void goTo();
    void asterisk (bool modified);
    void reformat (TextEdit *textEdit);
    void zoomIn();
    void zoomOut();
    void zoomZero();
    void defaultSize();
    void focusView();
    void focusSidePane();
    //void align();
    void manageSessions();
    void executeProcess();
    void exitProcess();
    void displayError();
    void displayOutput();
    void docProp();
    void filePrint();
    void printing();
    void detachTab();
    void nextTab();
    void previousTab();
    void lastTab();
    void firstTab();
    void lastActiveTab();
    void tabContextMenu (const QPoint& p);
    void listContextMenu (const QPoint& p);
    void editorContextMenu (const QPoint& p);
    void changeTab (QListWidgetItem *current);
    void toggleSidePane();
    void prefDialog();
    void checkSpelling();
    void userDict();
    void aboutDialog();
    void helpDoc();
    void matchBrackets();
    void addText (const QString& text, const QString& fileName, const QString& charset,
                  bool enforceEncod, bool reload,
                  int restoreCursor, int posInLine,
                  bool uneditable, // This doc should be uneditable?
                  bool multiple); // Multiple files are being loaded?
    void onOpeningHugeFiles();
    void onOpeninNonTextFiles();
    void onPermissionDenied();
    void onOpeningUneditable();
    void onOpeningNonexistent();
    void columnWarning();
    void autoSave();
    void pauseAutoSaving (bool pause);
    void enforceLang (QAction *action);
    void unbusy();

public:
    QWidget *dummyWidget; // Bypasses KDE's demand for a new window.
    Ui::FPwin *ui;

private:
    enum DOCSTATE {
      SAVED,
      UNDECIDED,
      DISCARDED
    };

    TabPage* createEmptyTab(bool setCurrent, bool allowNormalHighlighter = true);
    bool hasAnotherDialog();
    void deleteTabPage (int tabIndex, bool saveToList = false, bool closeWithLastTab = true);
    void loadText (const QString& fileName, bool enforceEncod, bool reload,
                   int restoreCursor = 0, int posInLine = 0,
                   bool enforceUneditable = false, bool multiple = false);
    bool alreadyOpen (TabPage *tabPage) const;
    void setWinTitle (const QString& title);
    void setTitle (const QString& fileName, int tabIndex = -1);
    DOCSTATE savePrompt (int tabIndex, bool noToAll,
                         int first = 0, int last = 0, bool closingWindow = false,
                         QListWidgetItem *curItem = nullptr, TabPage *curPage = nullptr);
    bool saveFile (bool keepSyntax,
                   int first = 0, int last = 0, bool closingWindow = false,
                   QListWidgetItem *curItem = nullptr, TabPage *curPage = nullptr);
    void saveAsRoot (const QString& fileName, TabPage *tabPage,
                     int first, int last, bool closingWindow,
                     QListWidgetItem *curItem, TabPage *curPage,
                     bool MSWinLineEnd);
    void reloadSyntaxHighlighter (TextEdit *textEdit);
    void lockWindow (TabPage *tabPage, bool lock, bool blur = false);
    void saveAllFiles (bool showWarning);
    void closeEvent (QCloseEvent *event);
    bool closePages (int first, int last, bool saveFilesList = false);
    void dragEnterEvent (QDragEnterEvent *event);
    void dropEvent (QDropEvent *event);
    void dropTab (const QString& str, QObject *source);
    void changeEvent (QEvent *event);
    void showEvent (QShowEvent *event);
    bool event (QEvent *event);
    QTextDocument::FindFlags getSearchFlags() const;
    void enableWidgets (bool enable) const;
    void updateShortcuts (bool disable, bool page = true);
    void setProgLang (TextEdit *textEdit);
    void syntaxHighlighting (TextEdit *textEdit, bool highlight = true, const QString& lang = QString());
    void encodingToCheck (const QString& encoding);
    const QString checkToEncoding() const;
    void applyConfigOnStarting();
    bool matchLeftParenthesis (QTextBlock currentBlock, int index, int numRightParentheses);
    bool matchRightParenthesis (QTextBlock currentBlock, int index, int numLeftParentheses);
    bool matchLeftBrace (QTextBlock currentBlock, int index, int numRightBraces);
    bool matchRightBrace (QTextBlock currentBlock, int index, int numLeftBraces);
    bool matchLeftBracket (QTextBlock currentBlock, int index, int numRightBrackets);
    bool matchRightBracket (QTextBlock currentBlock, int index, int numLeftBrackets);
    void createSelection (int pos);
    void removeGreenSel();
    void makeBusy();
    void displayMessage (bool error);
    WarningBar* showWarningBar (const QString& message, int timeout = 10, bool startupBar = false);
    void closeWarningBar (bool keepOnStartup = false);
    void disconnectLambda();
    void updateLangBtn (TextEdit *textEdit);
    void updateGUIForSingleTab (bool single);
    void stealFocus (QWidget *w);
    void stealFocus();
    void addRecentFile (const QString& file);

    QActionGroup *aGroup_;
    QString lastFile_; // The last opened or saved file (for file dialogs).
    QHash<QString, QVariant> lastWinFilesCur_; // The last window files and their cusrors (if restored).
    int rightClicked_; // The index/row of the right-clicked tab/item.
    int loadingProcesses_; // The number of loading processes (used to prevent early closing).
    QMetaObject::Connection lambdaConnection_; // Captures a lambda connection to disconnect it later.
    SidePane *sidePane_;
    QHash<QListWidgetItem*, TabPage*> sideItems_; // For fast tab switching.
    QHash<QString, QAction*> langs_; // All programming languages (to be enforced by the user).
    QHash<QAction*, QKeySequence> defaultShortcuts_;
    bool inactiveTabModified_; // The inactive tab is modified (e.g., when saving all files).
    bool shownBefore_; // Needed for correct restoring of the position.
    // Auto-saving:
    QTimer *autoSaver_;
    QElapsedTimer autoSaverPause_;
    int autoSaverRemainingTime_;
    // Needed with saving as root:
    bool locked_;
    bool closePreviousPages_;
};

}

#endif // FPWIN_H
