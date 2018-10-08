/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014 <tsujan2000@gmail.com>
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

#include <QDesktopWidget>
#include <QLocalSocket>
#include <QDialog>
#include <QStandardPaths>
#include <QCryptographicHash>

#ifdef HAS_X11
#if defined Q_WS_X11 || defined Q_OS_LINUX || defined Q_OS_FREEBSD
#include <QX11Info>
#endif
#endif

#include "singleton.h"

#ifdef HAS_X11
#include "x11.h"
#endif

namespace FeatherPad {

FPsingleton::FPsingleton (int &argc, char **argv) : QApplication (argc, argv)
{
#ifdef HAS_X11
    // For now, the lack of x11 is seen as wayland.
#if defined Q_WS_X11 || defined Q_OS_LINUX || defined Q_OS_FREEBSD
#if QT_VERSION < 0x050200
    isX11_ = true;
#else
    isX11_ = QX11Info::isPlatformX11();
#endif
#else
    isX11_ = false;
#endif
#else
    isX11_ = false;
#endif // HAS_X11

    socketFailure_ = false;
    config_.readConfig();
    lastFiles_ = config_.getLastFiles();
    if (config_.getIconless())
        setAttribute (Qt::AA_DontShowIconsInMenus);

    /* Instead of QSharedMemory, a lock file is used for creating a single instance because
       QSharedMemory not only would be unsafe with a crash but also might persist without a
       crash and even after being attached and detached, resulting in an unchangeable state. */
    QByteArray user = qgetenv ("USER");
    uniqueKey_ = "featherpad-" + QString::fromLocal8Bit (user) + QLatin1Char ('-')
                 + QCryptographicHash::hash (user, QCryptographicHash::Sha1).toHex();
    QString lockFilePath = QStandardPaths::writableLocation (QStandardPaths::TempLocation)
                           + "/" + uniqueKey_ + ".lock";
    lockFile_ = new QLockFile (lockFilePath);

    if (lockFile_->tryLock())
    { // create a local server and listen to incoming messages from other instances
        localServer = new QLocalServer (this);
        connect (localServer, &QLocalServer::newConnection, this, &FPsingleton::receiveMessage);
        if (!localServer->listen (uniqueKey_))
        {
            if (localServer->removeServer (uniqueKey_))
                localServer->listen (uniqueKey_);
            else
                qDebug ("Unable to remove server instance (from a previous crash).");
        }
    }
    else
    {
        delete lockFile_; lockFile_ = nullptr;
        localServer = nullptr;
    }
}
/*************************/
FPsingleton::~FPsingleton()
{
    if (lockFile_)
    {
        lockFile_->unlock();
        delete lockFile_;
    }
}
/*************************/
void FPsingleton::quitting()
{
    config_.writeConfig();
}
/*************************/
void FPsingleton::receiveMessage()
{
    QLocalSocket *localSocket = localServer->nextPendingConnection();
    if (!localSocket || !localSocket->waitForReadyRead (timeout))
    {
        qDebug ("%s", (const char *) localSocket->errorString().toLatin1());
        return;
    }
    QByteArray byteArray = localSocket->readAll();
    QString message = QString::fromUtf8 (byteArray.constData());
    emit messageReceived (message);
    localSocket->disconnectFromServer();
}
/*************************/
// A new instance will be started only if this function returns false.
bool FPsingleton::sendMessage (const QString &message)
{
    if (localServer != nullptr) // no other instance was running
        return false;
    QLocalSocket localSocket (this);
    localSocket.connectToServer (uniqueKey_, QIODevice::WriteOnly);
    if (!localSocket.waitForConnected (timeout))
    {
        socketFailure_ = true;
        qDebug ("%s", (const char *) localSocket.errorString().toLatin1());
        return false;
    }
    localSocket.write (message.toUtf8());
    if (!localSocket.waitForBytesWritten (timeout))
    {
        socketFailure_ = true;
        qDebug ("%s", (const char *) localSocket.errorString().toLatin1());
        return false;
    }
    localSocket.disconnectFromServer();
    return true;
}
/*************************/
bool FPsingleton::cursorInfo (const QString& commndOpt, int& lineNum, int& posInLine)
{
    if (commndOpt.isEmpty()) return false;
    lineNum = 0; // no cursor placing
    posInLine = 0;
    if (commndOpt == "+")
    {
        lineNum = -2; // means the end (-> FPwin::newTabFromName)
        posInLine = 0; // useless
        return true;
    }
    else if (commndOpt.startsWith ("+"))
    {
        bool ok;
        lineNum = commndOpt.toInt (&ok); // "+" is included
        if (ok)
        {
            if (lineNum > 0) // otherwise, the cursor will be ignored (-> FPwin::newTabFromName)
                lineNum += 1; // 1 is reserved for session files (-> FPwin::newTabFromName)
            return true;
        }
        else
        {
            QStringList l = commndOpt.split (",");
            if (l.count() == 2)
            {
                lineNum = l.at (0).toInt (&ok);
                if (ok)
                {
                    posInLine = l.at (1).toInt (&ok);
                    if (ok)
                    {
                        if (lineNum > 0)
                            lineNum += 1;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}
/*************************/
QStringList FPsingleton::processInfo (const QString& message,
                                      long &desktop, int& lineNum, int& posInLine,
                                      bool *newWin)
{
    desktop = -1;
    lineNum = 0; // no cursor placing
    posInLine = 0;
    QStringList sl = message.split ("\n\r"); // "\n\r" was used as the splitter
    if (sl.count() < 2) // impossible because "\n\r" is appended to desktop number
    {
        *newWin = true;
        return QStringList();
    }
    *newWin = false;
    desktop = sl.at (0).toInt();
    sl.removeFirst();
    bool hasCurInfo = cursorInfo (sl.at (0), lineNum, posInLine);
    if (hasCurInfo)
    {
        sl.removeFirst();
        if (!sl.isEmpty())
        { // check if the second option is --win/-w
            if (sl.at (0) == "--win" || sl.at (0) == "-w")
            {
                *newWin = true;
                sl.removeFirst();
            }
        }
    }
    // check if the first option is --win/-w
    else if (sl.at (0) == "--win" || sl.at (0) == "-w")
    {
        *newWin = true;
        sl.removeFirst();
        if (!sl.isEmpty())
            hasCurInfo = cursorInfo (sl.at (0), lineNum, posInLine);
        if (hasCurInfo)
            sl.removeFirst();
    }

    if (sl.isEmpty())
    {
        if (hasCurInfo)
            qDebug ("FeatherPad: File path/name is missing.");
    }
    else if (sl.count() == 1 && sl.at (0).isEmpty())
        sl.clear(); // no empty path/name
    return sl;
}
/*************************/
void FPsingleton::firstWin (const QString& message)
{
    int lineNum = 0, posInLine = 0;
    long d = -1;
    bool openNewWin;
    QStringList filesList = processInfo (message, d, lineNum, posInLine, &openNewWin);
    newWin (filesList, lineNum, posInLine);
    lastFiles_ = QStringList(); // they should be called only with the session start
}
/*************************/
FPwin* FPsingleton::newWin (const QStringList& filesList,
                            int lineNum, int posInLine)
{
    FPwin *fp = new FPwin;
    fp->show();
    if (socketFailure_)
        fp->showCrashWarning();
    Wins.append (fp);

    if (!filesList.isEmpty())
    {
        bool multiple (filesList.count() > 1 || fp->isLoading());
        for (int i = 0; i < filesList.count(); ++i) // open all files in new tabs
            fp->newTabFromName (filesList.at (i), lineNum, posInLine, multiple);
    }
    else if (!lastFiles_.isEmpty())
    {
        bool multiple (lastFiles_.count() > 1 || fp->isLoading());
        for (int i = 0; i < lastFiles_.count(); ++i)
            fp->newTabFromName (lastFiles_.at (i), -1, 0, multiple); // restore cursor positions too
    }

    return fp;
}
/*************************/
void FPsingleton::removeWin (FPwin *win)
{
    Wins.removeOne (win);
    win->deleteLater();
}
/*************************/
void FPsingleton::handleMessage (const QString& message)
{
    int lineNum = 0, posInLine = 0;
    long d = -1;
    bool openNewWin;
    QStringList filesList = processInfo (message, d, lineNum, posInLine, &openNewWin);
    if (openNewWin)
    {
        newWin (filesList, lineNum, posInLine);
        return;
    }
    bool found = false;
    if (!config_.getOpenInWindows())
    {
        const QRect sr = QApplication::desktop()->screenGeometry();
        for (int i = 0; i < Wins.count(); ++i)
        {
#ifdef HAS_X11
            WId id = Wins.at (i)->winId();
#endif
            /* if the command is issued from where a FeatherPad
               window exists and if that window isn't minimized
               and doesn't have a modal dialog... */
            if (!isX11_ // always open a new tab on wayland
#ifdef HAS_X11
                || (onWhichDesktop (id) == d
                    && (!Wins.at (i)->isMinimized() || isWindowShaded (id)))
#endif
               )
            {
                bool hasDialog = false;
                QList<QDialog*> dialogs = Wins.at (i)->findChildren<QDialog*>();
                for (int j = 0; j < dialogs.count(); ++j)
                {
                    if (dialogs.at (j)->objectName() !=  "processDialog"
                        && dialogs.at (j)->objectName() !=  "sessionDialog")
                    {
                        hasDialog = true;
                        break;
                    }
                }
                if (hasDialog) continue;
                /* consider viewports too, so that if more than half of the width as well as the height
                   of the window is inside the current viewport (of the current desktop), open a new tab */
                QRect g = Wins.at (i)->geometry();
                if (g.x() + g.width()/2 >= 0 && g.x() + g.width()/2 < sr.width()
                    && g.y() + g.height()/2 >= 0 && g.y() + g.height()/2 < sr.height())
                {
                    if (d >= 0) // it may be -1 for some DEs that don't support _NET_CURRENT_DESKTOP
                    {
                        /* first, pretend to KDE that a new window is created
                           (without this, the next new window would open on a wrong desktop) */
                        Wins.at (i)->dummyWidget->showMinimized();
                        QTimer::singleShot (0, Wins.at (i)->dummyWidget, SLOT (close()));
                    }

                    /* and then, open tab(s) in the current FeatherPad window... */
                    if (filesList.isEmpty())
                        Wins.at (i)->newTab();
                    else
                    {
                        bool multiple (filesList.count() > 1 || Wins.at (i)->isLoading());
                        for (int j = 0; j < filesList.count(); ++j)
                            Wins.at (i)->newTabFromName (filesList.at (j), lineNum, posInLine, multiple);
                    }
                    found = true;
                    break;
                }
            }
        }
    }
    if (!found)
        /* ... otherwise, open a new window */
        newWin (filesList, lineNum, posInLine);
}


}
