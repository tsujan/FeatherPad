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

#include "singleton.h"
#include "ui_fp.h"
#include <QMimeDatabase>
#include <QFileInfo>

namespace FeatherPad {

static QMimeType getMimeType (const QFileInfo &fInfo)
{
    QMimeDatabase mimeDatabase;
    return mimeDatabase.mimeTypeForFile (fInfo);
}
/*************************/
void FPwin::toggleSyntaxHighlighting()
{
    int count = ui->tabWidget->count();
    if (count == 0) return;

    bool enableSH = ui->actionSyntax->isChecked();
    if (enableSH)
        makeBusy(); // it may take a while with huge texts

    for (int i = 0; i < count; ++i)
    {
        TextEdit *textEdit = qobject_cast< TabPage *>(ui->tabWidget->widget (i))->textEdit();
        syntaxHighlighting (textEdit, enableSH, textEdit->getLang());
    }

    if (TabPage *tabPage = qobject_cast< TabPage *>(ui->tabWidget->currentWidget()))
        updateLangBtn (tabPage->textEdit());

    if (enableSH)
        QTimer::singleShot (0, this, &FPwin::unbusy);
}
/*************************/
// Falls back to "url".
void FPwin::setProgLang (TextEdit *textEdit)
{
    if (textEdit == nullptr) return;

    QString fname = textEdit->getFileName();
    if (fname.isEmpty()) return;

    /* examine the (final) target if existing */
    QFileInfo fInfo (fname);
    if (fInfo.isSymLink())
    {
        const QString finalTarget = fInfo.canonicalFilePath();
        if (!finalTarget.isEmpty())
            fname = finalTarget;
        else
            fname = fInfo.symLinkTarget();
    }

    if (fname.endsWith (".sub"))
        return; // "url" is the default for TextEdit

    QString progLan;

    /* first check some endings */
    QString baseName = fname.section ('/', -1);
    const static QRegularExpression expr("\\A(?:[^/]*\\.[^/\\.]+)\\z");
    QRegularExpressionMatch match = expr.match (baseName);
    if (match.hasMatch())
    {
        if (fname.endsWith (".cpp") || fname.endsWith (".h"))
            progLan = "cpp";
        else if (fname.endsWith (".c"))
            progLan = "c";
        else if (fname.endsWith (".sh") || fname.endsWith (".bashrc") || fname.endsWith (".rules")
                 || baseName == ".bash_profile" || baseName == ".bash_functions"
                 || baseName == ".xprofile" || baseName == ".profile"
                 || baseName == ".bash_aliases" || baseName == ".mkshrc"
                 || baseName == ".zprofile" || baseName == ".zlogin"
                 || baseName == ".zshrc" || baseName == ".zshenv")
            progLan = "sh";
        else if (fname.endsWith (".rb"))
            progLan = "ruby";
        else if (fname.endsWith (".lua") || fname.endsWith (".nelua"))
            progLan = "lua";
        else if (fname.endsWith (".py"))
            progLan = "python";
        else if (fname.endsWith (".pl"))
            progLan = "perl";
        else if (fname.endsWith (".pro") || fname.endsWith (".pri"))
            progLan = "qmake";
        else if (fname.endsWith (".tr") || fname.endsWith (".t") || fname.endsWith (".roff"))
            progLan = "troff";
        else if (fname.endsWith (".tex") || fname.endsWith (".ltx") || fname.endsWith (".latex") || fname.endsWith (".lyx"))
            progLan = "LaTeX";
        else if (fname.endsWith (".xml", Qt::CaseInsensitive) || fname.endsWith (".svg", Qt::CaseInsensitive) || fname.endsWith (".qrc")
                 || fname.endsWith (".meta4", Qt::CaseInsensitive) || fname.endsWith (".metalink", Qt::CaseInsensitive)
                 || fname.endsWith (".rdf") || fname.endsWith (".docbook") || fname.endsWith (".fnx")
                 || fname.endsWith (".ts") || fname.endsWith (".menu") || fname.endsWith (".kml", Qt::CaseInsensitive)
                 || fname.endsWith (".xspf", Qt::CaseInsensitive) || fname.endsWith (".asx", Qt::CaseInsensitive)
                 || fname.endsWith (".nfo")/* || fname.endsWith (".ui") || fname.endsWith (".xul")*/)
            progLan = "xml";
        else if (fname.endsWith (".css") || fname.endsWith (".qss"))
            progLan = "css";
        else if (fname.endsWith (".scss"))
            progLan = "scss";
        else if (fname.endsWith (".p") || fname.endsWith (".pas"))
             progLan = "pascal";
        else if (fname.endsWith (".desktop") || fname.endsWith (".desktop.in") || fname.endsWith (".directory"))
             progLan = "desktop";
        else if (fname.endsWith (".kvconfig")
                 || fname.endsWith (".service") || fname.endsWith (".mount") || fname.endsWith (".timer") // systemd related
                 || baseName == "sources.list" || baseName == "sources.list.save"
                 || baseName == "mimeinfo.cache" || baseName == "defaults.list"
                 || baseName == "mimeapps.list" || baseName.endsWith ("-mimeapps.list")
                 || fname.endsWith (".pls", Qt::CaseInsensitive))
             progLan = "config";
        else if (fname.endsWith (".js") || fname.endsWith (".hx"))
            progLan = "javascript";
        else if (fname.endsWith (".java"))
            progLan = "java";
        else if (fname.endsWith (".json"))
            progLan = "json";
        else if (fname.endsWith (".qml"))
            progLan = "qml";
        else if (fname.endsWith (".log", Qt::CaseInsensitive))
            progLan = "log";
        else if (fname.endsWith (".php"))
            progLan = "php";
        else if (fname.endsWith (".diff") || fname.endsWith (".patch"))
            progLan = "diff";
        else if (fname.endsWith (".srt"))
            progLan = "srt";
        else if (fname.endsWith (".theme"))
             progLan = "theme";
        else if (fname.endsWith (".fountain"))
             progLan = "fountain";
        else if (fname.endsWith (".yml") || fname.endsWith (".yaml"))
             progLan = "yaml";
        else if (fname.endsWith (".m3u", Qt::CaseInsensitive))
            progLan = "m3u";
        else if (fname.endsWith (".htm", Qt::CaseInsensitive) || fname.endsWith (".html", Qt::CaseInsensitive))
            progLan = "html";
        else if (fname.endsWith (".markdown") || fname.endsWith (".md") || fname.endsWith (".mkd"))
            progLan = "markdown";
        else if (fname.endsWith (".rst"))
            progLan = "reST";
        else if (fname.endsWith (".dart"))
            progLan = "dart";
        else if (fname.endsWith (".go"))
            progLan = "go";
        else if (fname.endsWith (".rs"))
            progLan = "rust";
        else if (fname.endsWith (".tcl") || fname.endsWith (".tk"))
            progLan = "tcl";
        else if (baseName.startsWith ("makefile.", Qt::CaseInsensitive) && !baseName.endsWith (".txt", Qt::CaseInsensitive))
            progLan = "makefile";
        else if (baseName.compare ("CMakeLists.txt", Qt::CaseInsensitive) == 0)
            progLan = "cmake";
    }
    else if (baseName == "PKGBUILD" || baseName == "fstab")
        progLan = "sh";
    /* makefile is an exception */
    else if (baseName.compare ("makefile", Qt::CaseInsensitive) == 0)
        progLan = "makefile";
    else if (baseName.compare ("changelog", Qt::CaseInsensitive) == 0)
        progLan = "changelog";
    else if (baseName == "gtkrc")
        progLan = "gtkrc";
    else if (baseName == "control")
        progLan = "deb";
    else if (baseName == "mirrorlist")
        progLan = "config";

    if (progLan.isEmpty()) // now, check the mime type
    {
        if (!fInfo.exists())
            progLan = "url"; // fall back to the default language
        else
        {
            QMimeType mimeType = fInfo.isSymLink() ? getMimeType (QFileInfo (fname)) : getMimeType (fInfo);
            const QString mime = mimeType.name();
            QString parentMime;
            auto parents = mimeType.parentMimeTypes();
            if (!parents.isEmpty())
                parentMime = parents.at (0);

            if (mime == "text/x-c++" || mime == "text/x-c++src" || mime == "text/x-c++hdr" || mime == "text/x-chdr")
                progLan = "cpp";
            else if (mime == "text/x-c" || mime == "text/x-csrc")
                progLan = "c";
            else if (mime == "application/x-shellscript" || mime == "text/x-shellscript")
                progLan = "sh";
            else if (mime == "application/x-ruby")
                progLan = "ruby";
            else if (mime == "text/x-lua")
                progLan = "lua";
            else if (mime.startsWith ("text/x-python")) // it may be "text/x-python3"
                progLan = "python";
            else if (mime == "application/x-perl")
                progLan = "perl";
            else if (mime == "text/x-makefile")
                progLan = "makefile";
            else if (mime == "text/x-cmake")
                progLan = "cmake";
            else if (mime == "application/vnd.nokia.qt.qmakeprofile")
                progLan = "qmake";
            else if (mime == "text/troff" || mime == "application/x-troff-man")
                progLan = "troff";
            else if (mime == "text/x-tex" || mime == "application/x-lyx")
                progLan = "LaTeX";
            else if (mime == "text/html" || parentMime == "text/html" || mime == "application/xhtml+xml") // should come before xml check
                progLan = "html";
            else if (mime == "application/xml" || parentMime == "application/xml"
                     || mime == "text/feathernotes-fnx" || mime == "audio/x-ms-asx" || mime == "text/x-nfo" || mime == "application/xml-dtd")
                progLan = "xml";
            else if (mime == "text/css")
                progLan = "css";
            else if (mime == "text/x-scss")
                progLan = "scss";
            else if (mime == "text/x-pascal")
                progLan = "pascal";
            else if (mime == "text/x-changelog")
                progLan = "changelog";
            else if (mime == "application/x-desktop")
                progLan = "desktop";
            else if (mime == "audio/x-scpls" || mime == "application/vnd.kde.kcfgc")
                progLan = "config";
            else if (mime == "application/javascript")
                progLan = "javascript";
            else if (mime == "text/x-java")
                progLan = "java";
            else if (mime == "application/json")
                progLan = "json";
            else if (mime == "text/x-qml")
                progLan = "qml";
            else if (mime == "text/x-log")
                progLan = "log";
            else if (mime == "application/x-php" || mime == "text/x-php")
                progLan = "php";
            else if (mime == "application/x-theme")
                progLan = "theme";
            else if (mime == "application/x-yaml")
                progLan = "yaml";
            else if (mime == "text/x-diff" || mime == "text/x-patch")
                progLan = "diff";
            else if (mime == "text/markdown")
                progLan = "markdown";
            else if (mime == "audio/x-mpegurl" || mime == "application/vnd.apple.mpegurl")
                progLan = "m3u";
            else if (mime == "text/x-go")
                progLan = "go";
            else if (mime == "text/rust")
                progLan = "rust";
            else if (mime == "text/x-tcl" || mime == "text/tcl")
                progLan = "tcl";
            else if (fname.endsWith (".conf") || fname.endsWith (".ini"))
                progLan = "config"; // only if the mime type isn't found
            else // fall back to the default language
                progLan = "url";
        }
    }

    textEdit->setProg (progLan);
}
/*************************/
void FPwin::syntaxHighlighting (TextEdit *textEdit, bool highlight, const QString& lang)
{
    if (textEdit == nullptr
        || textEdit->isUneditable()) // has huge lines or isn't a text
    {
        return;
    }

    if (highlight)
    {
        QString progLan = lang; // first try the enforced language
        if (progLan.isEmpty())
            progLan = textEdit->getProg();
        if (progLan == "help" // used for marking the help doc
            || progLan.isEmpty()) // impossible; just a precaution
        {
            return;
        }

        Config config = static_cast<FPsingleton*>(qApp)->getConfig();
        if (textEdit->getSize() > config.getMaxSHSize()*1024*1024)
        {
            QTimer::singleShot (100, textEdit, [=]() {
                if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
                {
                    if (tabPage->textEdit() == textEdit)
                        showWarningBar ("<center><b><big>" + tr ("The size limit for syntax highlighting is exceeded.") + "</big></b></center>");
                }
            });
            return;
        }

        if (!qobject_cast< Highlighter *>(textEdit->getHighlighter()))
        {
            QPoint Point (0, 0);
            QTextCursor start = textEdit->cursorForPosition (Point);
            Point = QPoint (textEdit->geometry().width(), textEdit->geometry().height());
            QTextCursor end = textEdit->cursorForPosition (Point);

            textEdit->setDrawIndetLines (config.getShowWhiteSpace());
            textEdit->setVLineDistance (config.getVLineDistance());
            Highlighter *highlighter = new Highlighter (textEdit->document(), progLan, start, end,
                                                        textEdit->hasDarkScheme(),
                                                        config.getShowWhiteSpace(),
                                                        config.getShowEndings(),
                                                        config.getWhiteSpaceValue(),
                                                        config.customSyntaxColors().isEmpty()
                                                            ? textEdit->hasDarkScheme() ? config.darkSyntaxColors()
                                                                                        : config.lightSyntaxColors()
                                                            : config.customSyntaxColors());
            textEdit->setHighlighter (highlighter);
        }
        /* if the highlighter is created just now, it's necessary
           to wait until the text is completely loaded */
        QTimer::singleShot (0, textEdit, [this, textEdit]() {
            if (textEdit->isVisible())
            {
                formatTextRect(); // the text may be scrolled immediately after syntax highlighting (when reloading)
                matchBrackets(); // in case the cursor is beside a bracket when the text is loaded
            }
            connect (textEdit, &TextEdit::updateBracketMatching, this, &FPwin::matchBrackets);
            /* visible text may change on block removal */
            connect (textEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::formatOnBlockChange);
            connect (textEdit, &TextEdit::updateRect, this, &FPwin::formatTextRect);
            connect (textEdit, &TextEdit::resized, this, &FPwin::formatTextRect);
            /* this is needed when the whole visible text is pasted */
            connect (textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::formatOnTextChange);
        });
    }
    else if (Highlighter *highlighter = qobject_cast< Highlighter *>(textEdit->getHighlighter()))
    {
        disconnect (textEdit->document(), &QTextDocument::contentsChange, this, &FPwin::formatOnTextChange);
        disconnect (textEdit, &TextEdit::resized, this, &FPwin::formatTextRect);
        disconnect (textEdit, &TextEdit::updateRect, this, &FPwin::formatTextRect);
        disconnect (textEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::formatOnBlockChange);
        disconnect (textEdit, &TextEdit::updateBracketMatching, this, &FPwin::matchBrackets);

        /* remove bracket highlights */
        QList<QTextEdit::ExtraSelection> es = textEdit->extraSelections();
        int n = textEdit->getRedSel().count();
        while (n > 0 && !es.isEmpty())
        {
            es.removeLast();
            --n;
        }
        textEdit->setRedSel (QList<QTextEdit::ExtraSelection>());
        textEdit->setExtraSelections (es);

        textEdit->setDrawIndetLines (false);
        textEdit->setVLineDistance (0);

        textEdit->setHighlighter (nullptr);
        delete highlighter; highlighter = nullptr;
    }
}
/*************************/
void FPwin::formatOnTextChange (int /*position*/, int charsRemoved, int charsAdded) const
{
    if (charsRemoved > 0 || charsAdded > 0)
    {
        /* wait until the document's layout manager is notified about the change;
           otherwise, the end cursor might be out of range in formatTextRect() */
        QTimer::singleShot (0, this, &FPwin::formatTextRect);
    }
}
/*************************/
void FPwin::formatOnBlockChange (int/* newBlockCount*/) const
{
    formatTextRect();
}
/*************************/
void FPwin::formatTextRect() const
{
    /* It's supposed that this function is called for the current tab.
       That isn't always the case and won't do any harm if it isn't. */
    if (TabPage *tabPage = qobject_cast<TabPage*>(ui->tabWidget->currentWidget()))
    {
        TextEdit *textEdit = tabPage->textEdit();
        Highlighter *highlighter = qobject_cast< Highlighter *>(textEdit->getHighlighter());
        if (highlighter == nullptr) return;

        QPoint Point (0, 0);
        QTextCursor start = textEdit->cursorForPosition (Point);
        Point = QPoint (textEdit->width(), textEdit->height());
        QTextCursor end = textEdit->cursorForPosition (Point);

        highlighter->setLimit (start, end);
        QTextBlock block = start.block();
        while (block.isValid() && block.blockNumber() <= end.blockNumber())
        {
            if (TextBlockData *data = static_cast<TextBlockData *>(block.userData()))
            {
                if (!data->isHighlighted()) // isn't highlighted (completely)
                    highlighter->rehighlightBlock (block);
            }
            block = block.next();
        }
    }
}

}
