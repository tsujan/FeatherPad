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
#include <QDialog>

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#endif

#if defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
#include <unistd.h> // for geteuid()
#endif

#include "singleton.h"
#include "featherpadadaptor.h"

#ifdef HAS_X11
#include "x11.h"
#endif

namespace FeatherPad {

#ifdef Q_OS_WIN

static const char *hiddenWindowTitle = "FeatherPadHiddenWindow";

FPsingletonHiddenWindow::FPsingletonHiddenWindow ()
{
    hwndPrimary_ = FindWindowA(NULL, hiddenWindowTitle);
    isPrimaryInstance_ = !hwndPrimary_;
    if (isPrimaryInstance_)
	{
		QWidget();
        setWindowTitle(hiddenWindowTitle);
        setWindowFlags(Qt::Tool);
        showMinimized();
        hide();
    }
}

bool FPsingletonHiddenWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
	Q_UNUSED(result);
	Q_UNUSED(eventType);
	MSG *msg = static_cast< MSG * >( message );
    if (msg->message == WM_COPYDATA)
    {
        COPYDATASTRUCT *ds = (COPYDATASTRUCT*)msg->lParam;
        emit dataReceived(QString::fromUtf8((const char *)ds->lpData).split('\n'));
    }
	return false;
}

void FPsingletonHiddenWindow::sendInfo(QStringList info)
{
	const char *data = info.join('\n').toUtf8();
    COPYDATASTRUCT ds;
    ds.dwData = 0;
    ds.cbData = sizeof(char) * (strlen(data) + 1); //sizeof(data);
    ds.lpData = (LPVOID)data;
    SendMessageW(hwndPrimary_, WM_COPYDATA, (WPARAM)hwndPrimary_, (LPARAM)(LPVOID)&ds);
}

#else
static const char *serviceName = "org.featherpad.FeatherPad";
static const char *ifaceName = "org.featherpad.Application";
#endif


FPsingleton::FPsingleton (int &argc, char **argv) : QApplication (argc, argv)
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

    isPrimaryInstance_ = true;
    standalone_ = false;
    quitSignalReceived_ = false;
    isRoot_ = false;
    config_.readConfig();
    lastFiles_ = config_.getLastFiles();
    if (config_.getSharedSearchHistory())
        searchModel_ = new QStandardItemModel (0, 1, this);
    else
        searchModel_ = nullptr;
}
/*************************/
FPsingleton::~FPsingleton()
{
    qDeleteAll (Wins);
}
/*************************/
void FPsingleton::init (bool standalone)
{
    standalone_ = standalone;
    isPrimaryInstance_ = standalone;
    if (!standalone_)
    {
#ifndef Q_OS_WIN
        QDBusConnection dbus = QDBusConnection::sessionBus();
        if (!dbus.isConnected()) // interpret it as the lack of D-Bus
        {
            isPrimaryInstance_ = true;
            standalone_ = true;
        }
        else if (dbus.registerService (QLatin1String (serviceName)))
        {
            isPrimaryInstance_ = true;
            new FeatherPadAdaptor (this);
            dbus.registerObject (QStringLiteral ("/Application"), this);
        }
#else
		hiddenWindow_ = new FPsingletonHiddenWindow ();
        isPrimaryInstance_ = hiddenWindow_->isPrimaryInstance();
        if (isPrimaryInstance_)
        	connect(hiddenWindow_, &FPsingletonHiddenWindow::dataReceived, this, &FPsingleton::dataReceived);
#endif
    }
}
/*************************/
void FPsingleton::quitting()
{
    /* save some important info if windows aren't closed
       (e.g., when the app is terminated by SIGTERM) */
    for (int i = 0; i < Wins.size(); ++i)
        Wins.at (i)->cleanUpOnTerminating (config_, i == Wins.size() - 1);

    if (searchModel_)
        delete searchModel_;
    config_.writeConfig();
}
/*************************/
void FPsingleton::quitSignalReceived()
{
    quitSignalReceived_ = true;
    quit();
}
/*************************/
void FPsingleton::sendInfo (const QStringList &info)
{
#ifndef Q_OS_WIN
    QDBusConnection dbus = QDBusConnection::sessionBus();
    QDBusInterface iface (QLatin1String (serviceName),
                          QStringLiteral ("/Application"),
                          QLatin1String (ifaceName), dbus, this);
    iface.call (QStringLiteral ("handleInfo"), info);
#else
	hiddenWindow_->sendInfo(QStringList(QStringLiteral("handleInfo")) + info);
#endif
}

#ifdef Q_OS_WIN
void FPsingleton::dataReceived(QStringList data)
{
	if (data.first() == QLatin1String ("handleInfo"))
	{
		data.removeFirst();
		handleInfo(data);
	}
	else if (data.first() == QLatin1String ("addRecentFile"))
	{
		addRecentFile(data[1], data[2] == QLatin1String ("true"));
	}
}
#endif

