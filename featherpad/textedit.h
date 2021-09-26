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

#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include <QPlainTextEdit>
#include <QUrl>
#include <QMimeData>
#include <QDateTime>
#include <QElapsedTimer>
#include <QSyntaxHighlighter>

namespace FeatherPad {

/* This is for auto-indentation, line numbers, DnD, zooming, customized
   vertical scrollbar, appropriate signals, and saving/getting useful info. */
class TextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    TextEdit (QWidget *parent = nullptr, int bgColorValue = 255);
    ~TextEdit();

    void setTextCursor (const QTextCursor &cursor)
    {
        QPlainTextEdit::setTextCursor (cursor);
        /* this is needed for formatTextRect() to be called (for syntax highlighting) */
        emit QPlainTextEdit::updateRequest (rect(), 1);
    }

    void setEditorFont (const QFont &f, bool setDefault = true);
    void adjustScrollbars();

    void lineNumberAreaPaintEvent (QPaintEvent *event);
    int lineNumberAreaWidth();
    void showLineNumbers (bool show);

    void sortLines (bool reverse = false);

    bool toSoftTabs();

    QString getUrl (const int pos) const;

    QFont getDefaultFont() const {
        return font_;
    }

    QString getTextTab_() const {
        return textTab_;
    }
    void setTtextTab (int textTabSize) {
        textTab_ = textTab_.leftJustified (textTabSize, ' ', true);
    }

    QTextEdit::ExtraSelection currentLineSelection() {
        return currentLine_;
    }

    void setAutoIndentation (bool indent) {
        autoIndentation_ = indent;
    }
    bool getAutoIndentation() const {
        return autoIndentation_;
    }

    void setAutoReplace (bool replace) {
        autoReplace_ = replace;
    }
    bool getAutoReplace() const {
        return autoReplace_;
    }

    void setDrawIndetLines (bool draw) {
        drawIndetLines_ = draw;
    }

    void setVLineDistance (int distance) {
        vLineDistance_ = distance;
    }

    void setDateFormat (const QString &format) {
        dateFormat_ = format;
    }

    void setAutoBracket (bool autoB) {
        autoBracket_ = autoB;
    }

    bool hasDarkScheme() const {
        return (darkValue_ > -1);
    }
    int getDarkValue() const {
        return darkValue_;
    }

    QColor getTextPrintColor() const {
        /* with syntax highlighting, the color of line/document ends
           should be returned because the ordinary text is formatted */
        if (highlighter_)
            return separatorColor_;
        return (darkValue_ == -1 ? Qt::black : Qt::white);
    }

    void setCurLineHighlight (int value);

    void zooming (float range);

    qint64 getSize() const {
        return size_;
    }
    void setSize (qint64 size) {
        size_ = size;
    }

    QDateTime getLastModified() const {
        return lastModified_;
    }
    void setLastModified (const QDateTime& m) {
        lastModified_ = m;
    }

    int getWordNumber() const {
        return wordNumber_;
    }
    void setWordNumber (int n) {
        wordNumber_ = n;
    }

    QString getSearchedText() const {
        return searchedText_;
    }
    void setSearchedText (const QString &text) {
        searchedText_ = text;
    }

    QString getReplaceTitle() const {
        return replaceTitle_;
    }
    void setReplaceTitle (const QString &title) {
        replaceTitle_ = title;
    }

    QString getFileName() const {
        return fileName_;
    }
    void setFileName (const QString &name) {
        fileName_ = name;
    }

    QString getProg() const {
        return prog_.isEmpty() ? "url" // impossible; just a precaution
                               : prog_;
    }
    void setProg (const QString &prog) {
        prog_ = prog;
    }

    QString getLang() const {
        return lang_;
    }
    void setLang (const QString &lang) {
        lang_ = lang;
    }

    QString getEncoding() const {
        return encoding_;
    }
    void setEncoding (const QString &encoding) {
        encoding_ = encoding;
    }

    QList<QTextEdit::ExtraSelection> getGreenSel() const {
        return greenSel_;
    }
    void setGreenSel (QList<QTextEdit::ExtraSelection> sel) {
        greenSel_ = sel;
    }

    QList<QTextEdit::ExtraSelection> getRedSel() const {
        return redSel_;
    }
    void setRedSel (QList<QTextEdit::ExtraSelection> sel) {
        redSel_ = sel;
    }

    QList<QTextEdit::ExtraSelection> getBlueSel() const {
        return blueSel_;
    }

    bool isUneditable() const {
        return uneditable_;
    }
    void makeUneditable (bool readOnly) {
        uneditable_ = readOnly;
    }

    QSyntaxHighlighter *getHighlighter() const {
        return highlighter_;
    }
    void setHighlighter (QSyntaxHighlighter *h) {
        highlighter_ = h;
        matchedBrackets_ = false;
    }

    bool getInertialScrolling() const {
        return inertialScrolling_;
    }
    void setInertialScrolling (bool inertial) {
        inertialScrolling_ = inertial;
    }

    bool getSaveCursor() const {
        return saveCursor_;
    }
    void setSaveCursor (bool save) {
        saveCursor_ = save;
    }

    bool getThickCursor() const {
        return (cursorWidth() > 1);
    }
    void setThickCursor (bool thick) {
        setCursorWidth (thick ? 2 : 1);
    }

    void matchedBrackets() {
        matchedBrackets_ = true;
    }

    void forgetTxtCurHPos() {
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
    }

    bool getSelectionHighlighting() const {
        return selectionHighlighting_;
    }
    void setSelectionHighlighting (bool enable);

    void skipSelectionHighlighting() {
        highlightThisSelection_ = false;
    }

    void setPastePaths (bool pastePaths) {
        pastePaths_ = pastePaths;
    }

