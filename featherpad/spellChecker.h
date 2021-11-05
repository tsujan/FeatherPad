/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2019-2021 <tsujan2000@gmail.com>
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

#ifndef SPELLCHECKER_H
#define SPELLCHECKER_H

#include <QString>
#include <QHash>
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
#include <QTextCodec>
#else
#include <QStringEncoder>
#endif

class Hunspell;

namespace FeatherPad {

class SpellChecker {
public:
    SpellChecker (const QString& dictionaryPath, const QString& userDictionary);
    ~SpellChecker();

    bool spell (const QString& word);
    QStringList suggest (const QString& word);
    void ignoreWord (const QString& word);
    void addToUserWordlist (const QString& word);

    void addToCorrections (const QString& misspelled, const QString& correct) {
        corrections_.insert (misspelled, correct);
    }
    QString correct (const QString& misspelled) const {
        return corrections_.value (misspelled);
    }

private:
    Hunspell *hunspell_;
    QString userDictionary_;
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    QTextCodec *codec_;
#else
    QStringEncoder encoder_;
#endif
    QHash<QString, QString> corrections_;
};

}

#endif // SPELLCHECKER_H
