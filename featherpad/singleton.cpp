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

#include <QDir>
#include <QScreen>
#include <QLocalSocket>
#include <QDialog>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QThread>

#if defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
#include <unistd.h> // for geteuid()
#endif

#include "singleton.h"

#ifdef HAS_X11
#include "x11.h"
#endif

namespace FeatherPad {

FPsingleton::FPsingleton (int &argc, char **argv, bool standalone) : QApplication (argc, argv)
{
#ifdef HAS_X11
    isX11_ = (QString::compare (QGuiApplication::platformName(), "xcb", Qt::CaseInsensitive) == 0);
#else
    isX11_ = false;
#endif

    if (isX11_)
        isWayland_ = false;
    else
        isWayland_ = (QString::compare (QGuiApplication::platformName(), "wayland", Qt::CaseInsensitive) == 0);

    standalone_ = standalone;
    socketFailure_ = false;
    config_.readConfig();
    lastFiles_ = config_.getLastFiles();
    if (config_.getSharedSearchHistory())
        searchModel_ = new QStandardItemModel (0, 1, this);
    else
        searchModel_ = nullptr;

    if (standalone)
    {
        lockFile_ = nullptr;
        localServer_ = nullptr;
        return;
    }

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
        localServer_ = new QLocalServer (this);
        connect (localServer_, &QLocalServer::newConnection, this, &FPsingleton::receiveMessage);
        if (!localServer_->listen (uniqueKey_))
        {
            if (localServer_->removeServer (uniqueKey_))
                localServer_->listen (uniqueKey_);
            else
                qDebug ("Unable to remove server instance (from a previous crash).");
        }
    }
    else
    {
        delete lockFile_; lockFile_ = nullptr;
        localServer_ = nullptr;
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
    /* save some important info if windows aren't closed
       (e.g., when the app is terminated by SIGTERM) */
    for (int i = 0; i < Wins.size(); ++i)
        Wins.at (i)->saveInfoOnTerminating (config_, i == Wins.size() - 1);

    if (searchModel_)
        delete searchModel_;
    config_.writeConfig();
}
/*************************/
void FPsingleton::receiveMessage()
{
    QLocalSocket *localSocket = localServer_->nextPendingConnection();
    if (!localSocket)
    {
        qDebug ("Unable to find local socket.");
        return;
    }
    if (!localSocket->waitForReadyRead (timeout_))
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
bool FPsingleton::sendMessage (const QString& message)
{
    if (standalone_ // it's standalone or...
        || localServer_ != nullptr) // ... no other instance was running
    {
        return false;
    }

    QLocalSocket localSocket (this);
    localSocket.connectToServer (uniqueKey_, QIODevice::WriteOnly);
    /* NOTE: If "QStandardPaths::TempLocation" isn't on RAM, the socket may not be
             ready yet. So, we retry a few times to make sure this isn't about a crash. */
    int waiting = 0;
    while (waiting < 5 && !localSocket.waitForConnected (timeout_))
    {
        QThread::msleep(500);
        localSocket.connectToServer (uniqueKey_, QIODevice::WriteOnly);
        ++ waiting;
    }
    if (waiting == 5 && !localSocket.waitForConnected (timeout_))
    {
        socketFailure_ = true;
        qDebug ("%s", (const char *) localSocket.errorString().toLatin1());
        return false;
    }

    localSocket.write (message.toUtf8());
    if (!localSocket.waitForBytesWritten (timeout_))
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
                                      bool *newWindow)
{
    desktop = -1;
    lineNum = 0; // no cursor placing
    posInLine = 0;
    QStringList sl = message.split ("\n\r"); // "\n\r" was used as the splitter
    if (sl.count() < 3) // impossible because "\n\r" is appended to desktop number plus current directory
    {
        *newWindow = true;
        return QStringList();
    }
    desktop = sl.at (0).toInt();
    sl.removeFirst();
    QDir curDir (sl.at (0));
    sl.removeFirst();
    if (standalone_)
    {
        *newWindow = true;
        sl.removeFirst(); // "--standalone" is always the first optionn
        if (sl.isEmpty())
            return QStringList();
    }
    else
        *newWindow = false;
    bool hasCurInfo = cursorInfo (sl.at (0), lineNum, posInLine);
    if (hasCurInfo)
    {
        sl.removeFirst();
        if (!sl.isEmpty())
        { // check if the second option is --win/-w
            if (sl.at (0) == "--win" || sl.at (0) == "-w")
            {
                *newWindow = true;
                sl.removeFirst();
            }
        }
    }
    // check if the first option is --win/-w
    else if (sl.at (0) == "--win" || sl.at (0) == "-w")
    {
        *newWindow = true;
        sl.removeFirst();
        if (!sl.isEmpty())
            hasCurInfo = cursorInfo (sl.at (0), lineNum, posInLine);
        if (hasCurInfo)
            sl.removeFirst();
    }

    /* always return absolute clean paths (works around KDE's double slash bug too) */
    QStringList filesList;
    for (const auto &path : qAsConst (sl))
    {
        if (!path.isEmpty()) // no empty path/name
        {
            QString realPath = path;
#ifdef __OS2__
						// We try to match, whether the filepath begins with a driverletter + ':'
						// QUrl mistakes that for a scheme.
						QRegularExpression re("^[A-Za-z]:");
						if (path.indexOf(re, 0, nullptr) != -1)
						{
							realPath.prepend("file:");
						}
#endif
            QString scheme = QUrl (realPath).scheme();
            if (scheme == "file")
                realPath = QUrl (realPath).toLocalFile();
            else if (scheme == "admin") // gvfs' "admin:///"
                realPath = QUrl (realPath).path();
            else if (!scheme.isEmpty())
                continue;
            realPath = curDir.absoluteFilePath (realPath); // also works with absolute paths outside curDir
            filesList << QDir::cleanPath (realPath);
        }
    }
    if (filesList.isEmpty() && hasCurInfo)
        qDebug ("FeatherPad: File path/name is missing.");
    return filesList;
}
/*************************/
void FPsingleton::firstWin (const QString& message)
{
    int lineNum = 0, posInLine = 0;
    long d = -1;
    bool openNewWin;
    const QStringList filesList = processInfo (message, d, lineNum, posInLine, &openNewWin);
    if (config_.getOpenInWindows() && !filesList.isEmpty())
    {
        for (const auto &file : filesList)
          newWin (QStringList() << file, lineNum, posInLine);
    }
    else
        newWin (filesList, lineNum, posInLine);
    lastFiles_ = QStringList(); // they should be called only with the session start
}
/*************************/
FPwin* FPsingleton::newWin (const QStringList& filesList,
                            int lineNum, int posInLine)
{
    FPwin *fp = new FPwin (nullptr, standalone_);
    fp->show();
    if (socketFailure_)
        fp->showCrashWarning();
#if defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
    else if (geteuid() == 0)
        fp->showRootWarning();
#endif
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
    const QStringList filesList = processInfo (message, d, lineNum, posInLine, &openNewWin);
    if (openNewWin && !config_.getOpenInWindows())
    {
        newWin (filesList, lineNum, posInLine);
        return;
    }
    bool found = false;
    if (!config_.getOpenInWindows())
    {
        QRect sr;
        if (QScreen *pScreen = QApplication::primaryScreen())
            sr = pScreen->virtualGeometry();
        for (int i = 0; i < Wins.count(); ++i)
        {
            FPwin *thisWin = Wins.at (i);
#ifdef HAS_X11
            WId id = thisWin->winId();
            long whichDesktop = -1;
            if (isX11_)
                whichDesktop = onWhichDesktop (id);
#endif
            /* if the command is issued from where a FeatherPad
               window exists and if that window isn't minimized
               and doesn't have a modal dialog... */
            if (!isX11_ // always open a new tab if we aren't on x11
#ifdef HAS_X11
                || ((whichDesktop == d
                     /* if a window is created a moment ago, it should be
                        on the current desktop but may not report that yet */
                     || whichDesktop == -1)
                    && (!thisWin->isMinimized() || isWindowShaded (id)))
#endif
               )
            {
                bool hasDialog = thisWin->isLocked();
                if (!hasDialog)
                {
                    QList<QDialog*> dialogs = thisWin->findChildren<QDialog*>();
                    for (int j = 0; j < dialogs.count(); ++j)
                    {
                        if (dialogs.at (j)->isModal())
                        {
                            hasDialog = true;
                            break;
                        }
                    }
                }
                if (hasDialog) continue;
                /* consider viewports too, so that if more than half of the width as well as the height
                   of the window is inside the current viewport (of the current desktop), open a new tab */
                if (sr.contains (thisWin->geometry().center()))
                {
                    if (d >= 0) // it may be -1 for some DEs that don't support _NET_CURRENT_DESKTOP
                    {
                        /* first, pretend to KDE that a new window is created
                           (without this, the next new window would open on a wrong desktop) */
                        thisWin->dummyWidget->showMinimized();
                        QTimer::singleShot (0, thisWin->dummyWidget, &QWidget::close);
                    }

                    /* and then, open tab(s) in the current FeatherPad window... */
                    if (filesList.isEmpty())
                        thisWin->newTab();
                    else
                    {
                        bool multiple (filesList.count() > 1 || thisWin->isLoading());
                        for (int j = 0; j < filesList.count(); ++j)
                            thisWin->newTabFromName (filesList.at (j), lineNum, posInLine, multiple);
                    }
                    found = true;
                    break;
                }
            }
        }
    }
    if (!found)
    {
        /* ... otherwise, open a new window */
        if (config_.getOpenInWindows() && !filesList.isEmpty())
        {
            for (const auto &file : filesList)
              newWin (QStringList() << file, lineNum, posInLine);
        }
        else
            newWin (filesList, lineNum, posInLine);
    }
}


}