    QTextCursor finding (const QString& str, const QTextCursor& start,
                         QTextDocument::FindFlags flags = QTextDocument::FindFlags(),
                         bool isRegex = false, const int end = 0) const;

signals:
    /* inform the main widget */
    void fileDropped (const QString& localFile,
                      int restoreCursor, // Only for connecting to FPwin::newTabFromName().
                      int posInLine, // Only for connecting to FPwin::newTabFromName().
                      bool multiple); // Multiple files are dropped?
    void resized(); // needed by syntax highlighting
    void updateRect();
    void zoomedOut (TextEdit *textEdit); // needed for reformatting text
    void updateBracketMatching();

public slots:
    void copy();
    void cut();
    void undo();
    void redo();
    void paste();
    void selectAll();
    void insertPlainText (const QString &text);
    void selectionHlight();
    void onContentsChange (int position, int charsRemoved, int charsAdded);

protected:
    void keyPressEvent (QKeyEvent *event);
    void keyReleaseEvent (QKeyEvent *event);
    void wheelEvent (QWheelEvent *event);
    void resizeEvent (QResizeEvent *event);
    void timerEvent (QTimerEvent *event);
    void paintEvent (QPaintEvent *event); // only for working around the RTL bug
    void showEvent (QShowEvent *event);
    void mouseMoveEvent (QMouseEvent *event);
    void mousePressEvent (QMouseEvent *event);
    void mouseReleaseEvent (QMouseEvent *event);
    void mouseDoubleClickEvent (QMouseEvent *event);
    bool event (QEvent *event);
    bool eventFilter (QObject *watched, QEvent *event);

    QMimeData* createMimeDataFromSelection() const;
    /* we want to pass dropping of files to
       the main widget with a custom signal */
    bool canInsertFromMimeData (const QMimeData* source) const;
    void insertFromMimeData (const QMimeData* source);

private slots:
    void updateLineNumberAreaWidth (int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea (const QRect &rect, int dy);
    void onUpdateRequesting (const QRect&, int dy);
    void onSelectionChanged();
    void scrollWithInertia();

private:
    QString computeIndentation (const QTextCursor &cur) const;
    QString remainingSpaces (const QString& spaceTab, const QTextCursor& cursor) const;
    QTextCursor backTabCursor(const QTextCursor& cursor, bool twoSpace) const;

    int prevAnchor_, prevPos_; // used only for bracket matching
    QWidget *lineNumberArea_;
    QTextEdit::ExtraSelection currentLine_;
    QRect lastCurrentLine_;
    int widestDigit_;
    bool autoIndentation_;
    bool autoReplace_;
    bool drawIndetLines_;
    bool autoBracket_;
    int darkValue_;
    QColor separatorColor_;
    int vLineDistance_;
    QString dateFormat_;
    QColor lineHColor_;
    int resizeTimerId_, selectionTimerId_; // for not wasting CPU's time
    QPoint pressPoint_; // used internally for hyperlinks
    QFont font_; // used internally for keeping track of the unzoomed font
    QString textTab_; // text tab in terms of spaces
    QElapsedTimer tripleClickTimer_;
    /* To know whether text may be pasted, in contrast to text/file dropping: */
    bool pasting_;
    /* To keep text cursor's horizontal position with Up/Down keys
       (also used in a workaround for a Qt regression): */
    bool keepTxtCurHPos_;
    int txtCurHPos_;
    /********************************************
     ***** All needed information on a page *****
     ********************************************/
    qint64 size_; // file size for limiting syntax highlighting (the file may be removed)
    QDateTime lastModified_; // the last modification time for knowing about changes.
    int wordNumber_; // the calculated number of words (-1 if not counted yet)
    QString searchedText_; // the text that is being searched in the document
    QString replaceTitle_; // the title of the Replacement dock (can change)
    QString fileName_; // opened file
    QString prog_; // real programming language (never empty; defaults to "url")
    QString lang_; // selected (enforced) programming language (empty if nothing's enforced)
    QString encoding_; // text encoding (UTF-8 by default)
    /*
       Highlighting order: (1) current line;
                           (2) replacing;
                           (3) search matches;
                           (4) selection matches;
                           (5) bracket matches.
    */
    QList<QTextEdit::ExtraSelection> greenSel_; // for replaced matches
    QList<QTextEdit::ExtraSelection> blueSel_; // for selection highlighting
    QList<QTextEdit::ExtraSelection> redSel_; // for bracket matching
    bool selectionHighlighting_; // should selections be highlighted?
    bool highlightThisSelection_; // should this selection be highlighted?
    bool removeSelectionHighlights_; // used only internally
    bool matchedBrackets_; // is bracket matching done (is FPwin::matchBrackets called)?
    bool uneditable_; // the doc should be made uneditable because of its contents
    QSyntaxHighlighter *highlighter_; // syntax highlighter
    bool saveCursor_;
    bool pastePaths_;
    /******************************
     ***** Inertial scrolling *****
     ******************************/
    bool inertialScrolling_;
    struct scrollData {
      int delta;
      int leftSteps;
      int totalSteps;
    };
    QList<scrollData> queuedScrollSteps_;
    QTimer *scrollTimer_;
};
/*************************/
class LineNumberArea : public QWidget
{
    Q_OBJECT

public:
    LineNumberArea (TextEdit *Editor) : QWidget (Editor) {
        editor = Editor;
    }

    QSize sizeHint() const {
        return QSize (editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent (QPaintEvent *event) {
        editor->lineNumberAreaPaintEvent (event);
    }

    void mouseDoubleClickEvent (QMouseEvent *event) {
        if (rect().contains (event->pos()))
            editor->centerCursor();
        QWidget::mouseDoubleClickEvent (event);
    }

private:
    TextEdit *editor;
};

}

#endif // TEXTEDIT_H
