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

#include "config.h"
#include <QFileInfo>
#include <QKeySequence>

namespace FeatherPad {

Config::Config():
    remSize_ (true),
    remPos_ (false),
    remSplitterPos_ (true),
    iconless_ (false),
    sysIcon_ (false),
    noToolbar_ (false),
    noMenubar_ (false),
    hideSearchbar_ (false),
    showStatusbar_ (true),
    showCursorPos_ (false),
    showLangSelector_ (false),
    sidePaneMode_ (false),
    remFont_ (true),
    wrapByDefault_ (true),
    indentByDefault_ (true),
    autoBracket_ (false),
    lineByDefault_ (false),
    syntaxByDefault_ (true),
    showWhiteSpace_ (false),
    showEndings_ (false),
    isMaxed_ (false),
    isFull_ (false),
    darkColScheme_ (false),
    thickCursor_ (false),
    tabWrapAround_ (false),
    hideSingleTab_ (false),
    executeScripts_ (false),
    appendEmptyLine_ (true),
    removeTrailingSpaces_ (false),
    openInWindows_ (false),
    nativeDialog_ (false),
    inertialScrolling_ (false),
    autoSave_ (false),
    scrollJumpWorkaround_ (false),
    skipNonText_ (true),
    saveUnmodified_ (false),
    closeWithLastTab_ (false),
    tabInsertSpaces_ (false),
    vLineDistance_ (-80),
    tabPosition_ (0),
    maxSHSize_ (2),
    lightBgColorValue_ (255),
    darkBgColorValue_ (15),
    recentFilesNumber_ (10),
    curRecentFilesNumber_ (10), // not needed
    autoSaveInterval_ (1), // not needed
    textTabSize_ (4), // not needed
    winSize_ (QSize (700, 500)),
    startSize_ (QSize (700, 500)),
    winPos_ (QPoint (0, 0)),
    splitterPos_ (20), // percentage
    font_ (QFont ("Monospace")),
    recentOpened_ (false),
    saveLastFilesList_ (false),
    cursorPosRetrieved_ (false) {}
/*************************/
Config::~Config() {}
/*************************/
void Config::readConfig()
{
    QVariant v;
    Settings settings ("featherpad", "fp");

    /**************
     *** Window ***
     **************/

    settings.beginGroup ("window");

    if (settings.value ("size") == "none")
        remSize_ = false; // true by default
    else
    {
        winSize_ = settings.value ("size", QSize (700, 500)).toSize();
        if (!winSize_.isValid() || winSize_.isNull())
            winSize_ = QSize (700, 500);
        isMaxed_ = settings.value ("max", false).toBool();
        isFull_ = settings.value ("fullscreen", false).toBool();
    }
    startSize_ = settings.value ("startSize", QSize (700, 500)).toSize();
    if (!startSize_.isValid() || startSize_.isNull())
        startSize_ = QSize (700, 500);

    v = settings.value ("position");
    if (v.isValid() && settings.value ("position") != "none")
    {
        remPos_ = true; // false by default
        winPos_ = settings.value ("position", QPoint (0, 0)).toPoint();
    }

    if (settings.value ("splitterPos") == "none")
        remSplitterPos_ = false; // true by default
    else
        splitterPos_ = qMin (qMax (settings.value ("splitterPos", 20).toInt(), 0), 100);

    if (settings.value ("iconless").toBool())
        iconless_ = true; // false by default

    if (settings.value ("sysIcon").toBool())
        sysIcon_ = true; // false by default

    if (settings.value ("noToolbar").toBool())
        noToolbar_ = true; // false by default

    if (settings.value ("noMenubar").toBool())
        noMenubar_ = true; // false by default

    if (noToolbar_ && noMenubar_)
    { // we don't want to hide all actions
        noToolbar_ = false;
        noMenubar_ = true;
    }

    if (settings.value ("hideSearchbar").toBool())
        hideSearchbar_ = true; // false by default

    v = settings.value ("showStatusbar");
    if (v.isValid()) // true by default
        showStatusbar_ = v.toBool();

    if (settings.value ("showCursorPos").toBool())
        showCursorPos_ = true; // false by default

    if (settings.value ("showLangSelector").toBool())
        showLangSelector_ = true; // false by default

    if (settings.value ("sidePaneMode").toBool())
        sidePaneMode_ = true; // false by default

    int pos = settings.value ("tabPosition").toInt();
    if (pos > 0 && pos <= 3)
        tabPosition_ = pos; // 0 by default

    if (settings.value ("tabWrapAround").toBool())
        tabWrapAround_ = true; // false by default

    if (settings.value ("hideSingleTab").toBool())
        hideSingleTab_ = true; // false by default

    if (settings.value ("openInWindows").toBool())
        openInWindows_ = true; // false by default

    if (settings.value ("nativeDialog").toBool())
        nativeDialog_ = true; // false by default

    if (settings.value ("closeWithLastTab").toBool())
        closeWithLastTab_ = true; // false by default

    if (settings.value ("tabInsertSpaces").toBool())
        tabInsertSpaces_ = true; // false by default

    settings.endGroup();

    /************
     *** Text ***
     ************/

    settings.beginGroup ("text");

    if (settings.value ("font") == "none")
    {
        remFont_ = false; // true by default
        font_.setPointSize (qMax (QFont().pointSize(), 9));
    }
    else
    {
        QString fontStr = settings.value ("font").toString();
        if (!fontStr.isEmpty())
            font_.fromString (fontStr);
        else
            font_.setPointSize (qMax (QFont().pointSize(), 9));
    }

    if (settings.value ("noWrap").toBool())
        wrapByDefault_ = false; // true by default

    if (settings.value ("noIndent").toBool())
        indentByDefault_ = false; // true by default

    if (settings.value ("autoBracket").toBool())
        autoBracket_ = true; // false by default

    if (settings.value ("lineNumbers").toBool())
        lineByDefault_ = true; // false by default

    if (settings.value ("noSyntaxHighlighting").toBool())
        syntaxByDefault_ = false; // true by default

    if (settings.value ("showWhiteSpace").toBool())
        showWhiteSpace_ = true; // false by default

    if (settings.value ("showEndings").toBool())
        showEndings_ = true; // false by default

    if (settings.value ("darkColorScheme").toBool())
        darkColScheme_ = true; // false by default

    if (settings.value ("thickCursor").toBool())
        thickCursor_ = true; // false by default

    if (settings.value ("inertialScrolling").toBool())
        inertialScrolling_ = true; // false by default

    if (settings.value ("autoSave").toBool())
        autoSave_ = true; // false by default

    int distance = settings.value ("vLineDistance").toInt();
    if (qAbs (distance) >= 10 && qAbs (distance) < 1000)
        vLineDistance_ = distance; // -80 by default

    if (settings.value ("scrollJumpWorkaround").toBool())
        scrollJumpWorkaround_ = true; // false by default

    v = settings.value ("skipNonText");
    if (v.isValid()) // true by default
        skipNonText_ = v.toBool();

    if (settings.value ("saveUnmodified").toBool())
        saveUnmodified_ = true; // false by default

    maxSHSize_ = qBound (1, settings.value ("maxSHSize", 2).toInt(), 10);

    /* don't let the dark bg be darker than #e6e6e6 */
    lightBgColorValue_ = qBound (230, settings.value ("lightBgColorValue", 255).toInt(), 255);

    /* don't let the dark bg be lighter than #323232 */
    darkBgColorValue_ = qBound (0, settings.value ("darkBgColorValue", 15).toInt(), 50);

    dateFormat_ = settings.value ("dateFormat").toString();

    if (settings.value ("executeScripts").toBool())
        executeScripts_ = true; // false by default
    executeCommand_ = settings.value ("executeCommand").toString();

    v = settings.value ("appendEmptyLine");
    if (v.isValid()) // true by default
        appendEmptyLine_ = v.toBool();

    if (settings.value ("removeTrailingSpaces").toBool())
        removeTrailingSpaces_ = true; // false by default

    recentFilesNumber_ = qBound (1, settings.value ("recentFilesNumber", 10).toInt(), 20);
    curRecentFilesNumber_ = recentFilesNumber_; // fixed
    recentFiles_ = settings.value ("recentFiles").toStringList();
    recentFiles_.removeAll ("");
    recentFiles_.removeDuplicates();
    while (recentFiles_.count() > recentFilesNumber_)
        recentFiles_.removeLast();
    if (settings.value ("recentOpened").toBool())
        recentOpened_ = true; // false by default

    if (settings.value ("saveLastFilesList").toBool())
        saveLastFilesList_ = true; // false by default

    autoSaveInterval_ = qBound (1, settings.value ("autoSaveInterval", 1).toInt(), 60);

    textTabSize_ = qBound (2, settings.value ("textTabSize", 4).toInt(), 10);

    settings.endGroup();
}
/*************************/
void Config::readShortcuts()
{
    Settings settings ("featherpad", "fp");
    settings.beginGroup ("shortcuts");
    QStringList actions = settings.childKeys();

    for (int i = 0; i < actions.size(); ++i)
    {
        QVariant v = settings.value (actions.at (i));
        QString vs = validatedShortcut (v);
        if (!vs.isEmpty())
            setActionShortcut (actions.at (i), vs);
        else // remove the key on writing config
            removedActions_ << actions.at (i);
    }
    settings.endGroup();
}
/*************************/
QStringList Config::getLastFiles()
{
    if (!saveLastFilesList_) // it's already decided
        return QStringList();

    Settings settingsLastCur ("featherpad", "fp_last_cursor_pos");
    lasFilesCursorPos_ = settingsLastCur.value ("cursorPositions").toHash();

    QStringList lastFiles = lasFilesCursorPos_.keys();
    lastFiles.removeAll ("");
    lastFiles.removeDuplicates();
    while (lastFiles.count() > 50) // never more than 50 files
        lastFiles.removeLast();
    return lastFiles;
}
/*************************/
void Config::writeConfig()
{
    Settings settings ("featherpad", "fp");
    if (!settings.isWritable()) return;

    /**************
     *** Window ***
     **************/

    settings.beginGroup ("window");

    if (remSize_)
    {
        settings.setValue ("size", winSize_);
        settings.setValue ("max", isMaxed_);
        settings.setValue ("fullscreen", isFull_);
    }
    else
    {
        settings.setValue ("size", "none");
        settings.remove ("max");
        settings.remove ("fullscreen");
    }

    if (remPos_)
        settings.setValue ("position", winPos_);
    else
        settings.setValue ("position", "none");

    if (remSplitterPos_)
        settings.setValue ("splitterPos", splitterPos_);
    else
        settings.setValue ("splitterPos", "none");

    settings.setValue ("startSize", startSize_);
    settings.setValue ("iconless", iconless_);
    settings.setValue ("sysIcon", sysIcon_);
    settings.setValue ("noToolbar", noToolbar_);
    settings.setValue ("noMenubar", noMenubar_);
    settings.setValue ("hideSearchbar", hideSearchbar_);
    settings.setValue ("showStatusbar", showStatusbar_);
    settings.setValue ("showCursorPos", showCursorPos_);
    settings.setValue ("showLangSelector", showLangSelector_);
    settings.setValue ("sidePaneMode", sidePaneMode_);
    settings.setValue ("tabPosition", tabPosition_);
    settings.setValue ("tabWrapAround", tabWrapAround_);
    settings.setValue ("hideSingleTab", hideSingleTab_);
    settings.setValue ("openInWindows", openInWindows_);
    settings.setValue ("nativeDialog", nativeDialog_);
    settings.setValue ("closeWithLastTab", closeWithLastTab_);
    settings.setValue ("tabInsertSpaces", tabInsertSpaces_);

    settings.endGroup();

    /************
     *** Text ***
     ************/

    settings.beginGroup ("text");

    if (remFont_)
        settings.setValue ("font", font_.toString());
    else
        settings.setValue ("font", "none");

    settings.setValue ("noWrap", !wrapByDefault_);
    settings.setValue ("noIndent", !indentByDefault_);
    settings.setValue ("autoBracket", autoBracket_);
    settings.setValue ("lineNumbers", lineByDefault_);
    settings.setValue ("noSyntaxHighlighting", !syntaxByDefault_);
    settings.setValue ("showWhiteSpace", showWhiteSpace_);
    settings.setValue ("showEndings", showEndings_);
    settings.setValue ("darkColorScheme", darkColScheme_);
    settings.setValue ("thickCursor", thickCursor_);
    settings.setValue ("inertialScrolling", inertialScrolling_);
    settings.setValue ("autoSave", autoSave_);
    settings.setValue ("scrollJumpWorkaround", scrollJumpWorkaround_);
    settings.setValue ("skipNonText", skipNonText_);
    settings.setValue ("saveUnmodified", saveUnmodified_);
    settings.setValue ("maxSHSize", maxSHSize_);

    settings.setValue ("lightBgColorValue", lightBgColorValue_);
    settings.setValue ("dateFormat", dateFormat_);
    settings.setValue ("darkBgColorValue", darkBgColorValue_);
    settings.setValue ("executeScripts", executeScripts_);
    settings.setValue ("appendEmptyLine", appendEmptyLine_);
    settings.setValue ("removeTrailingSpaces", removeTrailingSpaces_);

    settings.setValue ("vLineDistance", vLineDistance_);

    settings.setValue ("recentFilesNumber", recentFilesNumber_);
    settings.setValue ("executeCommand", executeCommand_);
    while (recentFiles_.count() > recentFilesNumber_) // recentFilesNumber_ may have decreased
        recentFiles_.removeLast();
    if (recentFiles_.isEmpty()) // don't save "@Invalid()"
        settings.remove ("recentFiles");
    else
        settings.setValue ("recentFiles", recentFiles_);
    settings.setValue ("recentOpened", recentOpened_);

    settings.setValue ("saveLastFilesList", saveLastFilesList_);

    settings.setValue ("autoSaveInterval", autoSaveInterval_);

    settings.setValue ("textTabSize", textTabSize_);

    settings.endGroup();

    /*****************
     *** Shortcuts ***
     *****************/

    settings.beginGroup ("shortcuts");

    for (int i = 0; i < removedActions_.size(); ++i)
        settings.remove (removedActions_.at (i));

    QHash<QString, QString>::const_iterator it = actions_.constBegin();
    while (it != actions_.constEnd())
    {
        settings.setValue (it.key(), it.value());
        ++it;
    }

    settings.endGroup();

    writeCursorPos();
}
/*************************/
void Config::readCursorPos()
{
    if (!cursorPosRetrieved_)
    {
        Settings settings ("featherpad", "fp_cursor_pos");
        cursorPos_ = settings.value ("cursorPositions").toHash();
        cursorPosRetrieved_ = true;
    }
}
/*************************/
void Config::writeCursorPos()
{
    Settings settings ("featherpad", "fp_cursor_pos");
    if (settings.isWritable())
    {
        if (!cursorPos_.isEmpty())
        {
            /* no need to clean up the config file here because
           it's more or less cleaned by the session dialog */
            /*QHash<QString, QVariant>::iterator it = cursorPos_.begin();
            while (it != cursorPos_.end())
            {
                if (!QFileInfo (it.key()).isFile())
                    it = cursorPos_.erase (it);
                else ++it;
            }*/
            settings.setValue ("cursorPositions", cursorPos_);
        }
    }

    Settings settingsLastCur ("featherpad", "fp_last_cursor_pos");
    if (settingsLastCur.isWritable())
    {
        if (saveLastFilesList_ && !lasFilesCursorPos_.isEmpty())
            settingsLastCur.setValue ("cursorPositions", lasFilesCursorPos_);
        else
            settingsLastCur.remove ("cursorPositions");
    }
}
/*************************/
void Config::addRecentFile (const QString& file)
{
    recentFiles_.removeAll (file);
    recentFiles_.prepend (file);
    while (recentFiles_.count() > curRecentFilesNumber_)
        recentFiles_.removeLast();
}
/*************************/
QString Config::validatedShortcut (const QVariant v)
{
    static QStringList added;
    if (v.isValid())
    {
        QString str = v.toString();

        /* NOTE: In older versions of FeatherPad, shorcuts were saved in
           the NativeText format. For the sake of backward compatibility,
           we convert them into the PortableText format. */
        QKeySequence keySeq (str);
        if (str == keySeq.toString (QKeySequence::NativeText))
            str = keySeq.toString();

        if (!QKeySequence (str, QKeySequence::PortableText).toString().isEmpty())
        {
            if (!reservedShortcuts_.contains (str)
                // prevent ambiguous shortcuts at startup as far as possible
                && !added.contains (str))
            {
                added << str;
                return str;
            }
        }
    }
    return QString();
}

}