/*************************/
// Called only in standalone mode.
void FPsingleton::sendRecentFile (const QString &file, bool recentOpened)
{
#ifndef Q_OS_WIN
    QDBusMessage methodCall =
    QDBusMessage::createMethodCall (QLatin1String (serviceName),
                                    QStringLiteral ("/Application"),
                                    QString(),
                                    QStringLiteral ("addRecentFile"));
    methodCall.setAutoStartService (false);
    QList<QVariant> args;
    args.append (file);
    args.append (recentOpened);
    methodCall.setArguments (args);
    QDBusConnection::sessionBus().call (methodCall, QDBus::NoBlock, 1000);
#else
	QStringList args;
	args.append (QStringLiteral ("addRecentFile"));
    args.append (file);
    args.append (recentOpened ? QStringLiteral ("true") : QStringLiteral("false"));
    hiddenWindow_->sendInfo( args );
#endif
}
/*************************/
bool FPsingleton::cursorInfo (const QString &commndOpt, int &lineNum, int &posInLine)
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
QStringList FPsingleton::processInfo (const QStringList &info,
                                      long &desktop, int &lineNum, int &posInLine,
                                      bool *newWindow)
{
    desktop = -1;
    lineNum = 0; // no cursor placing
    posInLine = 0;
    QStringList sl = info;
    if (sl.isEmpty()) // normally impossible, because of desktop number and current directory
    {
        *newWindow = true;
        return QStringList();
    }
    *newWindow = standalone_; // standalone_ is true without D-Bus
    desktop = sl.at (0).toInt();
    sl.removeFirst();
    if (sl.isEmpty())
        return QStringList();
    QDir curDir (sl.at (0));
    sl.removeFirst();
    if (!sl.isEmpty() && (sl.at (0) == "--standalone" || sl.at (0) == "-s"))
        sl.removeFirst(); // "--standalone" is always the first option
    if (sl.isEmpty())
        return QStringList();
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
#if (QT_VERSION >= QT_VERSION_CHECK(6,6,0))
    for (const auto &path : std::as_const (sl))
#else
    for (const auto &path : qAsConst (sl))
#endif
    {
        if (!path.isEmpty()) // no empty path/name
        {
            QString realPath = path;
#if defined(__OS2__) || defined(Q_OS_WIN)
            /* Check whether the file path begins with a driverletter + ':'.
               QUrl mistakes that for a scheme. */
            if (path.indexOf (QRegularExpression ("^[A-Za-z]:")) != -1)
                realPath.prepend ("file:");
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
void FPsingleton::firstWin (const QStringList &info)
{
#if defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
    isRoot_ = (geteuid() == 0);
#endif
    int lineNum = 0, posInLine = 0;
    long d = -1;
    bool openNewWin;
    const QStringList filesList = processInfo (info, d, lineNum, posInLine, &openNewWin);
    /*if (config_.getOpenInWindows() && !filesList.isEmpty())
    {
        for (const auto &file : filesList)
          newWin (QStringList() << file, lineNum, posInLine);
    }
    else*/
        newWin (filesList, lineNum, posInLine);
    lastFiles_ = QStringList(); // they should be called only with the session start
}
/*************************/
FPwin* FPsingleton::newWin (const QStringList &filesList,
                            int lineNum, int posInLine)
{
    FPwin *fp = new FPwin (nullptr);
    fp->show();
    if (isRoot_)
        fp->showRootWarning();
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
// Called only by D-Bus.
void FPsingleton::handleInfo (const QStringList &info)
{
    int lineNum = 0, posInLine = 0;
    long d = -1;
    bool openNewWin;
    const QStringList filesList = processInfo (info, d, lineNum, posInLine, &openNewWin);
    if (openNewWin/* && !config_.getOpenInWindows()*/)
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
                    /*&& (!thisWin->isMinimized() || isWindowShaded (id))*/)
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
                if (!isX11_ || sr.contains (thisWin->geometry().center()))
                {
                    if (d >= 0) // it may be -1 for some DEs that don't support _NET_CURRENT_DESKTOP
                    {
                        /* first, because of an old bug, pretend to KDE that a new window is created
                           (without this, the next new window would open on a wrong desktop) */
                        thisWin->dummyWidget->showMinimized();
                        QTimer::singleShot (0, thisWin->dummyWidget, &QWidget::hide);
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
        /*if (config_.getOpenInWindows() && !filesList.isEmpty())
        {
            for (const auto &file : filesList)
                newWin (QStringList() << file, lineNum, posInLine);
        }
        else*/
            newWin (filesList, lineNum, posInLine);
    }
}
/*************************/
// Called only by D-Bus.
void FPsingleton::addRecentFile (const QString &file, bool recentOpened)
{
    if (config_.getRecentOpened() == recentOpened)
        config_.addRecentFile (file);
}

}
