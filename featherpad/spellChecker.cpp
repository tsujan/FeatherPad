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

#include "spellChecker.h"

#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QRegularExpression>
#include <hunspell.hxx>


namespace FeatherPad {

SpellChecker::SpellChecker (const QString& dictionaryPath, const QString& userDictionary)
{
    userDictionary_ = userDictionary;

    QString dictFile = dictionaryPath + ".dic";
    QString affixFile = dictionaryPath + ".aff";
    QByteArray dictFilePathBA = dictFile.toLocal8Bit();
    QByteArray affixFilePathBA = affixFile.toLocal8Bit();
    hunspell_ = new Hunspell (affixFilePathBA.constData(), dictFilePathBA.constData());

    QString encoding = "UTF-8";
    QFile _affixFile (affixFile);
    if (_affixFile.open (QIODevice::ReadOnly))
    {
        QTextStream stream (&_affixFile);
        QRegularExpression encDetector ("^\\s*SET\\s+([A-Z0-9\\-]+)\\s*", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match;
        for (QString line = stream.readLine(); !line.isEmpty(); line = stream.readLine())
        {
            if (line.indexOf (encDetector, 0, &match) > -1)
            {
                encoding = match.captured (1);
                break;
            }
        }
        _affixFile.close();
    }
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    codec_ = QTextCodec::codecForName (encoding.toLatin1().constData());
#else
    encoder_ = QStringEncoder (encoding.compare ("UTF-8", Qt::CaseInsensitive) == 0 ?
                                   QStringConverter::Utf8
                               : encoding.compare ("UTF-16", Qt::CaseInsensitive) == 0 ?
                                   QStringConverter::Utf16
                               : encoding.compare ("UTF-32", Qt::CaseInsensitive) == 0 ?
                                   QStringConverter::Utf32
                               : QStringConverter::Latin1);
#endif

    if (!userDictionary_.isEmpty())
    {
        QFile userDictonaryFile (userDictionary_);
        if (userDictonaryFile.open (QIODevice::ReadOnly))
        {
            QTextStream stream (&userDictonaryFile);
            for (QString word = stream.readLine(); !word.isEmpty(); word = stream.readLine())
                ignoreWord (word);
            userDictonaryFile.close();
        }
    }
}
/*************************/
SpellChecker::~SpellChecker()
{
    delete hunspell_;
}
/*************************/
bool SpellChecker::spell (const QString& word)
{
    return hunspell_->spell (word.toStdString());
}
/*************************/
QStringList SpellChecker::suggest (const QString& word)
{
    const std::vector<std::string> strSuggestions = hunspell_->suggest (word.toStdString());
    QStringList suggestions;
    for (const auto &str : strSuggestions)
        suggestions << QString::fromStdString (str);
    return suggestions;
}
/*************************/
void SpellChecker::ignoreWord (const QString& word)
{
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    hunspell_->add (codec_->fromUnicode (word).toStdString());
#else
    QByteArray b = encoder_.encode (word);
    hunspell_->add (b.toStdString());
#endif
}
/*************************/
void SpellChecker::addToUserWordlist (const QString& word)
{
    ignoreWord (word);
    if (!userDictionary_.isEmpty())
    {
        QFile userDictonaryFile (userDictionary_);
        if (userDictonaryFile.open (QIODevice::Append))
        {
            QTextStream stream (&userDictonaryFile);
            stream << word << "\n";
            userDictonaryFile.close();
        }
    }
}

}
