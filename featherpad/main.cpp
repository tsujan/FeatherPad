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

#include <QDir>
#include <QTextStream>
#include "singleton.h"

#if !defined(Q_OS_WIN)
#include "signalDaemon.h"
#endif

#ifdef HAS_X11
#include "x11.h"
#endif

#include <QLibraryInfo>
#include <QTranslator>

int main (int argc, char **argv)
{
    const QString name = "FeatherPad";
    const QString version = "1.5.2";

    FeatherPad::FPsingleton singleton (argc, argv);
    singleton.setApplicationName (name);
    singleton.setApplicationVersion (version);

    QStringList args = singleton.arguments();
    if (!args.isEmpty())
        args.removeFirst();

    QString firstArg;
    if (!args.isEmpty())
        firstArg = args.at (0);

    if (firstArg == "--help" || firstArg == "-h")
    {
        QTextStream out (stdout);
        out << "FeatherPad - Lightweight Qt text editor\n"\
               "Usage:\n	featherpad [option(s)] [file1 file2 ...]\n"\
               "Or:\n	fpad [option(s)] [file1 file2 ...]\n\n"\
               "Options:\n\n"\
               "--help or -h        Show this help and exit.\n"\
               "--version or -v     Show version information and exit.\n"\
               "--standalone or -s  Start a standalone process of FeatherPad.\n"\
               "--win or -w         Open file(s) in a new window.\n"\
               "+                   Place cursor at document end.\n"\
               "+<L>                Place cursor at start of line L (L starts from 1).\n"\
               "+<L>,<P>            Place cursor at position P of line L (P starts from 0\n"\
               "                    but a negative value means line end).\n"\
               "\nNOTE1: <X> means number X without brackets.\n"\
               "NOTE2: --standalone or -s can only be the first option. If it exists,\n"\
               "       --win or -w will be ignored because a standalone process always\n"\
               "       has its separate, single window.\n"\
               "NOTE3: --win or -w can come before or after cursor option, with a space\n"\
               "       in between."
            << Qt::endl;
        return 0;
    }
    else if (firstArg == "--version" || firstArg == "-v")
    {
        QTextStream out (stdout);
        out << name << " " << version << Qt::endl;
        return 0;
    }

    singleton.init (firstArg == "--standalone" || firstArg == "-s");

    // with QLocale::system().name(), X and X_Y may be the same in tests
    QStringList langs (QLocale::system().uiLanguages());
    QString lang;
    if (!langs.isEmpty())
        lang = langs.first().replace ('-', '_');

    QTranslator qtTranslator;
    if (qtTranslator.load ("qt_" + lang, QLibraryInfo::path (QLibraryInfo::TranslationsPath)))
        singleton.installTranslator (&qtTranslator);
    else if (!langs.isEmpty())
    { // shouldn't be needed
        lang = langs.first().split (QLatin1Char ('_')).first();
        if (qtTranslator.load ("qt_" + lang, QLibraryInfo::path (QLibraryInfo::TranslationsPath)))
            singleton.installTranslator (&qtTranslator);
    }

    QTranslator FPTranslator;
#if defined(Q_OS_HAIKU)
    if (FPTranslator.load ("featherpad_" + lang, QStringLiteral(DATADIR) + "/../translations"))
#elif defined(Q_OS_MAC)
    if (FPTranslator.load ("featherpad_" + lang, singleton.applicationDirPath() + QStringLiteral("/../Resources/translations/")))
#elif defined(Q_OS_WIN)
    if (FPTranslator.load ("featherpad_" + lang, qApp->applicationDirPath() + "/data/translations"))
#else
    if (FPTranslator.load ("featherpad_" + lang, QStringLiteral(DATADIR) + "/featherpad/translations"))
#endif
    {
        singleton.installTranslator (&FPTranslator);
    }

    QStringList info;
#ifdef HAS_X11
    int d = singleton.isX11() ? static_cast<int>(FeatherPad::fromDesktop()) : -1;
#else
    int d = -1;
#endif
    info << QString::number (d) << QDir::currentPath();
    if (!args.isEmpty())
        info << args;

    if (!singleton.isPrimaryInstance())
    {
        singleton.sendInfo (info); // is sent to the primary instance
        return 0;
    }

#if !defined(Q_OS_WIN)
    // Handle SIGQUIT, SIGINT, SIGTERM and SIGHUP (-> https://en.wikipedia.org/wiki/Unix_signal).
    FeatherPad::signalDaemon D;
    D.watchUnixSignals();
    QObject::connect (&D, &FeatherPad::signalDaemon::sigQUIT,
                      &singleton, &FeatherPad::FPsingleton::quitSignalReceived);
#endif

    QObject::connect (&singleton, &QCoreApplication::aboutToQuit, &singleton, &FeatherPad::FPsingleton::quitting);
    singleton.firstWin (info);

    return singleton.exec();
}
