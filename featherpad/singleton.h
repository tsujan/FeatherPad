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

#ifndef SINGLETON_H
#define SINGLETON_H

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <QApplication>
#include "fpwin.h"
#include "config.h"

namespace FeatherPad {

#ifdef Q_OS_WIN
class FPsingletonHiddenWindow : public QWidget
{
    Q_OBJECT
public:
    FPsingletonHiddenWindow ();
    bool isPrimaryInstance() const {
        return isPrimaryInstance_;
    }
	void sendInfo(QStringList info);
	bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) Q_DECL_OVERRIDE;

signals:
    void dataReceived(QStringList data);

private:
	HWND hwndPrimary_;
    bool isPrimaryInstance_;
};
#endif

class FPsingleton : public QApplication
{
    Q_OBJECT
public:
    FPsingleton (int &argc, char **argv);
    ~FPsingleton();

    void init (bool standalone);

    void sendInfo (const QStringList &info);
    void handleInfo (const QStringList &info);

    void sendRecentFile (const QString &file, bool recentOpened);
    void addRecentFile (const QString &file, bool recentOpened);

    void firstWin (const QStringList &info);
    FPwin* newWin (const QStringList &filesList = QStringList(),
                   int lineNum = 0, int posInLine = 0);
    void removeWin (FPwin *win);

    QList<FPwin*> Wins; // All FeatherPad windows.

    Config& getConfig() {
        return config_;
    }

    bool isPrimaryInstance() const {
        return isPrimaryInstance_;
    }
    bool isStandAlone() const {
        return standalone_;
    }
    bool isX11() const {
        return isX11_;
    }
    bool isWayland() const {
        return isWayland_;
    }
    bool isRoot() const {
        return isRoot_;
    }
    bool isQuitSignalReceived() const {
        return quitSignalReceived_;
    }
    QStandardItemModel *searchModel() const {
        return searchModel_;
    }

public slots:
    void quitSignalReceived();
    void quitting();
#ifdef Q_OS_WIN
    void dataReceived(QStringList data);
#endif

private:
    bool cursorInfo (const QString &commndOpt, int &lineNum, int &posInLine);
    QStringList processInfo (const QStringList &info,
                             long &desktop, int &lineNum, int &posInLine,
                             bool *newWindow);
    bool quitSignalReceived_;
    Config config_;
    QStringList lastFiles_;
    bool isPrimaryInstance_;
    bool standalone_; // Whether this is a standalone instance.
    bool isX11_;
    bool isWayland_;
    bool isRoot_;
    QStandardItemModel *searchModel_; // The common search history if any.
#ifdef Q_OS_WIN
    FPsingletonHiddenWindow *hiddenWindow_;
#endif
};

}

#endif // SINGLETON_H
