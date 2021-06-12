/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2021 <tsujan2000@gmail.com>
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
#include "singleton.h"

#ifdef HAS_X11
#include "x11.h"
#endif

#include <signal.h>
#include <QLibraryInfo>
#include <QTranslator>

void handleQuitSignals (const std::vector<int>& quitSignals)
{
    auto handler = [](int sig) ->void {
        Q_UNUSED (sig)
        //printf("\nUser signal = %d.\n", sig);
        QCoreApplication::quit();
    };

    for (int sig : quitSignals)
        signal (sig, handler); // handle these signals by quitting gracefully
}

int main (int argc, char **argv)
{
    const QString name = "FeatherPad";
    const QString version = "0.19.0";
    const QString option = QString::fromUtf8 (argv[1]);
    if (option == "--help" || option == "-h")
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
#if (QT_VERSION >= QT_VERSION_CHECK(5,15,0))
            << Qt::endl;
#else
            << endl;
#endif
        return 0;
    }
    else if (option == "--version" || option == "-v")
    {
        QTextStream out (stdout);
        out << name << " " << version
#if (QT_VERSION >= QT_VERSION_CHECK(5,15,0))
            << Qt::endl;
#else
            << endl;
#endif
        return 0;
    }

    FeatherPad::FPsingleton singleton (argc, argv, option == "--standalone" || option == "-s");
    singleton.setApplicationName (name);
    singleton.setApplicationVersion (version);

    handleQuitSignals ({SIGQUIT, SIGINT, SIGTERM, SIGHUP}); // -> https://en.wikipedia.org/wiki/Unix_signal

    singleton.setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QStringList langs (QLocale::system().uiLanguages());
    QString lang; // bcp47Name() doesn't work under vbox
    if (!langs.isEmpty())
        lang = langs.first().replace ('-', '_');

    QTranslator qtTranslator;
    if (!qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath)))
    { // shouldn't be needed
        if (!langs.isEmpty())
        {
            lang = langs.first().split (QLatin1Char ('_')).first();
            qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath));
        }
    }
    singleton.installTranslator (&qtTranslator);

    QTranslator FPTranslator;
#if defined(Q_OS_HAIKU)
    FPTranslator.load ("featherpad_" + lang, QStringLiteral (DATADIR) + "/../translations");
#elif defined(Q_OS_MAC)
    FPTranslator.load ("featherpad_" + lang, singleton.applicationDirPath() + QStringLiteral ("/../Resources/translations/"));
#else
    FPTranslator.load ("featherpad_" + lang, QStringLiteral (DATADIR) + "/featherpad/translations");
#endif
    singleton.installTranslator (&FPTranslator);

    QString info;
#ifdef HAS_X11
    int d = singleton.isX11() ? static_cast<int>(FeatherPad::fromDesktop()) : -1;
#else
    int d = -1;
#endif
    info.setNum (d);
    info += "\n\r"; // a string that can't be used in file names
    info += QDir::currentPath();
    info += "\n\r";
    for (int i = 1; i < argc; ++i)
    {
        info += QString::fromUtf8 (argv[i]);
        if (i < argc - 1)
            info += "\n\r";
    }

    // the slot is executed in the receiver's thread
    if (singleton.sendMessage (info))
        return 0;

    QObject::connect (&singleton, &QCoreApplication::aboutToQuit, &singleton, &FeatherPad::FPsingleton::quitting);
    singleton.firstWin (info);
    QObject::connect (&singleton, &FeatherPad::FPsingleton::messageReceived,
                      &singleton, &FeatherPad::FPsingleton::handleMessage);

    return singleton.exec();
}
