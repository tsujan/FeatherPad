/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014-2025 <tsujan2000@gmail.com>
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

#include <QApplication>
#include <QTimer>
#include <QDateTime>
#include <QPainter>
#include <QMenu>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QProcess>
#include <QRegularExpression>
#include <QClipboard>
#include <QTextDocumentFragment>
#include "textedit.h"
#include "vscrollbar.h"

#include <algorithm>
#include <cmath>

#define UPDATE_INTERVAL 50 // in ms
#define SCROLL_FRAMES_PER_SEC 60
#define SCROLL_DURATION 300 // in ms

namespace FeatherPad {

TextEdit::TextEdit (QWidget *parent, int bgColorValue) : QPlainTextEdit (parent)
{
    prevAnchor_ = prevPos_ = -1;
    widestDigit_ = 0;
    autoIndentation_ = true;
    autoReplace_ = true;
    autoBracket_ = false;
    drawIndetLines_ = false;
    saveCursor_ = false;
    pastePaths_ = false;
    vLineDistance_ = 0;
    matchedBrackets_ = false;

    inertialScrolling_ = false;
    scrollTimer_ = nullptr;

    mousePressed_ = false;

    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;

    prog_ = "url"; // the default language

    textTab_ = "    "; // the default text tab is four spaces

    resizeTimerId_ = 0;
    selectionTimerId_ = 0;
    selectionHighlighting_ = false;
    highlightThisSelection_ = true;
    removeSelectionHighlights_ = false;
    size_ = 0;
    wordNumber_ = -1; // not calculated yet
    encoding_= "UTF-8";
    uneditable_ = false;

    setMouseTracking (true);
    //document()->setUseDesignMetrics (true);

    /* set the backgound color and ensure enough contrast
       between the selection and line highlight colors */
    QPalette p = palette();
    bgColorValue = std::clamp (bgColorValue, 0, 255);
    if (bgColorValue < 230 && bgColorValue > 50) // not good for a text editor
        bgColorValue = 230;
    if (bgColorValue < 230)
    {
        darkValue_ = bgColorValue;
        viewport()->setStyleSheet (QString (".QWidget {"
                                            "color: white;"
                                            "background-color: rgb(%1, %1, %1);}")
                                   .arg (bgColorValue));
        QColor col = p.highlight().color();
        if (qGray (col.rgb()) - bgColorValue < 30 && col.hslSaturation() < 100)
        {
            setStyleSheet ("QPlainTextEdit {"
                           "selection-background-color: rgb(180, 180, 180);"
                           "selection-color: black;}");
        }
        else
        {
            col = p.color (QPalette::Inactive, QPalette::Highlight);
            if (qGray (col.rgb()) - bgColorValue < 30 && col.hslSaturation() < 100)
            { // also check the inactive highlight color
                p.setColor (QPalette::Inactive, QPalette::Highlight, p.highlight().color());
                p.setColor (QPalette::Inactive, QPalette::HighlightedText, p.highlightedText().color());
                setPalette (p);
            }
        }
        /* Use alpha in paintEvent to gray out the paragraph separators and
           document terminators. The real text will be formatted by the highlighter. */
        separatorColor_ = Qt::white;
        separatorColor_.setAlpha (95 - std::round (3 * static_cast<double>(darkValue_) / 5));
    }
    else
    {
        darkValue_ = -1;
        viewport()->setStyleSheet (QString (".QWidget {"
                                            "color: black;"
                                            "background-color: rgb(%1, %1, %1);}")
                                   .arg (bgColorValue));
        QColor col = p.highlight().color();
        if (bgColorValue - qGray (col.rgb()) < 30 && col.hslSaturation() < 100)
        {
            setStyleSheet ("QPlainTextEdit {"
                           "selection-background-color: rgb(100, 100, 100);"
                           "selection-color: white;}");
        }
        else
        {
            col = p.color (QPalette::Inactive, QPalette::Highlight);
            if (bgColorValue - qGray (col.rgb()) < 30 && col.hslSaturation() < 100)
            {
                p.setColor (QPalette::Inactive, QPalette::Highlight, p.highlight().color());
                p.setColor (QPalette::Inactive, QPalette::HighlightedText, p.highlightedText().color());
                setPalette (p);
            }
        }
        separatorColor_ = Qt::black;
        separatorColor_.setAlpha (2 * std::round (static_cast<double>(bgColorValue) / 5) - 32);
    }
    setCurLineHighlight (-1);

    setFrameShape (QFrame::NoFrame);
    /* first we replace the widget's vertical scrollbar with ours because
       we want faster wheel scrolling when the mouse cursor is on the scrollbar */
    VScrollBar *vScrollBar = new VScrollBar;
    setVerticalScrollBar (vScrollBar);

    lineNumberArea_ = new LineNumberArea (this);
    lineNumberArea_->setToolTip (tr ("Double click to center current line"));
    lineNumberArea_->hide();
    lineNumberArea_->installEventFilter (this);

    connect (this, &QPlainTextEdit::updateRequest, this, &TextEdit::onUpdateRequesting);
    connect (this, &QPlainTextEdit::cursorPositionChanged, [this] {
        if (!keepTxtCurHPos_)
            txtCurHPos_ = -1; // forget the last cursor position if it shouldn't be remembered
        emit updateBracketMatching();
        /* also, remove the column highlight if no mouse button is pressed */
        if (!colSel_.isEmpty() && !mousePressed_)
            removeColumnHighlight();
    });
    connect (this, &QPlainTextEdit::selectionChanged, this, &TextEdit::onSelectionChanged);
    connect (this, &QPlainTextEdit::copyAvailable, [this] (bool yes) {
        if (yes) emit canCopy (true);
        else if (colSel_.isEmpty()) emit canCopy (false);
    });

    setContextMenuPolicy (Qt::CustomContextMenu);
}
/*************************/
void TextEdit::setCurLineHighlight (int value)
{
    if (value >= 0 && value <= 255)
        lineHColor_ = QColor (value, value, value);
    else if (darkValue_ == -1)
        lineHColor_ = QColor (0, 0, 0, 4);
    else
    {
        /* a quadratic equation for darkValue_ -> opacity: 0 -> 20,  27 -> 8, 50 -> 2 */
        int opacity = std::clamp (static_cast<int>(std::round (static_cast<double>(darkValue_ * (19 * darkValue_ - 2813)) / 5175) + 20), 1, 30);
        lineHColor_ = QColor (255, 255, 255, opacity);

    }
}
/*************************/
bool TextEdit::eventFilter (QObject *watched, QEvent *event)
{
    if (watched == lineNumberArea_)
    {
        if (event->type() == QEvent::Wheel)
        {
            if (QWheelEvent *we = static_cast<QWheelEvent*>(event))
            {
                wheelEvent (we);
                return false;
            }
        }
        else if (event->type() == QEvent::MouseButtonPress)
            return true; // prevent the window from being dragged by widget styles (like Kvantum)
    }
    return QPlainTextEdit::eventFilter (watched, event);
}
/*************************/
void TextEdit::setEditorFont (const QFont &f, bool setDefault)
{
    if (setDefault)
        font_ = f;
    setFont (f);
    viewport()->setFont (f); // needed when whitespaces are shown
    document()->setDefaultFont (f);
    /* we want consistent tabs */
    QFontMetricsF metrics (f);
    QTextOption opt = document()->defaultTextOption();
    opt.setTabStopDistance (metrics.horizontalAdvance (textTab_));
    document()->setDefaultTextOption (opt);

    /* the line number is bold only for the current line */
    QFont F(f);
    if (f.bold())
    {
        F.setBold (false);
        lineNumberArea_->setFont (F);
    }
    else
        lineNumberArea_->setFont (f);
    /* find the widest digit (used in calculating line number area width)*/
    F.setBold (true); // it's bold for the current line
    widestDigit_ = 0;
    int maxW = 0;
    for (int i = 0; i < 10; ++i)
    {
        int w = QFontMetrics (F).horizontalAdvance (locale().toString (i));
        if (w > maxW)
        {
            maxW = w;
            widestDigit_ = i;
        }
    }
}
/*************************/
TextEdit::~TextEdit()
{
    if (scrollTimer_)
    {
        disconnect (scrollTimer_, &QTimer::timeout, this, &TextEdit::scrollWithInertia);
        scrollTimer_->stop();
        delete scrollTimer_;
    }
    delete lineNumberArea_;
}
/*************************/
void TextEdit::showLineNumbers (bool show)
{
    if (show)
    {
        lineNumberArea_->show();
        connect (this, &QPlainTextEdit::blockCountChanged, this, &TextEdit::updateLineNumberAreaWidth);
        connect (this, &QPlainTextEdit::updateRequest, this, &TextEdit::updateLineNumberArea);
        connect (this, &QPlainTextEdit::cursorPositionChanged, this, &TextEdit::highlightCurrentLine);

        updateLineNumberAreaWidth (0);
        highlightCurrentLine();
    }
    else
    {
        disconnect (this, &QPlainTextEdit::blockCountChanged, this, &TextEdit::updateLineNumberAreaWidth);
        disconnect (this, &QPlainTextEdit::updateRequest, this, &TextEdit::updateLineNumberArea);
        disconnect (this, &QPlainTextEdit::cursorPositionChanged, this, &TextEdit::highlightCurrentLine);

        lineNumberArea_->hide();
        setViewportMargins (0, 0, 0, 0);
        QList<QTextEdit::ExtraSelection> es = extraSelections();
        if (!es.isEmpty() && !currentLine_.cursor.isNull())
            es.removeFirst();
        setExtraSelections (es);
        currentLine_.cursor = QTextCursor(); // nullify currentLine_
        lastCurrentLine_ = QRect();
    }
}
/*************************/
int TextEdit::lineNumberAreaWidth()
{
    QString digit = locale().toString (widestDigit_);
    QString num = digit;
    int max = std::max (1, blockCount());
    while (max >= 10)
    {
        max /= 10;
        num += digit;
    }
    QFont f = font();
    f.setBold (true);
    return (6 + QFontMetrics (f).horizontalAdvance (num)); // 6 = 3 + 3 (-> lineNumberAreaPaintEvent)
}
/*************************/
void TextEdit::updateLineNumberAreaWidth (int /* newBlockCount */)
{
    if (QApplication::layoutDirection() == Qt::RightToLeft)
        setViewportMargins (0, 0, lineNumberAreaWidth(), 0);
    else
        setViewportMargins (lineNumberAreaWidth(), 0, 0, 0);
}
/*************************/
void TextEdit::updateLineNumberArea (const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea_->scroll (0, dy);
    else
    {
        /* since the current line number is distinguished from other numbers,
           its rectangle should be updated also when the line is wrapped */
        if (lastCurrentLine_.isValid())
            lineNumberArea_->update (0, lastCurrentLine_.y(), lineNumberArea_->width(), lastCurrentLine_.height());
        QRect totalRect;
        QTextCursor cur = cursorForPosition (rect.center());
        if (rect.contains (cursorRect (cur).center()))
        {
            QRectF blockRect = blockBoundingGeometry (cur.block()).translated (contentOffset());
            totalRect = rect.united (blockRect.toRect());
        }
        else
            totalRect = rect;
        lineNumberArea_->update (0, totalRect.y(), lineNumberArea_->width(), totalRect.height());
    }

    if (rect.contains (viewport()->rect()))
        updateLineNumberAreaWidth (0);
}
/*************************/
QString TextEdit::computeIndentation (const QTextCursor &cur) const
{
    QTextCursor cusror = cur;
    if (cusror.hasSelection())
        cusror.setPosition (std::min (cusror.anchor(), cusror.position()));
    QTextCursor tmp = cusror;
    tmp.movePosition (QTextCursor::StartOfBlock);
    QString str;
    if (tmp.atBlockEnd())
        return str;
    int pos = tmp.position();
    tmp.setPosition (++pos, QTextCursor::KeepAnchor);
    QString selected;
    while (!tmp.atBlockEnd()
           && tmp <= cusror
           && ((selected = tmp.selectedText()) == " "
               || (selected = tmp.selectedText()) == "\t"))
    {
        str.append (selected);
        tmp.setPosition (pos);
        tmp.setPosition (++pos, QTextCursor::KeepAnchor);
    }
    if (tmp.atBlockEnd()
        && tmp <= cusror
        && ((selected = tmp.selectedText()) == " "
            || (selected = tmp.selectedText()) == "\t"))
    {
        str.append (selected);
    }
    return str;
}
/*************************/
// Finds the (remaining) spaces that should be inserted with Ctrl+Tab.
QString TextEdit::remainingSpaces (const QString& spaceTab, const QTextCursor& cursor) const
{
    QTextCursor tmp = cursor;
    QString txt = cursor.block().text().left (cursor.positionInBlock());
    QFontMetricsF fm = QFontMetricsF (document()->defaultFont());
    double spaceL = fm.horizontalAdvance (" ");
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    { // find tab widths in terms of spaces
        tmp.setPosition (tmp.block().position() + i);
        double x = static_cast<double>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<double>(cursorRect (tmp).right()) - x;
        n += std::max (static_cast<int>(std::round (std::abs (x) / spaceL)) - 1, 0); // x is negative for RTL
        ++i;
    }
    n += txt.size();
    n = spaceTab.size() - n % spaceTab.size();
    QString res;
    for (int i = 0 ; i < n; ++i)
        res += " ";
    return res;
}
/*************************/
// Returns a cursor that selects the spaces to be removed by a backtab.
// If "twoSpace" is true, a 2-space backtab will be applied as far as possible.
QTextCursor TextEdit::backTabCursor (const QTextCursor& cursor, bool twoSpace) const
{
    QTextCursor tmp = cursor;
    tmp.movePosition (QTextCursor::StartOfBlock);
    /* find the start of the real text */
    const QString blockText = cursor.block().text();
    int indx = 0;
    QRegularExpressionMatch match;
    if (blockText.indexOf (QRegularExpression ("^\\s+"), 0, &match) > -1)
        indx = match.capturedLength();
    else
        return tmp;
    int txtStart = cursor.block().position() + indx;

    QString txt = blockText.left (indx);
    QFontMetricsF fm = QFontMetricsF (document()->defaultFont());
    double spaceL = fm.horizontalAdvance (" ");
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    { // find tab widths in terms of spaces
        tmp.setPosition (tmp.block().position() + i);
        double x = static_cast<double>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<double>(cursorRect (tmp).right()) - x;
        n += std::max (static_cast<int>(std::round (std::abs (x) / spaceL) - 1), 0);
        ++i;
    }
    n += txt.size();
    n = n % textTab_.size();
    if (n == 0) n = textTab_.size();

    if (twoSpace) n = std::min (n, 2);

    tmp.setPosition (txtStart);
    QChar ch = blockText.at (indx - 1);
    if (ch == QChar (QChar::Space))
        tmp.setPosition (txtStart - n, QTextCursor::KeepAnchor);
    else // the previous character is a tab
    {
        double x = static_cast<double>(cursorRect (tmp).right());
        tmp.setPosition (txtStart - 1, QTextCursor::KeepAnchor);
        x -= static_cast<double>(cursorRect (tmp).right());
        n -= std::round (std::abs (x) / spaceL);
        if (n < 0) n = 0; // impossible without "twoSpace"
        tmp.setPosition (tmp.position() - n, QTextCursor::KeepAnchor);
    }

    return tmp;
}
/*************************/
/* NOTE: "event->text()" should not be empty when calling this function because using
         "QPlainTextEdit::keyPressEvent(event)" here can ruin syntax highlighting. */
void TextEdit::prependToColumn (QKeyEvent *event)
{
    if (colSel_.count() > 1000)
        QTimer::singleShot (0, this, [this]() {emit hugeColumn();});
    else
    {
        bool origMousePressed = mousePressed_;
        mousePressed_ = true;
        QTextCursor cur = textCursor();
        cur.beginEditBlock();
        for (auto const &extra : std::as_const (colSel_))
        {
            if (!extra.cursor.hasSelection()) continue;
            cur = extra.cursor;
            cur.setPosition (std::min (cur.anchor(), cur.position()));
            cur.insertText (event->text());
            // TODO: overwriteMode() ?
        }
        cur.endEditBlock();
        mousePressed_ = origMousePressed;
    }
    event->accept();
}
/*************************/
static inline bool isOnlySpaces (const QString &str)
{
    int i = 0;
    while (i < str.size())
    { // always skip the starting spaces
        QChar ch = str.at (i);
        if (ch == QChar (QChar::Space) || ch == QChar (QChar::Tabulation))
            ++i;
        else return false;
    }
    return true;
}

void TextEdit::keyPressEvent (QKeyEvent *event)
{
    keepTxtCurHPos_ = false;

    /* first, deal with spacial cases of pressing Ctrl */
    if (event->modifiers() & Qt::ControlModifier)
    {
        if (event->modifiers() == Qt::ControlModifier) // no other modifier is pressed
        {
            /* deal with hyperlinks */
            if (event->key() == Qt::Key_Control) // no other key is pressed either
            {
                if (highlighter_)
                {
                    if (getUrl (cursorForPosition (viewport()->mapFromGlobal (QCursor::pos())).position()).isEmpty())
                    {
                        if (viewport()->cursor().shape() != Qt::IBeamCursor)
                            viewport()->setCursor (Qt::IBeamCursor);
                    }
                    else if (viewport()->cursor().shape() != Qt::PointingHandCursor)
                        viewport()->setCursor (Qt::PointingHandCursor);
                    QPlainTextEdit::keyPressEvent (event);
                    return;
                }
            }
        }
        if (event->key() != Qt::Key_Control) // another modifier/key is pressed
        {
            if (highlighter_ && viewport()->cursor().shape() != Qt::IBeamCursor)
                viewport()->setCursor (Qt::IBeamCursor);
        }
    }

    /* workarounds for copy/cut/... -- see TextEdit::copy()/cut()/... */
    if (event == QKeySequence::Copy)
    {
        copy();
        event->accept();
        return;
    }
    if (event == QKeySequence::Cut)
    {
        if (!isReadOnly())
            cut();
        event->accept();
        return;
    }
    if (event == QKeySequence::Paste)
    {
        if (!isReadOnly())
            paste();
        event->accept();
        return;
    }
    if (event == QKeySequence::SelectAll)
    {
        selectAll();
        event->accept();
        return;
    }
    if (event == QKeySequence::Undo)
    {
        /* QWidgetTextControl::undo() callls ensureCursorVisible() even when there's nothing to undo.
           Users may press Ctrl+Z just to know whether a document is in its original state and
           a scroll jump can confuse them when there's nothing to undo. Also see "TextEdit::undo()". */
        if (!isReadOnly() && document()->isUndoAvailable())
            undo();
        event->accept();
        return;
    }
    if (event == QKeySequence::Redo)
    {
        /* QWidgetTextControl::redo() calls ensureCursorVisible() even when there's nothing to redo.
           That may cause a scroll jump, which can be confusing when nothing else has happened.
           Also see "TextEdit::redo()". */
        if (!isReadOnly() && document()->isRedoAvailable())
            redo();
        event->accept();
        return;
    }

    if (isReadOnly())
    {
        QPlainTextEdit::keyPressEvent (event);
        return;
    }

    if (event == QKeySequence::Delete || event == QKeySequence::DeleteStartOfWord)
    {
        if (!colSel_.isEmpty() && event == QKeySequence::Delete)
        {
            deleteColumn();
            event->accept();
            return;
        }
        bool hadSelection (textCursor().hasSelection());
        QPlainTextEdit::keyPressEvent (event);
        if (!hadSelection)
            emit updateBracketMatching(); // isn't emitted in another way
        return;
    }

    if (event->key() == Qt::Key_Backspace)
    {
        if (!colSel_.isEmpty())
        {
            keepTxtCurHPos_ = false;
            txtCurHPos_ = -1;
            if (colSel_.count() > 1000)
                QTimer::singleShot (0, this, [this]() {emit hugeColumn();});
            else
            {
                bool origMousePressed = mousePressed_;
                mousePressed_ = true;
                QTextCursor cur = textCursor();
                cur.beginEditBlock();
                for (auto const &extra : std::as_const (colSel_))
                {
                    if (!extra.cursor.hasSelection()) continue;
                    cur = extra.cursor;
                    cur.setPosition (std::min (cur.anchor(), cur.position()));
                    if (cur.columnNumber() == 0)
                        continue; // don't go to the previous line
                    cur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                    cur.insertText ("");
                }
                cur.endEditBlock();
                mousePressed_ = origMousePressed;
                if (lineWrapMode() != QPlainTextEdit::NoWrap)
                { // the selection may have changed with a wrapped text
                    if (selectionTimerId_)
                    {
                        killTimer (selectionTimerId_);
                        selectionTimerId_ = 0;
                    }
                    selectionTimerId_ = startTimer (UPDATE_INTERVAL);
                }
            }
            event->accept();
            return;
        }
        keepTxtCurHPos_ = true;
        if (txtCurHPos_ < 0)
        {
            QTextCursor startCur = textCursor();
            startCur.movePosition (QTextCursor::StartOfLine);
            txtCurHPos_ = std::abs (cursorRect().left() - cursorRect (startCur).left()); // is negative for RTL
        }
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        keepTxtCurHPos_ = true;
        if (txtCurHPos_ < 0)
        {
            QTextCursor startCur = textCursor();
            startCur.movePosition (QTextCursor::StartOfLine);
            txtCurHPos_ = std::abs (cursorRect().left() - cursorRect (startCur).left());
        }

        QTextCursor cur = textCursor();
        QString selTxt = cur.selectedText();

        if (autoReplace_ && selTxt.isEmpty())
        {
            const int p = cur.positionInBlock();
            if (p > 1)
            {
                bool replaceStr = true;
                cur.beginEditBlock();
                if (prog_ == "url" || prog_ == "changelog"
                    || lang_ == "url" || lang_ == "changelog")
                { // not with programming languages
                    cur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 2);
                    const QString sel = cur.selectedText();
                    replaceStr = sel.endsWith (".");
                    if (!replaceStr)
                    {
                        if (sel == "--")
                        {
                            QTextCursor prevCur = cur;
                            prevCur.setPosition (cur.position());
                            prevCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                            if (prevCur.selectedText() != "-")
                                cur.insertText ("—");
                        }
                        else if (sel == "->")
                            cur.insertText ("→");
                        else if (sel == "<-")
                            cur.insertText ("←");
                        else if (sel == ">=")
                            cur.insertText ("≥");
                        else if (sel == "<=")
                            cur.insertText ("≤");
                    }
                }
                if (replaceStr && p > 2)
                {
                    cur = textCursor();
                    cur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 3);
                    const QString sel = cur.selectedText();
                    if (sel == "...")
                    {
                        QTextCursor prevCur = cur;
                        prevCur.setPosition (cur.position());
                        if (p > 3)
                        {
                            prevCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                            if (prevCur.selectedText() != ".")
                                cur.insertText ("…");
                        }
                        else cur.insertText ("…");
                    }
                }
                cur.endEditBlock();
                cur = textCursor(); // reset the current cursor
            }
        }

        bool isBracketed (false);
        QString prefix, indent;
        bool withShift (event->modifiers() & Qt::ShiftModifier);

        /* with Shift+Enter, find the non-letter prefix */
        if (withShift)
        {
            cur.clearSelection();
            setTextCursor (cur);
            const QString blockText = cur.block().text();
            int i = 0;
            int curBlockPos = cur.position() - cur.block().position();
            while (i < curBlockPos)
            {
                QChar ch = blockText.at (i);
                if (!ch.isLetterOrNumber())
                {
                    prefix += ch;
                    ++i;
                }
                else break;
            }
            /* still check if a letter or number follows */
            if (i < curBlockPos)
            {
                QChar c = blockText.at (i);
                if (c.isLetter())
                {
                    if (i + 1 < curBlockPos
                        && !prefix.isEmpty() && !prefix.at (prefix.size() - 1).isSpace()
                        && blockText.at (i + 1).isSpace())
                    { // non-letter and non-space character -> singlle letter -> space
                        prefix = blockText.left (i + 2);
                        QChar cc = QChar (c.unicode() + 1);
                        if (cc.isLetter()) prefix.replace (c, cc);
                    }
                    else if (i + 2 < curBlockPos
                             && !blockText.at (i + 1).isLetterOrNumber() && !blockText.at (i + 1).isSpace()
                             && blockText.at (i + 2).isSpace())
                    { // singlle letter -> non-letter and non-space character -> space
                        prefix = blockText.left (i + 3);
                        QChar cc = QChar (c.unicode() + 1);
                        if (cc.isLetter()) prefix.replace (c, cc);
                    }
                }
                else if (c.isNumber())
                { // making lists with numbers
                    QString num;
                    while (i < curBlockPos)
                    {
                        QChar ch = blockText.at (i);
                        if (ch.isNumber())
                        {
                            num += ch;
                            ++i;
                        }
                        else
                        {
                            if (!num.isEmpty())
                            {
                                QLocale l = locale();
                                l.setNumberOptions (QLocale::OmitGroupSeparator);
                                QChar ch = blockText.at (i);
                                if (ch.isSpace())
                                { // non-letter and non-space character -> number -> space
                                    if (!prefix.isEmpty() && !prefix.at (prefix.size() - 1).isSpace())
                                        num = l.toString (locale().toInt (num) + 1) + ch;
                                    else num = QString();
                                }
                                else if (i + 1 < curBlockPos
                                         && !ch.isLetterOrNumber() && !ch.isSpace()
                                         && blockText.at (i + 1).isSpace())
                                { // number -> non-letter and non-space character -> space
                                    num = l.toString (locale().toInt (num) + 1) + ch + blockText.at (i + 1);
                                }
                                else num = QString();
                            }
                            break;
                        }
                    }
                    if (i < curBlockPos) // otherwise, it'll be just a number
                        prefix += num;
                }
            }
        }
        else
        {
            /* find the indentation */
            if (autoIndentation_)
                indent = computeIndentation (cur);
            /* check whether a bracketed text is selected
               so that the cursor position is at its start */
            QTextCursor anchorCur = cur;
            anchorCur.setPosition (cur.anchor());
            if (autoBracket_
                && cur.position() == cur.selectionStart()
                && !cur.atBlockStart() && !anchorCur.atBlockEnd())
            {
                cur.setPosition (cur.position());
                cur.movePosition (QTextCursor::PreviousCharacter);
                cur.movePosition (QTextCursor::NextCharacter,
                                  QTextCursor::KeepAnchor,
                                  selTxt.size() + 2);
                QString selTxt1 = cur.selectedText();
                if (selTxt1 == "{" + selTxt + "}" || selTxt1 == "(" + selTxt + ")")
                    isBracketed = true;
                cur = textCursor(); // reset the current cursor
            }
        }

        if (withShift || autoIndentation_ || isBracketed)
        {
            cur.beginEditBlock();
            /* first press Enter normally... */
            cur.insertText (QChar (QChar::ParagraphSeparator));
            /* ... then, insert indentation... */
            cur.insertText (indent);
            /* ... and handle Shift+Enter or brackets */
            if (withShift)
                cur.insertText (prefix);
            else if (isBracketed)
            {
                cur.movePosition (QTextCursor::PreviousBlock);
                cur.movePosition (QTextCursor::EndOfBlock);
                int start = -1;
                QStringList lines = selTxt.split (QChar (QChar::ParagraphSeparator));
                if (lines.size() == 1)
                {
                    cur.insertText (QChar (QChar::ParagraphSeparator));
                    cur.insertText (indent);
                    start = cur.position();
                    if (!isOnlySpaces (lines. at (0)))
                        cur.insertText (lines. at (0));
                }
                else // multi-line
                {
                    for (int i = 0; i < lines.size(); ++i)
                    {
                        if (i == 0 && isOnlySpaces (lines. at (0)))
                            continue;
                        cur.insertText (QChar (QChar::ParagraphSeparator));
                        if (i == 0)
                        {
                            cur.insertText (indent);
                            start = cur.position();
                        }
                        else if (i == 1 && start == -1)
                            start = cur.position(); // the first line was only spaces
                        cur.insertText (lines. at (i));
                    }
                }
                cur.setPosition (start, start >= cur.block().position()
                                            ? QTextCursor::MoveAnchor
                                            : QTextCursor::KeepAnchor);
                setTextCursor (cur);
            }
            cur.endEditBlock();
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_ParenLeft
             || event->key() == Qt::Key_BraceLeft
             || event->key() == Qt::Key_BracketLeft
             || event->key() == Qt::Key_QuoteDbl)
    {
        if (!colSel_.isEmpty())
        {
            prependToColumn (event);
            return;
        }
        if (autoBracket_)
        {
            QTextCursor cursor = textCursor();
            bool autoB (false);
            if (!cursor.hasSelection())
            {
                if (cursor.atBlockEnd())
                    autoB = true;
                else
                {
                    QTextCursor tmp = cursor;
                    tmp.movePosition (QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                    if (!tmp.selectedText().at (0).isLetterOrNumber())
                        autoB = true;
                }
            }
            else if (cursor.position() == cursor.selectionStart())
                autoB = true;
            if (autoB)
            {
                int pos = cursor.position();
                int anch = cursor.anchor();
                cursor.beginEditBlock();
                cursor.setPosition (anch);
                if (event->key() == Qt::Key_ParenLeft)
                {
                    cursor.insertText (")");
                    cursor.setPosition (pos);
                    cursor.insertText ("(");
                }
                else if (event->key() == Qt::Key_BraceLeft)
                {
                    cursor.insertText ("}");
                    cursor.setPosition (pos);
                    cursor.insertText ("{");
                }
                else if (event->key() == Qt::Key_BracketLeft)
                {
                    cursor.insertText ("]");
                    cursor.setPosition (pos);
                    cursor.insertText ("[");
                }
                else// if (event->key() == Qt::Key_QuoteDbl)
                {
                    cursor.insertText ("\"");
                    cursor.setPosition (pos);
                    cursor.insertText ("\"");
                }
                /* select the text and set the cursor at its start */
                cursor.setPosition (anch + 1, QTextCursor::MoveAnchor);
                cursor.setPosition (pos + 1, QTextCursor::KeepAnchor);
                cursor.endEditBlock();
                highlightThisSelection_ = false;
                /* WARNING: Why does putting "setTextCursor()" before "endEditBlock()"
                            cause a crash with huge lines? Most probably, a Qt bug. */
                setTextCursor (cursor);
                event->accept();
                return;
            }
        }
    }
    else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)
    {
        /* when text is selected, use arrow keys
           to go to the start or end of the selection */
        QTextCursor cursor = textCursor();
        if (event->modifiers() == Qt::NoModifier && cursor.hasSelection())
        {
            QString selTxt = cursor.selectedText();
            if (event->key() == Qt::Key_Left)
            {
                if (selTxt.isRightToLeft())
                    cursor.setPosition (cursor.selectionEnd());
                else
                    cursor.setPosition (cursor.selectionStart());
            }
            else
            {
                if (selTxt.isRightToLeft())
                    cursor.setPosition (cursor.selectionStart());
                else
                    cursor.setPosition (cursor.selectionEnd());
            }
            cursor.clearSelection();
            setTextCursor (cursor);
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Up)
    {
        if (event->modifiers() == Qt::ControlModifier)
        {
            if (QScrollBar* vbar = verticalScrollBar())
            { // scroll without changing the cursor position
                vbar->setValue(vbar->value() + (event->key() == Qt::Key_Down ? 1 : -1));
                event->accept();
                return;
            }
        }
        else if (event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier))
        { // move the line(s)
            QTextCursor cursor = textCursor();
            int anch = cursor.anchor();
            int pos = cursor.position();

            QTextCursor tmp = cursor;
            tmp.setPosition (anch);
            int anchorInBlock = tmp.positionInBlock();
            tmp.setPosition (pos);
            int posInBlock = tmp.positionInBlock();

            if (event->key() == Qt::Key_Down)
            {
                highlightThisSelection_ = false;
                cursor.beginEditBlock();
                cursor.setPosition (std::min (anch, pos));
                cursor.movePosition (QTextCursor::StartOfBlock);
                cursor.setPosition (std::max (anch, pos), QTextCursor::KeepAnchor);
                cursor.movePosition (QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                QString txt = cursor.selectedText();
                if (cursor.movePosition (QTextCursor::NextBlock, QTextCursor::KeepAnchor))
                {
                    cursor.deleteChar();
                    cursor.movePosition (QTextCursor::EndOfBlock);
                    cursor.insertText (QString (QChar::ParagraphSeparator));
                    int firstLine = cursor.position();
                    cursor.insertText (txt);
                    if (anch >= pos)
                    {
                        cursor.setPosition (cursor.block().position() + anchorInBlock);
                        cursor.setPosition (firstLine + posInBlock, QTextCursor::KeepAnchor);
                    }
                    else
                    {
                        cursor.movePosition (QTextCursor::StartOfBlock);
                        int lastLine = cursor.position();
                        cursor.setPosition (firstLine + anchorInBlock);
                        cursor.setPosition (lastLine + posInBlock, QTextCursor::KeepAnchor);
                    }
                    cursor.endEditBlock();
                    setTextCursor (cursor);
                    ensureCursorVisible();
                    event->accept();
                    return;
                }
                else cursor.endEditBlock();
            }
            else
            {
                highlightThisSelection_ = false;
                cursor.beginEditBlock();
                cursor.setPosition (std::max (anch, pos));
                cursor.movePosition (QTextCursor::EndOfBlock);
                cursor.setPosition (std::min (anch, pos), QTextCursor::KeepAnchor);
                cursor.movePosition (QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
                QString txt = cursor.selectedText();
                if (cursor.movePosition (QTextCursor::PreviousBlock, QTextCursor::KeepAnchor))
                {
                    cursor.movePosition (QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                    cursor.deleteChar();
                    cursor.movePosition (QTextCursor::StartOfBlock);
                    int firstLine = cursor.position();
                    cursor.insertText (txt);
                    cursor.insertText (QString (QChar::ParagraphSeparator));
                    cursor.movePosition (QTextCursor::PreviousBlock);
                    if (anch >= pos)
                    {
                        cursor.setPosition (cursor.block().position() + anchorInBlock);
                        cursor.setPosition (firstLine + posInBlock, QTextCursor::KeepAnchor);
                    }
                    else
                    {
                        int lastLine = cursor.position();
                        cursor.setPosition (firstLine + anchorInBlock);
                        cursor.setPosition (lastLine + posInBlock, QTextCursor::KeepAnchor);
                    }
                    cursor.endEditBlock();
                    setTextCursor (cursor);
                    ensureCursorVisible();
                    event->accept();
                    return;
                }
                else cursor.endEditBlock();
            }
        }
        else if (event->modifiers() == Qt::NoModifier
                || (!(event->modifiers() & Qt::AltModifier)
                    && ((event->modifiers() & Qt::ShiftModifier)
                        || (event->modifiers() & Qt::MetaModifier)
                        || (event->modifiers() & Qt::KeypadModifier))))
        {
            /* NOTE: This also includes a useful Qt feature with Down/Up after Backspace/Enter.
                     The feature was removed with Backspace due to a regression in Qt 5.14.1. */
            keepTxtCurHPos_ = true;
            QTextCursor cursor = textCursor();
            int hPos;
            if (txtCurHPos_ >= 0)
                hPos = txtCurHPos_;
            else
            {
                QTextCursor startCur = cursor;
                startCur.movePosition (QTextCursor::StartOfLine);
                hPos = std::abs (cursorRect().left() - cursorRect (startCur).left()); // is negative for RTL
                txtCurHPos_ = hPos;
            }
            QTextCursor::MoveMode mode = ((event->modifiers() & Qt::ShiftModifier)
                                              ? QTextCursor::KeepAnchor
                                              : QTextCursor::MoveAnchor);
            if ((event->modifiers() & Qt::MetaModifier))
            { // try to restore the cursor pixel position between blocks
                cursor.movePosition (event->key() == Qt::Key_Down
                                         ? QTextCursor::EndOfBlock
                                         : QTextCursor::StartOfBlock,
                                     mode);
                if (cursor.movePosition (event->key() == Qt::Key_Down
                                             ? QTextCursor::NextBlock
                                             : QTextCursor::PreviousBlock,
                                         mode))
                {
                    setTextCursor (cursor); // WARNING: This is needed because of a Qt bug.
                    bool rtl (cursor.block().textDirection() == Qt::RightToLeft);
                    QPoint cc = cursorRect (cursor).center();
                    cursor.setPosition (cursorForPosition (QPoint (cc.x() + (rtl ? -1 : 1) * hPos, cc.y())).position(), mode);
                }
            }
            else
            { // try to restore the cursor pixel position between lines
                cursor.movePosition (event->key() == Qt::Key_Down
                                         ? QTextCursor::EndOfLine
                                         : QTextCursor::StartOfLine,
                                     mode);
                if (cursor.movePosition (event->key() == Qt::Key_Down
                                             ? QTextCursor::NextCharacter
                                             : QTextCursor::PreviousCharacter,
                                         mode))
                { // next/previous line or block
                    cursor.movePosition (QTextCursor::StartOfLine, mode);
                    setTextCursor (cursor); // WARNING: This is needed because of a Qt bug.
                    bool rtl (cursor.block().textDirection() == Qt::RightToLeft);
                    QPoint cc = cursorRect (cursor).center();
                    cursor.setPosition (cursorForPosition (QPoint (cc.x() + (rtl ? -1 : 1) * hPos, cc.y())).position(), mode);
                }
            }
            setTextCursor (cursor);
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_PageDown || event->key() == Qt::Key_PageUp)
    {
        if (event->modifiers() == Qt::ControlModifier)
        {
            if (QScrollBar* vbar = verticalScrollBar())
            { // scroll without changing the cursor position
                vbar->setValue(vbar->value() + (event->key() == Qt::Key_PageDown ? 1 : -1) * vbar->pageStep());
                event->accept();
                return;
            }
        }
    }
    else if (event->key() == Qt::Key_Tab)
    {
        QTextCursor cursor = textCursor();
        int newLines = cursor.selectedText().count (QChar (QChar::ParagraphSeparator));
        if (newLines > 0)
        {
            highlightThisSelection_ = false;
            cursor.beginEditBlock();
            cursor.setPosition (std::min (cursor.anchor(), cursor.position())); // go to the first block
            cursor.movePosition (QTextCursor::StartOfBlock);
            for (int i = 0; i <= newLines; ++i)
            {
                /* skip all spaces to align the real text */
                int indx = 0;
                QRegularExpressionMatch match;
                if (cursor.block().text().indexOf (QRegularExpression ("^\\s+"), 0, &match) > -1)
                    indx = match.capturedLength();
                cursor.setPosition (cursor.block().position() + indx);
                if (event->modifiers() & Qt::ControlModifier)
                {
                    cursor.insertText (remainingSpaces (event->modifiers() & Qt::MetaModifier
                                                        ? "  " : textTab_, cursor));
                }
                else
                    cursor.insertText ("\t");
                if (!cursor.movePosition (QTextCursor::NextBlock))
                    break; // not needed
            }
            cursor.endEditBlock();
            ensureCursorVisible();
            event->accept();
            return;
        }
        else if (event->modifiers() & Qt::ControlModifier)
        {
            if (!colSel_.isEmpty())
            {
                if (colSel_.count() > 1000)
                    QTimer::singleShot (0, this, [this]() {emit hugeColumn();});
                else
                {
                    bool origMousePressed = mousePressed_;
                    mousePressed_ = true;
                    const QString spaceTab (event->modifiers() & Qt::MetaModifier ? "  " : textTab_);
                    cursor.beginEditBlock();
                    for (auto const &extra : std::as_const (colSel_))
                    {
                        if (!extra.cursor.hasSelection()) continue;
                        cursor = extra.cursor;
                        cursor.setPosition (std::min (cursor.anchor(), cursor.position()));
                        cursor.insertText (remainingSpaces (spaceTab, cursor));
                    }
                    cursor.endEditBlock();
                    mousePressed_ = origMousePressed;
                }
                event->accept();
                return;
            }
            QTextCursor tmp (cursor);
            tmp.setPosition (std::min (tmp.anchor(), tmp.position()));
            cursor.insertText (remainingSpaces (event->modifiers() & Qt::MetaModifier
                                                ? "  " : textTab_, tmp));
            ensureCursorVisible();
            event->accept();
            return;
        }
        else if (!colSel_.isEmpty())
        {
            prependToColumn (event);
            return;
        }
    }
    else if (event->key() == Qt::Key_Backtab)
    {
        QTextCursor cursor = textCursor();
        int newLines = cursor.selectedText().count (QChar (QChar::ParagraphSeparator));
        cursor.setPosition (std::min (cursor.anchor(), cursor.position()));
        highlightThisSelection_ = false;
        cursor.beginEditBlock();
        cursor.movePosition (QTextCursor::StartOfBlock);
        for (int i = 0; i <= newLines; ++i)
        {
            if (cursor.atBlockEnd())
            {
                if (!cursor.movePosition (QTextCursor::NextBlock))
                    break; // not needed
                continue;
            }
            cursor = backTabCursor (cursor, event->modifiers() & Qt::MetaModifier
                                            ? true : false);
            cursor.removeSelectedText();
            if (!cursor.movePosition (QTextCursor::NextBlock))
                break; // not needed
        }
        cursor.endEditBlock();
        ensureCursorVisible();

        /* otherwise, do nothing with SHIFT+TAB */
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_Insert)
    {
        if (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::KeypadModifier)
        {
            setOverwriteMode (!overwriteMode());
            if (!overwriteMode())
                update(); // otherwise, a part of the thick cursor might remain
            event->accept();
            return;
        }
    }
    /* because of a bug in Qt, the non-breaking space (ZWNJ) may not be inserted with SHIFT+SPACE */
    else if (event->key() == 0x200c)
    {
        insertPlainText (QChar (0x200C));
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_Space)
    {
        if (!colSel_.isEmpty())
        {
            prependToColumn (event);
            return;
        }
        if (autoReplace_ && event->modifiers() == Qt::NoModifier)
        {
            QTextCursor cur = textCursor();
            if (!cur.hasSelection())
            {
                const int p = cur.positionInBlock();
                if (p > 1)
                {
                    bool replaceStr = true;
                    cur.beginEditBlock();
                    if (prog_ == "url" || prog_ == "changelog"
                        || lang_ == "url" || lang_ == "changelog")
                    { // not with programming languages
                        cur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 2);
                        const QString selTxt = cur.selectedText();
                        replaceStr = selTxt.endsWith (".");
                        if (!replaceStr)
                        {
                            if (selTxt == "--")
                            {
                                QTextCursor prevCur = cur;
                                prevCur.setPosition (cur.position());
                                prevCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                                if (prevCur.selectedText() != "-")
                                    cur.insertText ("—");
                            }
                            else if (selTxt == "->")
                                cur.insertText ("→");
                            else if (selTxt == "<-")
                                cur.insertText ("←");
                            else if (selTxt == ">=")
                                cur.insertText ("≥");
                            else if (selTxt == "<=")
                                cur.insertText ("≤");
                        }
                    }
                    if (replaceStr && p > 2)
                    {
                        cur = textCursor();
                        cur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 3);
                        const QString selTxt = cur.selectedText();
                        if (selTxt == "...")
                        {
                            QTextCursor prevCur = cur;
                            prevCur.setPosition (cur.position());
                            if (p > 3)
                            {
                                prevCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                                if (prevCur.selectedText() != ".")
                                    cur.insertText ("…");
                            }
                            else cur.insertText ("…");
                        }
                    }

                    cur.endEditBlock();
                }
            }
        }
    }
    else if (event->key() == Qt::Key_Home)
    {
        if (!(event->modifiers() & Qt::ControlModifier))
        { // Qt's default behavior isn't acceptable
            QTextCursor cur = textCursor();
            int p = cur.positionInBlock();
            int indx = 0;
            QRegularExpressionMatch match;
            if (cur.block().text().indexOf (QRegularExpression ("^\\s+"), 0, &match) > -1)
                indx = match.capturedLength();
            if (p > 0)
            {
                if (p <= indx) p = 0;
                else p = indx;
            }
            else p = indx;
            cur.setPosition (p + cur.block().position(),
                             event->modifiers() & Qt::ShiftModifier ? QTextCursor::KeepAnchor
                                                                    : QTextCursor::MoveAnchor);
            setTextCursor (cur);
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    else if (!colSel_.isEmpty() && !event->text().isEmpty())
    {
        prependToColumn (event);
        return;
    }

    QPlainTextEdit::keyPressEvent (event);
}
/*************************/
// QPlainTextEdit doesn't give a plain text to the clipboard on copying/cutting
// but we're interested only in plain text.
void TextEdit::copy()
{
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
        QApplication::clipboard()->setText (cursor.selection().toPlainText());
    else
        copyColumn();
}
void TextEdit::cut()
{
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
    {
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        QApplication::clipboard()->setText (cursor.selection().toPlainText());
        cursor.removeSelectedText();
    }
    else
        cutColumn();
}
/*************************/
void TextEdit::deleteText()
{
    if (textCursor().hasSelection())
    {
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        insertPlainText ("");
    }
    else
        deleteColumn();
}
/*************************/
// These methods are overridden to forget the horizontal position of the text cursor and...
void TextEdit::undo()
{
    /* always remove replacing highlights before undoing */
    setGreenSel (QList<QTextEdit::ExtraSelection>());
    if (getSearchedText().isEmpty()) // FPwin::hlight() won't be called
    {
        QList<QTextEdit::ExtraSelection> es;
        if (!currentLine_.cursor.isNull())
            es.prepend (currentLine_);
        es.append (blueSel_);
        es.append (colSel_);
        es.append (redSel_);
        setExtraSelections (es);
    }

    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    QPlainTextEdit::undo();

    /* because of a bug in Qt, "QPlainTextEdit::selectionChanged()"
       may not be emitted after undoing */
    removeSelectionHighlights_ = true;
    selectionHlight();
}
void TextEdit::redo()
{
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    QPlainTextEdit::redo();

    removeSelectionHighlights_ = true;
    selectionHlight();
}
void TextEdit::paste()
{
    keepTxtCurHPos_ = false; // txtCurHPos_ isn't reset here because there may be nothing to paste
    if (!colSel_.isEmpty()) pasteOnColumn();
    else QPlainTextEdit::paste(); // calls insertFromMimeData() in Qt -> "qwidgettextcontrol.cpp"
}
void TextEdit::selectAll()
{
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1; // Qt bug: cursorPositionChanged() isn't emitted with selectAll()
    removeColumnHighlight();
    QPlainTextEdit::selectAll();
}
void TextEdit::insertPlainText (const QString &text)
{
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    QPlainTextEdit::insertPlainText (text);
}
/*************************/
QMimeData* TextEdit::createMimeDataFromSelection() const
{
    /* Prevent a rich text in the selection clipboard when the text
       is selected by the mouse. Also, see TextEdit::copy()/cut(). */
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
    {
        QMimeData *mimeData = new QMimeData;
        mimeData->setText (cursor.selection().toPlainText());
        return mimeData;
    }
    return nullptr;
}
/*************************/
static bool containsPlainText (const QStringList &list)
{
    for (const auto &str : list)
    {
        if (str.compare ("text/plain", Qt::CaseInsensitive) == 0
            || str.startsWith ("text/plain;charset=", Qt::CaseInsensitive))
        {
            return true;
        }
    }
    return false;
}
// This should be used, instead of "QPlainTextEdit::canPaste()", to set the
// enabled state of paste actions (when copied files are going to be pasted)
// because "canInsertFromMimeData()" is overridden below.
bool TextEdit::pastingIsPossible() const {
    if (textInteractionFlags() & Qt::TextEditable)
    {
        const QMimeData *md = QGuiApplication::clipboard()->mimeData();
        return md != nullptr
               && (md->hasUrls()
                   || (containsPlainText (md->formats()) && !md->text().isEmpty()));
    }
    return false;
}
// We want to pass dropping of files to the main widget by not accepting it here.
// We also want to control whether the pasted URLs should be opened.
bool TextEdit::canInsertFromMimeData (const QMimeData* source) const
{
    return source != nullptr
           && !source->hasUrls() // let the main widget handle dropped files
           && containsPlainText (source->formats()) && !source->text().isEmpty();
}
void TextEdit::insertFromMimeData (const QMimeData* source)
{
    keepTxtCurHPos_ = false;
    if (source == nullptr) return;
    if (source->hasUrls())
    {
        const QList<QUrl> urlList = source->urls();
        bool multiple (urlList.count() > 1);
        if (pastePaths_)
        {
            QTextCursor cur = textCursor();
            cur.beginEditBlock();
            for (const auto &url : urlList)
            {
                /* encode spaces of non-local paths to have a good highlighting
                   but remove the schemes of local paths */
                cur.insertText (url.isLocalFile() ? url.toLocalFile()
                                                  : url.toString (QUrl::EncodeSpaces));
                if (multiple)
                    cur.insertText ("\n");
            }
            cur.endEditBlock();
            ensureCursorVisible();
        }
        else
        {
            txtCurHPos_ = -1; // Qt bug: cursorPositionChanged() isn't emitted with file dropping
            for (const QUrl &url : urlList)
            {
                QString file;
                QString scheme = url.scheme();
                if (scheme == "admin") // gvfs' "admin:///"
                    file = url.adjusted (QUrl::NormalizePathSegments).path();
                else if (scheme == "file" || scheme.isEmpty())
                    file = url.adjusted (QUrl::NormalizePathSegments)  // KDE may give a double slash
                            .toLocalFile();
                else
                    continue;
                emit filePasted (file, 0, 0, multiple);
            }
        }
    }
    else if (containsPlainText (source->formats()) && !source->text().isEmpty())
        QPlainTextEdit::insertFromMimeData (source);
}
/*************************/
void TextEdit::copyColumn()
{
    QString res;
    for (auto const &extra : std::as_const (colSel_))
    {
        res.append (extra.cursor.selection().toPlainText());
        res.append ('\n');
    }
    if (!res.isEmpty())
    {
        res.remove (res.size() - 1, 1);
        if (!res.isEmpty())
            QApplication::clipboard()->setText (res);
    }
}
/*************************/
void TextEdit::cutColumn()
{
    if (colSel_.isEmpty()) return;
    if (colSel_.count() > 1000)
    {
        emit hugeColumn();
        return;
    }
    QString res;
    QTextCursor cur = textCursor();
    cur.beginEditBlock();
    for (auto const &extra : std::as_const (colSel_))
    {
        res.append (extra.cursor.selectedText());
        res.append ('\n');
        if (!extra.cursor.hasSelection()) continue;
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        cur = extra.cursor;
        cur.removeSelectedText();
    }
    cur.endEditBlock();
    if (!res.isEmpty())
    {
        res.remove (res.size() - 1, 1);
        if (!res.isEmpty())
            QApplication::clipboard()->setText (res);
    }
    removeColumnHighlight();
}
/*************************/
void TextEdit::deleteColumn()
{
    if (colSel_.isEmpty()) return;
    if (colSel_.count() > 1000)
    {
        emit hugeColumn();
        return;
    }
    QTextCursor cur = textCursor();
    cur.beginEditBlock();
    for (auto const &extra : std::as_const (colSel_))
    {
        if (!extra.cursor.hasSelection()) continue;
        keepTxtCurHPos_ = false;
        txtCurHPos_ = -1;
        cur = extra.cursor;
        cur.removeSelectedText();
    }
    cur.endEditBlock();
    removeColumnHighlight();
}
/*************************/
void TextEdit::pasteOnColumn()
{
    if (colSel_.isEmpty()) return;
    if (colSel_.count() > 1000)
    {
        emit hugeColumn();
        return;
    }
    bool origMousePressed = mousePressed_;
    mousePressed_ = true;
    QStringList parts = QApplication::clipboard()->text().split ('\n');
    QTextCursor cur = textCursor();
    cur.beginEditBlock();
    int i = 0;
    for (auto const &extra : std::as_const (colSel_))
    {
        if (i >= parts.size()) break;
        cur = extra.cursor;
        cur.insertText (parts.at (i));
        ++i;
    }
    cur.endEditBlock();
    mousePressed_ = origMousePressed;
    if (i > 0)
        keepTxtCurHPos_ = false;

    for (auto const &extra : std::as_const (colSel_))
    {
        if (extra.cursor.hasSelection())
        {
            if (selectionTimerId_)
            {
                killTimer (selectionTimerId_);
                selectionTimerId_ = 0;
            }
            selectionTimerId_ = startTimer (UPDATE_INTERVAL);
            return;
        }
    }
    removeColumnHighlight();
}
/*************************/
void TextEdit::keyReleaseEvent (QKeyEvent *event)
{
    /* deal with hyperlinks */
    if (highlighter_ && event->key() == Qt::Key_Control
        && viewport()->cursor().shape() != Qt::IBeamCursor)
    {
        viewport()->setCursor (Qt::IBeamCursor);
    }
    QPlainTextEdit::keyReleaseEvent (event);
}
/*************************/
void TextEdit::wheelEvent (QWheelEvent *event)
{
    QPoint anglePoint = event->angleDelta();
    if (event->modifiers() == Qt::ControlModifier)
    {
        float delta = anglePoint.y() / 120.f;
        zooming (delta);
        return;
    }

    bool horizontal (std::abs (anglePoint.x()) > std::abs (anglePoint.y()));

    if ((event->modifiers() & Qt::ShiftModifier) && QApplication::wheelScrollLines() > 1)
    { // line-by-line scrolling when Shift is pressed
        QScrollBar *sbar = nullptr;
        if (horizontal
            // horizontal scrolling when Alt is also pressed
            || (event->modifiers() & Qt::AltModifier))
        {
            sbar = horizontalScrollBar();
            if (!horizontal && !(sbar && sbar->isVisible()))
                sbar = verticalScrollBar();
        }
        else
            sbar = verticalScrollBar();
        if (sbar && sbar->isVisible())
        {
            int delta = horizontal ? anglePoint.x()
                                   : anglePoint.y();
            if (std::abs (delta) >= QApplication::wheelScrollLines())
            {
                QWheelEvent e (event->position(),
                               event->globalPosition(),
                               event->pixelDelta(),
                               QPoint (0, delta / QApplication::wheelScrollLines()),
                               event->buttons(),
                               Qt::NoModifier,
                               event->phase(),
                               false,
                               event->source());
                QCoreApplication::sendEvent (sbar, &e);
            }
            return;
        }
    }

    if ((event->modifiers() & Qt::AltModifier) && !horizontal)
    { // horizontal scrolling when Alt is pressed
        QScrollBar *hbar = horizontalScrollBar();
        if (hbar && hbar->isVisible())
        {
            QWheelEvent e (event->position(),
                           event->globalPosition(),
                           event->pixelDelta(),
                           QPoint (0, anglePoint.y()),
                           event->buttons(),
                           Qt::NoModifier,
                           event->phase(),
                           false,
                           event->source());
            QCoreApplication::sendEvent (hbar, &e);
            return;
        }
    }

    /* inertial scrolling */
    if (inertialScrolling_ && QApplication::wheelScrollLines() > 0
        && event->spontaneous()
        && !horizontal
        && event->source() == Qt::MouseEventNotSynthesized)
    {
        QScrollBar *vbar = verticalScrollBar();
        if (vbar && vbar->isVisible())
        {
            int delta = anglePoint.y();
            /* with mouse, always set the initial speed to 3 lines per wheel turn;
               with more sensitive devices, set it to one line */
            if (std::abs (delta) >= 120)
            {
                if (std::abs (delta * 3) >= QApplication::wheelScrollLines())
                    delta = delta * 3 / QApplication::wheelScrollLines();
            }
            else if (std::abs (delta) >= QApplication::wheelScrollLines())
                delta = delta / QApplication::wheelScrollLines();

            if((delta > 0 && vbar->value() == vbar->minimum())
               || (delta < 0 && vbar->value() == vbar->maximum()))
            {
                return; // the scrollbar can't move
            }
            /* find the number of wheel events in 500 ms
               and set the scroll frames per second accordingly */
            static QList<qint64> wheelEvents;
            wheelEvents << QDateTime::currentMSecsSinceEpoch();
            while (wheelEvents.last() - wheelEvents.first() > 500)
                wheelEvents.removeFirst();
            int steps = std::max (SCROLL_FRAMES_PER_SEC / static_cast<int>(wheelEvents.size()), 5)
                        * SCROLL_DURATION / 1000;

            /* wait until the angle delta reaches an acceptable value */
            static int _delta = 0;
            _delta += delta;
            if (std::abs (_delta) < steps)
                return;

            /* set the data for inertial scrolling */
            scrollData data;
            data.delta = _delta;
            _delta = 0;
            data.totalSteps = data.leftSteps = steps;
            queuedScrollSteps_.append (data);
            if (!scrollTimer_)
            {
                scrollTimer_ = new QTimer();
                scrollTimer_->setTimerType (Qt::PreciseTimer);
                connect (scrollTimer_, &QTimer::timeout, this, &TextEdit::scrollWithInertia);
            }
            if (!scrollTimer_->isActive())
                scrollTimer_->start (1000 / SCROLL_FRAMES_PER_SEC);
            return;
        }
    }

    /* proceed as in QPlainTextEdit::wheelEvent() */
    QAbstractScrollArea::wheelEvent (event);
    updateMicroFocus();
}
/*************************/
void TextEdit::scrollWithInertia()
{
    QScrollBar *vbar = verticalScrollBar();
    if (!vbar || !vbar->isVisible())
    {
        queuedScrollSteps_.clear();
        scrollTimer_->stop();
        return;
    }

    int totalDelta = 0;
    for (QList<scrollData>::iterator it = queuedScrollSteps_.begin(); it != queuedScrollSteps_.end(); ++it)
    {
        totalDelta += std::round (static_cast<double>(it->delta) / it->totalSteps);
        -- it->leftSteps;
    }
    /* only remove the first queue to simulate an inertia */
    while (!queuedScrollSteps_.empty())
    {
        int t = queuedScrollSteps_.begin()->totalSteps; // 18 for one wheel turn
        int l = queuedScrollSteps_.begin()->leftSteps;
        if ((t > 10 && l <= 0)
            || (t > 5 && t <= 10 && l <= -3)
            || (t <= 5 && l <= -6))
        {
            queuedScrollSteps_.removeFirst();
        }
        else break;
    }

    if (totalDelta != 0)
    {
        QWheelEvent e (QPointF(),
                       QPointF(),
                       QPoint(),
                       QPoint (0, totalDelta),
                       Qt::NoButton,
                       Qt::NoModifier,
                       Qt::NoScrollPhase,
                       false);
        QCoreApplication::sendEvent (vbar, &e);
    }

    if (queuedScrollSteps_.empty())
        scrollTimer_->stop();
}
/*************************/
void TextEdit::resizeEvent (QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent (event);

    QRect cr = contentsRect();
    int lw = lineNumberAreaWidth();
    lineNumberArea_->setGeometry (QRect (QApplication::layoutDirection() == Qt::RightToLeft
                                             ? cr.width() - lw : cr.left(),
                                         cr.top(),
                                         lw, cr.height()));

    if (lineWrapMode() != QPlainTextEdit::NoWrap)
        removeColumnHighlight();

    if (resizeTimerId_)
    {
        killTimer (resizeTimerId_);
        resizeTimerId_ = 0;
    }
    resizeTimerId_ = startTimer (UPDATE_INTERVAL);
}
/*************************/
void TextEdit::timerEvent (QTimerEvent *event)
{
    QPlainTextEdit::timerEvent (event);

    if (event->timerId() == resizeTimerId_)
    {
        killTimer (event->timerId());
        resizeTimerId_ = 0;
        emit resized();
    }
    else if (event->timerId() == selectionTimerId_)
    {
        killTimer (event->timerId());
        selectionTimerId_ = 0;
        selectionHlight();
        emit selChanged();
    }
}
/**********************
***** Paint event *****
***********************/
static void fillBackground (QPainter *p, const QRectF &rect, QBrush brush, const QRectF &gradientRect = QRectF())
{
    p->save();
    if (brush.style() >= Qt::LinearGradientPattern && brush.style() <= Qt::ConicalGradientPattern)
    {
        if (!gradientRect.isNull())
        {
            QTransform m = QTransform::fromTranslate (gradientRect.left(), gradientRect.top());
            m.scale (gradientRect.width(), gradientRect.height());
            brush.setTransform (m);
            const_cast<QGradient *>(brush.gradient())->setCoordinateMode (QGradient::LogicalMode);
        }
    }
    else
        p->setBrushOrigin (rect.topLeft());
    p->fillRect (rect, brush);
    p->restore();
}

// Changes are made to QPlainTextEdit::paintEvent() for setting the
// RTL layout text option, drawing vertical indentation lines, etc.
void TextEdit::paintEvent (QPaintEvent *event)
{
    QPainter painter (viewport());
    Q_ASSERT (qobject_cast<QPlainTextDocumentLayout*>(document()->documentLayout()));

    QPointF offset (contentOffset());

    QRect er = event->rect();
    QRect viewportRect = viewport()->rect();
    double maximumWidth = document()->documentLayout()->documentSize().width();
    painter.setBrushOrigin (offset);

    int maxX = static_cast<int>(offset.x() + std::max (static_cast<double>(viewportRect.width()), maximumWidth)
                                - document()->documentMargin());
    er.setRight (std::min (er.right(), maxX));
    painter.setClipRect (er);

    bool editable = !isReadOnly();
    QAbstractTextDocumentLayout::PaintContext context = getPaintContext();
    QTextBlock block = firstVisibleBlock();
    while (block.isValid())
    {
        QRectF r = blockBoundingRect (block).translated (offset);
        QTextLayout *layout = block.layout();

        if (!block.isVisible())
        {
            offset.ry() += r.height();
            block = block.next();
            continue;
        }

        if (r.bottom() >= er.top() && r.top() <= er.bottom())
        {
            /* take care of RTL (workaround for the RTL bug in QPlainTextEdit) */
            bool rtl (block.textDirection() == Qt::RightToLeft);
            QTextOption opt = document()->defaultTextOption();
            if (rtl)
            {
                if (lineWrapMode() == QPlainTextEdit::WidgetWidth)
                    opt.setAlignment (Qt::AlignRight); // doesn't work without wrapping
                opt.setTextDirection (Qt::RightToLeft);
                layout->setTextOption (opt);
            }

            QTextBlockFormat blockFormat = block.blockFormat();
            QBrush bg = blockFormat.background();
            if (bg != Qt::NoBrush)
            {
                QRectF contentsRect = r;
                contentsRect.setWidth (std::max (r.width(), maximumWidth));
                fillBackground (&painter, contentsRect, bg);
            }

            if (lineNumberArea_->isVisible() && (opt.flags() & QTextOption::ShowLineAndParagraphSeparators))
            {
                /* "QTextFormat::FullWidthSelection" isn't respected when new-lines are shown.
                   This is a workaround. */
                QRectF contentsRect = r;
                contentsRect.setWidth (std::max (r.width(), maximumWidth));
                if (contentsRect.contains (cursorRect().center()))
                {
                    contentsRect.setTop (cursorRect().top());
                    contentsRect.setBottom (cursorRect().bottom());
                    fillBackground (&painter, contentsRect, lineHColor_);
                }
            }

            QList<QTextLayout::FormatRange> selections;
            int blpos = block.position();
            int bllen = block.length();
            for (int i = 0; i < context.selections.size(); ++i)
            {
                const QAbstractTextDocumentLayout::Selection &range = context.selections.at (i);
                const int selStart = range.cursor.selectionStart() - blpos;
                const int selEnd = range.cursor.selectionEnd() - blpos;
                if (selStart < bllen && selEnd > 0
                    && selEnd > selStart)
                {
                    QTextLayout::FormatRange o;
                    o.start = selStart;
                    o.length = selEnd - selStart;
                    o.format = range.format;
                    selections.append (o);
                }
                else if (!range.cursor.hasSelection() && range.format.hasProperty (QTextFormat::FullWidthSelection)
                         && block.contains(range.cursor.position()))
                {
                    QTextLayout::FormatRange o;
                    QTextLine l = layout->lineForTextPosition (range.cursor.position() - blpos);
                    o.start = l.textStart();
                    o.length = l.textLength();
                    if (o.start + o.length == bllen - 1)
                        ++o.length; // include newline
                    o.format = range.format;
                    selections.append (o);
                }
            }

            bool drawCursor ((editable || (textInteractionFlags() & Qt::TextSelectableByKeyboard))
                             && context.cursorPosition >= blpos
                             && context.cursorPosition < blpos + bllen);
            bool drawCursorAsBlock (drawCursor && overwriteMode());

            if (drawCursorAsBlock)
            {
                if (context.cursorPosition == blpos + bllen - 1)
                    drawCursorAsBlock = false;
                else
                {
                    QTextLayout::FormatRange o;
                    o.start = context.cursorPosition - blpos;
                    o.length = 1;
                    if (darkValue_ > -1)
                    {
                        o.format.setForeground (Qt::black);
                        o.format.setBackground (Qt::white);
                    }
                    else
                    {
                        o.format.setForeground (Qt::white);
                        o.format.setBackground (Qt::black);
                    }
                    selections.append (o);
                }
            }

            if (!placeholderText().isEmpty() && document()->isEmpty())
            {
                painter.save();
                QColor col = palette().text().color();
                col.setAlpha (128);
                painter.setPen (col);
                const int margin = int(document()->documentMargin());
                painter.drawText (r.adjusted (margin, 0, 0, 0), Qt::AlignTop | Qt::TextWordWrap, placeholderText());
                painter.restore();
            }
            else
            {
                if (opt.flags() & QTextOption::ShowLineAndParagraphSeparators)
                {
                    painter.save();
                    painter.setPen (separatorColor_);
                }
                layout->draw (&painter, offset, selections, er);
                if (opt.flags() & QTextOption::ShowLineAndParagraphSeparators)
                    painter.restore();
            }

            if ((drawCursor && !drawCursorAsBlock)
                || (editable && context.cursorPosition < -1
                    && !layout->preeditAreaText().isEmpty()))
            {
                int cpos = context.cursorPosition;
                if (cpos < -1)
                    cpos = layout->preeditAreaPosition() - (cpos + 2);
                else
                    cpos -= blpos;
                layout->drawCursor (&painter, offset, cpos, cursorWidth());
            }

            /* indentation and position lines should be drawn after selections */
            if (drawIndetLines_)
            {
                QRegularExpressionMatch match;
                if (block.text().indexOf (QRegularExpression ("\\s+"), 0, &match) == 0)
                {
                    painter.save();
                    painter.setOpacity (0.18);
                    QTextCursor cur = textCursor();
                    cur.setPosition (match.capturedLength() + block.position());
                    QFontMetricsF fm = QFontMetricsF (document()->defaultFont());
                    int yTop = std::round (r.topLeft().y());
                    int yBottom =  std::round (r.height() >= static_cast<double>(2) * fm.lineSpacing()
                                               ? yTop + fm.height()
                                               : r.bottomLeft().y() - static_cast<double>(1));
                    double tabWidth = fm.horizontalAdvance (textTab_);
                    if (rtl)
                    {
                        double leftMost = cursorRect (cur).left();
                        double x = r.topRight().x();
                        x -= tabWidth;
                        while (x >= leftMost)
                        {
                            painter.drawLine (QLine (std::round (x), yTop, std::round (x), yBottom));
                            x -= tabWidth;
                        }
                    }
                    else
                    {
                        double rightMost = cursorRect (cur).right();
                        double x = r.topLeft().x();
                        x += tabWidth;
                        while (x <= rightMost)
                        {
                            painter.drawLine (QLine (std::round (x), yTop, std::round (x), yBottom));
                            x += tabWidth;
                        }
                    }
                    painter.restore();
                }
            }
            if (vLineDistance_ >= 10 && !rtl
                && QFontInfo (document()->defaultFont()).fixedPitch())
            {
                painter.save();
                QColor col;
                if (darkValue_ > -1)
                {
                    col = QColor (65, 154, 255);
                    col.setAlpha (90);
                }
                else
                {
                    col = Qt::blue;
                    col.setAlpha (70);
                }
                painter.setPen (col);
                QTextCursor cur = textCursor();
                cur.setPosition (block.position());
                QFontMetricsF fm = QFontMetricsF (document()->defaultFont());
                double rulerSpace = fm.horizontalAdvance (' ') * static_cast<double>(vLineDistance_);
                int yTop = std::round (r.topLeft().y());
                int yBottom =  std::round (r.height() >= static_cast<double>(2) * fm.lineSpacing()
                                       ? yTop + fm.height()
                                       : r.bottomLeft().y() - static_cast<double>(1));
                double rightMost = er.right();
                double x = static_cast<double>(cursorRect (cur).right());
                x += rulerSpace;
                while (x <= rightMost)
                {
                    painter.drawLine (QLine (std::round (x), yTop, std::round (x), yBottom));
                    x += rulerSpace;
                }
                painter.restore();
            }
        }

        offset.ry() += r.height();
        if (offset.y() > viewportRect.height())
            break;
        block = block.next();
    }

    if (backgroundVisible() && !block.isValid() && offset.y() <= er.bottom()
        && (centerOnScroll() || verticalScrollBar()->maximum() == verticalScrollBar()->minimum()))
    {
        painter.fillRect (QRect (QPoint (static_cast<int>(er.left()), static_cast<int>(offset.y())), er.bottomRight()), palette().window());
    }
}
/*********************************
***** End of the paint event *****
**********************************/

void TextEdit::highlightCurrentLine()
{
    /* keep yellow, green and blue highlights
       (related to searching, replacing and selecting) */
    QList<QTextEdit::ExtraSelection> es = extraSelections();
    if (!es.isEmpty() && !currentLine_.cursor.isNull())
        es.removeFirst(); // line highlight always comes first when it exists

    currentLine_.format.setBackground (document()->defaultTextOption().flags() & QTextOption::ShowLineAndParagraphSeparators
                                       ? Qt::transparent // workaround for a Qt bug (see TextEdit::paintEvent)
                                       : lineHColor_);
    currentLine_.format.setProperty (QTextFormat::FullWidthSelection, true);
    currentLine_.cursor = textCursor();
    currentLine_.cursor.clearSelection();
    es.prepend (currentLine_);

    setExtraSelections (es);
}
/*************************/
void TextEdit::lineNumberAreaPaintEvent (QPaintEvent *event)
{
    QPainter painter (lineNumberArea_);
    QColor currentBlockFg, currentLineBg, currentLineFg;
    if (darkValue_ > -1)
    {
        painter.fillRect (event->rect(), QColor (200, 200 , 200));
        painter.setPen (Qt::black);
        currentBlockFg = QColor (150, 0 , 0);
        currentLineBg = QColor (140, 0 , 0);
        currentLineFg = Qt::white;
    }
    else
    {
        painter.fillRect (event->rect(), QColor (40, 40 , 40));
        painter.setPen (Qt::white);
        currentBlockFg = QColor (255, 205 , 0);
        currentLineBg = QColor (255, 235 , 130);
        currentLineFg = Qt::black;
    }

    bool rtl (QApplication::layoutDirection() == Qt::RightToLeft);
    int w = lineNumberArea_->width();
    int left = rtl ? 3 : 0;

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry (block).translated (contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect (block).height());
    int curBlock = textCursor().blockNumber();
    int h = fontMetrics().height();
    QFont bf = font();
    bf.setBold (true);
    QLocale l = locale();
    l.setNumberOptions (QLocale::OmitGroupSeparator);

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            QString number = l.toString (blockNumber + 1);
            if (blockNumber == curBlock)
            {
                lastCurrentLine_ = QRect (0, top, 1, top + h);

                painter.save();
                painter.setFont (bf);
                painter.setPen (currentLineFg);
                QTextCursor tmp = textCursor();
                tmp.movePosition (QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
                if (tmp.atBlockStart())
                    painter.fillRect (0, top, w, h, currentLineBg);
                else
                {
                    int cur = cursorRect().center().y();
                    painter.fillRect (0, cur - h / 2, w, h, currentLineBg);
                    painter.drawText (left, cur - h / 2, w - 3, h,
                                      Qt::AlignRight, rtl ? "↲" : "↳");
                    painter.setPen (currentBlockFg);
                    if (tmp.movePosition (QTextCursor::Up, QTextCursor::MoveAnchor)) // always true
                    {
                        tmp.movePosition (QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
                        if (!tmp.atBlockStart())
                        {
                            cur = cursorRect (tmp).center().y();
                            painter.drawText (left, cur - h / 2, w - 3, h,
                                              Qt::AlignRight, number);
                        }
                    }
                }
            }
            painter.drawText (left, top, w - 3, h,
                              Qt::AlignRight, number);
            if (blockNumber == curBlock)
                painter.restore();
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect (block).height());
        ++blockNumber;
    }
}
/*************************/
// This calls the private function _q_adjustScrollbars()
// by calling QPlainTextEdit::resizeEvent().
void TextEdit::adjustScrollbars()
{
    QSize vSize = viewport()->size();
    QResizeEvent *_resizeEvent = new QResizeEvent (vSize, vSize);
    QCoreApplication::postEvent (viewport(), _resizeEvent);
}
/*************************/
void TextEdit::onUpdateRequesting (const QRect& /*rect*/, int dy)
{
    /* here, we're interested only in the vertical text scrolling
       (and, definitely, not in the blinking cursor updates) */
    if (dy == 0) return;
    /* we ignore the rectangle because QPlainTextEdit::updateRequest
       gives the whole rectangle when the text is scrolled */
    emit updateRect();
    /* because brackets may have been invisible before,
       FPwin::matchBrackets() should be called here */
    if (!matchedBrackets_ && isVisible())
        emit updateBracketMatching();
}
/*************************/
void TextEdit::onSelectionChanged()
{
    /* Bracket matching isn't only based on the signal "cursorPositionChanged()"
       because it isn't emitted when a selected text is removed while the cursor
       is at its start. So, an appropriate signal should be emitted in such cases. */
    QTextCursor cur = textCursor();
    if (!cur.hasSelection())
    {
        if (cur.position() == prevPos_ && cur.position() < prevAnchor_)
            emit updateBracketMatching();
        prevAnchor_ = prevPos_ = -1;
    }
    else
    {
        prevAnchor_ = cur.anchor();
        prevPos_ = cur.position();
    }

    if (selectionTimerId_)
    {
        killTimer (selectionTimerId_);
        selectionTimerId_ = 0;
    }
    selectionTimerId_ = startTimer (UPDATE_INTERVAL);

    /* selection highlighting */
    if (!selectionHighlighting_) return;
    if (highlightThisSelection_)
        removeSelectionHighlights_ = false; // reset
    else
    {
        removeSelectionHighlights_ = true;
        highlightThisSelection_ = true; // reset
    }
}
/*************************/
void TextEdit::zooming (float range)
{
    /* forget the horizontal position of the text cursor */
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;

    QFont f = document()->defaultFont();
    if (range == 0.f) // means unzooming
    {
        setEditorFont (font_, false);
        if (font_.pointSizeF() < f.pointSizeF())
            emit zoomedOut (this); // see the explanation below
    }
    else
    {
        const float newSize = static_cast<float>(f.pointSizeF()) + range;
        if (newSize <= 0) return;
        f.setPointSizeF (static_cast<double>(newSize));
        setEditorFont (f, false);

        /* if this is a zoom-out, the text will need
           to be formatted and/or highlighted again */
        if (range < 0) emit zoomedOut (this);
    }

    /* due to a Qt bug, this is needed for the
       scrollbar range to be updated correctly */
    adjustScrollbars();
}
/*************************/
// If the text page is first shown for a very short time (when, for example,
// the active tab is changed quickly several times), "updateRect()" might
// be emitted when the text page isn't visible, while "updateRequest()"
// might not be emitted when it becomes visible again. That will result
// in an incomplete syntax highlighting and, probably, bracket matching.
void TextEdit::showEvent (QShowEvent *event)
{
    QPlainTextEdit::showEvent (event);
    emit updateRect();
    if (!matchedBrackets_)
        emit updateBracketMatching();
}
/*************************/
void TextEdit::sortLines (bool reverse)
{
    if (isReadOnly()) return;
    QTextCursor cursor = textCursor();
    if (!cursor.selectedText().contains (QChar (QChar::ParagraphSeparator)))
        return;

    int anch = cursor.anchor();
    int pos = cursor.position();
    cursor.beginEditBlock();
    cursor.setPosition (std::min (anch, pos));
    cursor.movePosition (QTextCursor::StartOfBlock);
    cursor.setPosition (std::max (anch, pos), QTextCursor::KeepAnchor);
    cursor.movePosition (QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    QStringList lines = cursor.selectedText().split (QChar (QChar::ParagraphSeparator));
    /* QStringList::sort() is not aware of the locale */
    std::sort (lines.begin(), lines.end(), [](const QString &a, const QString &b) {
        return QString::localeAwareCompare (a, b) < 0;
    });
    int n = lines.size();
    if (reverse)
    {
        for (int i = 0; i < n; ++i)
        {
            cursor.insertText (lines.at (n - 1 - i));
            if (i < n - 1)
                cursor.insertBlock();
        }
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            cursor.insertText (lines.at (i));
            if (i < n - 1)
                cursor.insertBlock();
        }
    }
    cursor.endEditBlock();
}

/************************************************************
***** The following functions are mainly for hyperlinks *****
*************************************************************/

QString TextEdit::getUrl (const int pos) const
{
    static const QRegularExpression urlPattern ("[A-Za-z0-9_\\-]+://((?!&quot;|&gt;|&lt;)[A-Za-z0-9_.+/\\?\\=~&%#,;!@\\*\'\\-:\\(\\)\\[\\]])+(?<!\\.|\\?|!|:|;|,|\\(|\\)|\\[|\\]|\')|([A-Za-z0-9_.\\-]+@[A-Za-z0-9_\\-]+\\.[A-Za-z0-9.]+)(?<!\\.)");

    QString url;
    QTextBlock block = document()->findBlock (pos);
    QString text = block.text();
    if (text.length() <= 30000) // otherwise, too long
    {
        int cursorIndex = pos - block.position();
        QRegularExpressionMatch match;
        int indx = text.lastIndexOf (urlPattern, cursorIndex, &match);
        if (indx > -1 && indx + match.capturedLength() > cursorIndex)
        {
            url = match.captured();
            if (!match.captured (2).isEmpty()) // handle email
                url = "mailto:" + url;
        }
    }
    return url;
}
/*************************/
void TextEdit::highlightColumn (const QTextCursor &endCur, int gap)
{
    /* just a precaution */
    bool selectionHighlightingOrig = selectionHighlighting_;
    selectionHighlighting_ = false;

    QTextCursor cur = textCursor();
    if (cur.hasSelection())
    {
        cur.setPosition (cur.position());
        setTextCursor (cur);
    }

    int startIndent = cur.columnNumber();
    int endIndent = endCur.columnNumber() + gap;

    int hDistance = std::abs (endIndent - startIndent);
    int minIndent = std::min (startIndent, endIndent);

    QTextCursor tlCur; // top left cursor (with LTR)
    QTextCursor limitCur; // the cursor that sets the loop limit
    if (cur < endCur)
    {
        tlCur = cur;
        limitCur = endCur;
        if (startIndent > endIndent)
            tlCur.setPosition (tlCur.position() - (startIndent - endIndent));
    }
    else
    {
        tlCur = endCur;
        limitCur = cur;
        if (endIndent > startIndent)
            tlCur.setPosition (tlCur.position() - std::max (tlCur.columnNumber() - startIndent, 0));
    }
    int colNum = limitCur.columnNumber();
    if (limitCur.movePosition (QTextCursor::EndOfLine))
    {
        /* This and similar checks in the main loop below are for wrapped lines,
           although column selection is not useful when lines are wrapped. */
        if (limitCur.columnNumber() <= colNum)
            limitCur.movePosition (QTextCursor::PreviousCharacter);
    }

    QList<QTextEdit::ExtraSelection> es = extraSelections();
    int n = colSel_.count() + redSel_.count();
    while (n > 0 && !es.isEmpty())
    {
        es.removeLast();
        --n;
    }
    colSel_.clear();

    QTextEdit::ExtraSelection extra;
    extra.format.setBackground (palette().highlight().color());
    extra.format.setForeground (palette().highlightedText().color());

    bool empty (true);
    int i = 0;
    QTextCursor tmp;
    while (tlCur <= limitCur)
    {
        ++i;
        if (i > 1000)
        {
            emit hugeColumn();
            break;
        }

        cur.setPosition (tlCur.position());
        tmp = cur;
        if (tmp.movePosition (QTextCursor::EndOfLine))
        {
            if (tmp.columnNumber() <= cur.columnNumber() && tmp.position() == cur.position() + 1)
                tmp.movePosition (QTextCursor::PreviousCharacter);
        }
        cur.setPosition (std::min (cur.position() + hDistance, tmp.position()), QTextCursor::KeepAnchor);
        if (empty && cur.hasSelection())
            empty = false;

        extra.cursor = cur;
        colSel_.append (extra);

        /* WARNING: QTextCursor::movePosition(QTextCursor::Down) can be a mess with RTL. */
        //tlCur.movePosition (QTextCursor::StartOfLine);
        //if (!tlCur.movePosition (QTextCursor::Down))
        //    break;
        colNum = tlCur.columnNumber();
        if (tlCur.movePosition (QTextCursor::EndOfLine))
        {
            if (tlCur.columnNumber() > colNum)
            {
                if (!tlCur.movePosition (QTextCursor::NextCharacter))
                    break;
            }
        }
        else if (!tlCur.movePosition (QTextCursor::NextCharacter))
            break;
        tmp = tlCur;
        if (tmp.movePosition (QTextCursor::EndOfLine))
        {
            if (tmp.columnNumber() <= tlCur.columnNumber())
                tmp.movePosition (QTextCursor::PreviousCharacter);
        }
        tlCur.setPosition (std::min (tlCur.position() + minIndent, tmp.position()));
    }

    if (empty) // no row has text
        colSel_.clear();
    else
        es.append (colSel_);

    es.append (redSel_);
    setExtraSelections (es);

    selectionHighlighting_ = selectionHighlightingOrig;

    if (!colSel_.isEmpty()) emit canCopy (true);

    if (selectionTimerId_)
    {
        killTimer (selectionTimerId_);
        selectionTimerId_ = 0;
    }
    selectionTimerId_ = startTimer (UPDATE_INTERVAL);
}
/*************************/
void TextEdit::makeColumn (const QPoint &endPoint)
{
    /* limit the position to the viewport */
    QPoint p (std::clamp (endPoint.x(), 0, viewport()->width()),
              std::clamp (endPoint.y(), 0, viewport()->height()));
    QTextCursor endCur = cursorForPosition (p);
    QRect cRect (cursorRect (endCur));
    bool rtl (endCur.block().textDirection() == Qt::RightToLeft);
    if (rtl)
    { // a workaround for an RTL problem of Qt
        if (p.y() <= cRect.top())
        {
            QTextCursor tmp = endCur;
            if (tmp.movePosition (QTextCursor::StartOfLine)
                && tmp.movePosition (QTextCursor::PreviousCharacter)
                && tmp.blockNumber() == endCur.blockNumber())
            {
                endCur = tmp;
                cRect = cursorRect (endCur);
            }
        }
        else if (p.y() > cRect.bottom())
        {
            QTextCursor tmp = endCur;
            if (tmp.movePosition (QTextCursor::NextCharacter)
                && tmp.blockNumber() == endCur.blockNumber())
            {
                endCur = tmp;
                cRect = cursorRect (endCur);
            }
        }
    }
    int extraGap = 0;
    if (p.y() <= cRect.top())
    {
        if (rtl ? p.x() <= cRect.right() + 1 : p.x() >= cRect.left())
        { // if the cursor is in the next wrapped line, move it up
            QTextCursor tmp = endCur;
            if (!tmp.movePosition (QTextCursor::StartOfLine)
                && tmp.movePosition (QTextCursor::PreviousCharacter)
                && tmp.blockNumber() == endCur.blockNumber())
            {
                QRect tRect (cursorRect (tmp));
                if (p.y() >= tRect.top())
                { // the point is aligned with this wrapped line (the equality is needed with RTL)
                    endCur = tmp;
                    cRect = tRect;
                    extraGap = static_cast<int>(std::abs (p.x() - cRect.center().x())
                                                / QFontMetricsF (document()->defaultFont())
                                                  .horizontalAdvance (" "));
                    p = cRect.center();
                }
            }
        }
    }
    else if (p.y() > cRect.bottom())
    {
        /* do not stick to the end of the line when there is no text after it
           and the cursor is below it */
        p.setY (std::max (0, cRect.center().y()));
        endCur = cursorForPosition (p);
        cRect = cursorRect (endCur);
        if (rtl)
        {
            if (p.y() <= cRect.top())
            {
                QTextCursor tmp = endCur;
                if (tmp.movePosition (QTextCursor::PreviousCharacter)
                    && tmp.blockNumber() == endCur.blockNumber())
                {
                    endCur = tmp;
                    cRect = cursorRect (endCur);
                }
            }
            else if (p.y() > cRect.bottom())
            {
                QTextCursor tmp = endCur;
                if (tmp.movePosition (QTextCursor::NextCharacter)
                    && tmp.blockNumber() == endCur.blockNumber())
                {
                    endCur = tmp;
                    cRect = cursorRect (endCur);
                }
            }
        }
    }
    /* also, consider the top and left document margins by using the cursor rectangle */
    QPoint c (cRect.center());
    if (rtl)
        p.setX (std::min (p.x(), std::min (c.x(), viewport()->width())));
    else
        p.setX (std::max (std::max (0, c.x()), p.x()));
    p.setY (std::max (std::max (0, c.y()), p.y()));
    endCur = cursorForPosition (p);
    if (rtl)
    {
        cRect = cursorRect (endCur);
        if (p.y() <= cRect.top())
        {
            QTextCursor tmp = endCur;
            if (tmp.movePosition (QTextCursor::PreviousCharacter)
                && tmp.blockNumber() == endCur.blockNumber())
            {
                endCur = tmp;
            }
        }
        else if (p.y() > cRect.bottom())
        {
            QTextCursor tmp = endCur;
            if (tmp.movePosition (QTextCursor::NextCharacter)
                && tmp.blockNumber() == endCur.blockNumber())
            {
                endCur = tmp;
            }
        }
    }

    highlightColumn (endCur,
                     // the gap between the actual position and the cursor
                     static_cast<int>(std::abs (p.x() - cursorRect (endCur).center().x())
                                      / QFontMetricsF (document()->defaultFont())
                                        .horizontalAdvance (" ")) + extraGap);
}
/*************************/
void TextEdit::mouseMoveEvent (QMouseEvent *event)
{
    if (event->buttons() == Qt::LeftButton
        && event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier))
    { // column highlighting
        makeColumn (event->position().toPoint());
        event->accept();
        return;
    }

    QPlainTextEdit::mouseMoveEvent (event);

    if (!highlighter_) return;
    if (event->modifiers() != Qt::ControlModifier)
    {
        if (viewport()->cursor().shape() != Qt::IBeamCursor)
            viewport()->setCursor (Qt::IBeamCursor);
        return;
    }

    if (getUrl (cursorForPosition (event->position().toPoint()).position()).isEmpty())
    {
        if (viewport()->cursor().shape() != Qt::IBeamCursor)
            viewport()->setCursor (Qt::IBeamCursor);
    }
    else if (viewport()->cursor().shape() != Qt::PointingHandCursor)
        viewport()->setCursor (Qt::PointingHandCursor);
}
/*************************/
void TextEdit::mousePressEvent (QMouseEvent *event)
{
    /* forget the last cursor position */
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;
    mousePressed_ = true;

    /* With a triple click, QPlainTextEdit selects the current block
       plus its newline, if any. But it is better to select the
       current block without selecting its newline and start and end
       whitespaces (because, for example, the selection clipboard might
       be pasted into a terminal emulator). */
    if (tripleClickTimer_.isValid())
    {
        if (!tripleClickTimer_.hasExpired (qApp->doubleClickInterval())
            && event->buttons() == Qt::LeftButton)
        {
            tripleClickTimer_.invalidate();
            if (event->modifiers() != Qt::ControlModifier)
            {
                QTextCursor txtCur = textCursor();
                const QString txt = txtCur.block().text();
                const int l = txt.length();
                txtCur.movePosition (QTextCursor::StartOfBlock);
                int i = 0;
                while (i < l && txt.at (i).isSpace())
                    ++i;
                /* WARNING: QTextCursor::movePosition() can be a mess with RTL
                            but QTextCursor::setPosition() works fine. */
                if (i < l)
                {
                    txtCur.setPosition (txtCur.position() + i);
                    int j = l;
                    while (j > i && txt.at (j -  1).isSpace())
                        --j;
                    txtCur.setPosition (txtCur.position() + j - i, QTextCursor::KeepAnchor);
                    setTextCursor (txtCur);
                }
                if (txtCur.hasSelection())
                {
                    QClipboard *cl = QApplication::clipboard();
                    if (cl->supportsSelection())
                        cl->setText (txtCur.selection().toPlainText(), QClipboard::Selection);
                }
                event->accept();
                return;
            }
        }
        else
            tripleClickTimer_.invalidate();
    }

    /* remove the column highlight if this is not a right click
       (also, see "QPlainTextEdit::cursorPositionChanged" in c-tor) */
    if (!colSel_.isEmpty() && event->button() != Qt::RightButton)
        removeColumnHighlight();

    if (event->button() == Qt::LeftButton)
    {
        if (event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier))
        { // column highlighting
            makeColumn (event->position().toPoint());
            event->accept();
            return;
        }
        pressPoint_ = event->position().toPoint();
    }

    QPlainTextEdit::mousePressEvent (event);
}
/*************************/
void TextEdit::mouseReleaseEvent (QMouseEvent *event)
{
    QPlainTextEdit::mouseReleaseEvent (event);
    mousePressed_ = false;

    if (event->button() != Qt::LeftButton || !highlighter_)
        return;
    if (event->modifiers() != Qt::ControlModifier)
    {
        if (viewport()->cursor().shape() == Qt::PointingHandCursor)
        {
            /* this can happen if the window or viewport was inactive when
               the left mouse button was pressed but Ctrl was released before it */
            viewport()->setCursor (Qt::IBeamCursor);
        }
        return;
    }
    if (viewport()->cursor().shape() != Qt::PointingHandCursor)
        return; // another key may also be pressed besides Ctrl (-> keyPressEvent)

    QTextCursor cur = cursorForPosition (event->position().toPoint());
    if (cur == cursorForPosition (pressPoint_))
    {
        QString str = getUrl (cur.position());
        if (!str.isEmpty())
        {
            QUrl url (str);
            if (url.isRelative()) // treat relative URLs as local paths (not needed here)
                url = QUrl::fromUserInput (str, "/");
            if (QStandardPaths::findExecutable ("gio").isEmpty()
                || !QProcess::startDetached ("gio", QStringList() << "open" << url.toString()))
            {
                QDesktopServices::openUrl (url);
            }
        }
    }
    pressPoint_ = QPoint();
}
/*************************/
void TextEdit::mouseDoubleClickEvent (QMouseEvent *event)
{
    tripleClickTimer_.start();
    QPlainTextEdit::mouseDoubleClickEvent (event);

    /* Select the text between spaces with Ctrl.
       NOTE: QPlainTextEdit should process the event before this. */
    if (event->button() == Qt::LeftButton
        && event->modifiers() == Qt::ControlModifier)
    {
        QTextCursor txtCur = textCursor();
        const int blockPos = txtCur.block().position();
        const QString txt = txtCur.block().text();
        const int l = txt.length();
        int anc = txtCur.anchor();
        int pos = txtCur.position();
        while (anc > blockPos && !txt.at (anc - blockPos - 1).isSpace())
            -- anc;
        while (pos < blockPos + l && !txt.at (pos - blockPos).isSpace())
            ++ pos;
        if (anc != textCursor().anchor() || pos != textCursor().position())
        {
            txtCur.setPosition (anc);
            txtCur.setPosition (pos, QTextCursor::KeepAnchor);
            setTextCursor (txtCur);
            if (txtCur.hasSelection())
            {
                QClipboard *cl = QApplication::clipboard();
                if (cl->supportsSelection())
                    cl->setText (txtCur.selection().toPlainText(), QClipboard::Selection);
            }
            event->accept();
        }
    }
}
/*************************/
void TextEdit::removeColumnHighlight()
{
    int n = colSel_.count();
    if (n == 0) return;
    QList<QTextEdit::ExtraSelection> es = extraSelections();
    int nRed = redSel_.count();
    while (n > 0 && es.size() - nRed > 0)
    {
        es.removeAt (es.size() - 1 - nRed);
        --n;
    }
    colSel_.clear();
    setExtraSelections (es);
    if (!textCursor().hasSelection())
        emit canCopy (false);

    if (selectionTimerId_)
    {
        killTimer (selectionTimerId_);
        selectionTimerId_ = 0;
    }
    selectionTimerId_ = startTimer (UPDATE_INTERVAL);
}
/*************************/
int TextEdit::selectionSize() const
{
    int res = textCursor().selectedText().size();
    if (res > 0) return res;
    for (auto const &extra : std::as_const (colSel_))
    {
        res += extra.cursor.selectedText().size();
    }
    return res;
}
/*************************/
bool TextEdit::event (QEvent *event)
{
    if (highlighter_
        && ((event->type() == QEvent::WindowDeactivate && hasFocus()) // another window is activated
            || event->type() == QEvent::FocusOut) // another widget has been focused
        && viewport()->cursor().shape() != Qt::IBeamCursor)
    {
        viewport()->setCursor (Qt::IBeamCursor);
    }
    return QPlainTextEdit::event (event);
}

/************************************************************************
 ***** Qt's backward search has some bugs. Therefore, we do our own *****
 ***** backward search by using the following two static functions. *****
 ************************************************************************/
static bool findBackwardInBlock (const QTextBlock &block, const QString &str, int offset,
                                 QTextCursor &cursor, QTextDocument::FindFlags flags)
{
    Qt::CaseSensitivity cs = !(flags & QTextDocument::FindCaseSensitively)
                             ? Qt::CaseInsensitive : Qt::CaseSensitive;

    QString text = block.text();
    text.replace (QChar::Nbsp, QLatin1Char (' '));

    /* WARNING: QString::lastIndexOf() returns -1 if the position, from which the
                backward search is done, is the position of the block's last cursor.
                The following workaround compensates for this illogical behavior. */
    if (offset > 0 && offset == text.length())
        -- offset;

    int idx = -1;
    while (offset >= 0 && offset <= text.length())
    {
        idx = text.lastIndexOf (str, offset, cs);
        if (idx == -1)
            return false;
        if (flags & QTextDocument::FindWholeWords)
        {
            const int start = idx;
            const int end = start + str.length();
            if ((start != 0 && text.at (start - 1).isLetterOrNumber())
                || (end != text.length() && text.at (end).isLetterOrNumber()))
            { // if this is not a whole word, continue the backward search
                offset = idx - 1;
                idx = -1;
                continue;
            }
        }
        cursor.setPosition (block.position() + idx);
        cursor.setPosition (cursor.position() + str.length(), QTextCursor::KeepAnchor);
        return true;
    }
    return false;
}

static bool findBackward (const QTextDocument *txtdoc, const QString &str,
                          QTextCursor &cursor, QTextDocument::FindFlags flags)
{
    if (!str.isEmpty() && !cursor.isNull())
    {
        int pos = cursor.anchor()
                  - str.size(); // we don't want a match with the cursor inside it
        if (pos >= 0)
        {
            QTextBlock block = txtdoc->findBlock (pos);
            int blockOffset = pos - block.position();
            while (block.isValid())
            {
                if (findBackwardInBlock (block, str, blockOffset, cursor, flags))
                    return true;
                block = block.previous();
                blockOffset = block.length() - 1; // newline is included in QTextBlock::length()
            }
        }
    }
    cursor = QTextCursor();
    return false;
}

/************************************************************************
 ***** Qt's forward search goes to the end of the document but we  *****
 ***** may need to stop at some position. Therefore, we do our own *****
 ***** forward search by using the following two static functions. *****
 ************************************************************************/
static bool findForwardInBlock (const QTextBlock &block, const QString &str, int offset,
                                QTextCursor &cursor, QTextDocument::FindFlags flags)
{
    Qt::CaseSensitivity cs = !(flags & QTextDocument::FindCaseSensitively)
                             ? Qt::CaseInsensitive : Qt::CaseSensitive;

    QString text = block.text();
    text.replace (QChar::Nbsp, QLatin1Char (' '));
    int idx = -1;
    while (offset >= 0 && offset <= text.length())
    {
        idx = text.indexOf (str, offset, cs);
        if (idx == -1)
            return false;
        if (flags & QTextDocument::FindWholeWords)
        {
            const int start = idx;
            const int end = start + str.length();
            if ((start != 0 && text.at (start - 1).isLetterOrNumber())
                || (end != text.length() && text.at (end).isLetterOrNumber()))
            { // if this is not a whole word, continue the search
                offset = end + (str.length() == 0 // a zero-length string
                                || str.at (str.length() - 1).isLetterOrNumber() ? 1 : 0);
                idx = -1;
                continue;
            }
        }
        cursor.setPosition (block.position() + idx);
        cursor.setPosition (cursor.position() + str.length(), QTextCursor::KeepAnchor);
        return true;
    }
    return false;
}

static bool findForward (const QTextDocument *txtdoc, const QString &str,
                         QTextCursor &cursor, QTextDocument::FindFlags flags,
                         const int end)
{
    if (!str.isEmpty() && !cursor.isNull())
    {
        int pos = cursor.selectionEnd();
        QTextBlock block = txtdoc->findBlock (pos);
        int blockOffset = pos - block.position();
        while (block.isValid() && (end <= 0 || block.position() <= end))
        {
            if (findForwardInBlock (block, str, blockOffset, cursor, flags))
            {
                /* check the exact position */
                if (end > 0 && cursor.anchor() > end)
                {
                    cursor = QTextCursor();
                    return false;
                }
                return true;
            }
            block = block.next();
            blockOffset = 0;
        }
    }
    cursor = QTextCursor();
    return false;
}

/**********************************************************************
 ***** The following static functions are for searching regular   *****
 ***** expressions forward and backward. Each search should be    *****
 ***** done by two functions because, otherwise, lots of CPU time *****
 ***** and memory might be used with unusually large texts.       *****
 **********************************************************************/
static bool findRegexBackwardInBlock (const QTextBlock &block, const QRegularExpression &regex,
                                      int offset, QTextCursor &cursor, const int start)
{
    QString text = block.text();
    QRegularExpressionMatch match;
    while (offset >= 0 && offset <= text.length())
    {
        int idx = text.lastIndexOf (regex, offset, &match);
        if (idx == -1)
            return false;
        /* no empty match (e.g., with "\w*", or with
           ".*" and when the cursor is at the block start) */
        if (match.capturedLength() == 0
            /* also, the match start should be before the search start */
            || block.position() + idx == start)
        {
            -- offset;
            continue;
        }
        cursor.setPosition (block.position() + idx);
        cursor.setPosition (cursor.position() + match.capturedLength(), QTextCursor::KeepAnchor);
        return true;
    }
    return false;
}

static bool findRegexBackward (const QTextDocument *txtdoc, const QRegularExpression &regex,
                               QTextCursor &cursor)
{
    if (!cursor.isNull())
    {
        int pos = cursor.anchor();
        QTextBlock block = txtdoc->findBlock (pos);
        int blockOffset = pos - block.position();
        while (block.isValid())
        {
            /* with a backward search, the search start ("pos") should also be checked */
            if (findRegexBackwardInBlock (block, regex, blockOffset, cursor, pos))
                return true;
            block = block.previous();
            blockOffset = block.length() - 1; // newline is included in QTextBlock::length()
        }
    }
    cursor = QTextCursor();
    return false;
}
//--------------------
static bool findRegexForwardInBlock (const QTextBlock &block, const QRegularExpression &regex,
                                     int offset, QTextCursor &cursor)
{
    QString text = block.text();
    QRegularExpressionMatch match;
    while (offset >= 0 && offset <= text.length())
    {
        int idx = text.indexOf (regex, offset, &match);
        if (idx == -1)
            return false;
        if (match.capturedLength() == 0)
        {
            /* no empty match (e.g., with "\w*", or with
               ".*" and when the cursor is at the block end) */
            ++ offset;
            continue;
        }
        cursor.setPosition (block.position() + idx);
        cursor.setPosition (cursor.position() + match.capturedLength(), QTextCursor::KeepAnchor);
        return true;
    }
    return false;
}

static bool findRegexForward (const QTextDocument *txtdoc, const QRegularExpression &regex,
                              QTextCursor &cursor, const int end)
{
    if (!cursor.isNull())
    {
        int pos = cursor.selectionEnd(); // as with an ordinary search
        QTextBlock block = txtdoc->findBlock (pos);
        int blockOffset = pos - block.position();
        while (block.isValid() && (end <= 0 || block.position() <= end))
        {
            if (findRegexForwardInBlock (block, regex, blockOffset, cursor))
            {
                /* check the exact position */
                if (end > 0 && cursor.anchor() > end)
                {
                    cursor = QTextCursor();
                    return false;
                }
                return true;
            }
            block = block.next();
            blockOffset = 0;
        }
    }
    cursor = QTextCursor();
    return false;
}
/*************************/
// This method extends the searchable strings to those with line breaks.
// It also corrects the behavior of Qt's backward search and can set an
// end limit to the forward search.
QTextCursor TextEdit::finding (const QString& str, const QTextCursor& start, QTextDocument::FindFlags flags,
                               bool isRegex, const int end) const
{
    /* let's be consistent first */
    if (str.isEmpty())
        return QTextCursor(); // null cursor

    QTextCursor res = start;
    if (isRegex) // multiline matches aren't supported
    {
        QRegularExpression regexp (str, (flags & QTextDocument::FindCaseSensitively)
                                            ? QRegularExpression::NoPatternOption
                                            : QRegularExpression::CaseInsensitiveOption);
        if (!regexp.isValid())
            return QTextCursor();
        if (!(flags & QTextDocument::FindBackward))
            findRegexForward (document(), regexp, res, end);
        else
            findRegexBackward (document(), regexp, res);
    }
    else if (str.contains ('\n'))
    {
        QTextCursor cursor = start;
        QStringList sl = str.split ("\n");
        int i = 0;
        Qt::CaseSensitivity cs = !(flags & QTextDocument::FindCaseSensitively)
                                 ? Qt::CaseInsensitive : Qt::CaseSensitive;
        QString subStr;
        if (!(flags & QTextDocument::FindBackward))
        {
            /* this loop searches for the consecutive
               occurrences of newline separated strings */
            while (i < sl.count())
            {
                if (i == 0) // the first string
                {
                    subStr = sl.at (0);
                    /* when the first string is empty... */
                    if (subStr.isEmpty())
                    {
                        /* ... search anew from the next block */
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        if (end > 0 && cursor.anchor() > end)
                            return QTextCursor();
                        res.setPosition (cursor.position());
                        if (!cursor.movePosition (QTextCursor::NextBlock))
                            return QTextCursor();
                        ++i;
                    }
                    else
                    {
                        if (!findForward (document(), subStr, cursor, flags, end))
                            return QTextCursor();
                        int anc = cursor.anchor();
                        cursor.setPosition (cursor.position());
                        /* if the match doesn't end the block... */
                        while (!cursor.atBlockEnd())
                        {
                            /* ... move the cursor to right and search until a match is found */
                            cursor.movePosition (QTextCursor::EndOfBlock);
                            cursor.setPosition (cursor.position() - subStr.length());
                            if (!findForward (document(), subStr, cursor, flags, end))
                                return QTextCursor();
                            anc = cursor.anchor();
                            cursor.setPosition (cursor.position());
                        }

                        res.setPosition (anc);
                        if (!cursor.movePosition (QTextCursor::NextBlock))
                            return QTextCursor();
                        ++i;
                    }
                }
                else if (i != sl.count() - 1) // middle strings
                {
                    /* when the next block's test isn't the next string... */
                    if (QString::compare (cursor.block().text(), sl.at (i), cs) != 0)
                    {
                        /* ... reset the loop cautiously */
                        cursor.setPosition (res.position());
                        if (!cursor.movePosition (QTextCursor::NextBlock))
                            return QTextCursor();
                        i = 0;
                        continue;
                    }

                    if (!cursor.movePosition (QTextCursor::NextBlock))
                        return QTextCursor();
                    ++i;
                }
                else // the last string (i == sl.count() - 1)
                {
                    subStr = sl.at (i);
                    if (subStr.isEmpty()) break;
                    if (!(flags & QTextDocument::FindWholeWords))
                    {
                        /* when the last string doesn't start the next block... */
                        if (!cursor.block().text().startsWith (subStr, cs))
                        {
                            /* ... reset the loop cautiously */
                            cursor.setPosition (res.position());
                            if (!cursor.movePosition (QTextCursor::NextBlock))
                                return QTextCursor();
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (cursor.anchor() + subStr.size());
                        break;
                    }
                    else
                    {
                        if (!findForward (document(), subStr, cursor, flags, cursor.position()))
                        {
                            cursor = res;
                            cursor.setPosition (res.position());
                            if (!cursor.movePosition (QTextCursor::NextBlock))
                                return QTextCursor();
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (cursor.position());
                        break;
                    }
                }
            }
            res.setPosition (cursor.position(), QTextCursor::KeepAnchor);
        }
        else // backward search
        {
            cursor.setPosition (cursor.anchor());
            int endPos = cursor.position();
            QTextCursor found;
            while (i < sl.count())
            {
                if (i == 0) // the last string
                {
                    subStr = sl.at (sl.count() - 1);
                    if (subStr.isEmpty())
                    {
                        cursor.movePosition (QTextCursor::StartOfBlock);
                        endPos = cursor.position();
                        if (!cursor.movePosition (QTextCursor::PreviousBlock))
                            return QTextCursor();
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        ++i;
                    }
                    else
                    {
                        if (!findBackward (document(), subStr, cursor, flags))
                            return QTextCursor();
                        /* if the match doesn't start the block... */
                        while (cursor.anchor() > cursor.block().position())
                        {
                            /* ... move the cursor to left and search backward until a match is found */
                            cursor.setPosition (cursor.block().position() + subStr.size());
                            if (!findBackward (document(), subStr, cursor, flags))
                                return QTextCursor();
                        }

                        endPos = cursor.position();
                        if (!cursor.movePosition (QTextCursor::PreviousBlock))
                            return QTextCursor();
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        ++i;
                    }
                }
                else if (i != sl.count() - 1) // the middle strings
                {
                    if (QString::compare (cursor.block().text(), sl.at (sl.count() - i - 1), cs) != 0)
                    { // reset the loop if the block text doesn't match
                        cursor.setPosition (endPos);
                        if (!cursor.movePosition (QTextCursor::PreviousBlock))
                            return QTextCursor();
                        cursor.movePosition (QTextCursor::EndOfBlock);
                        i = 0;
                        continue;
                    }

                    if (!cursor.movePosition (QTextCursor::PreviousBlock))
                        return QTextCursor();
                    cursor.movePosition (QTextCursor::EndOfBlock);
                    ++i;
                }
                else // the first string
                {
                    subStr = sl.at (0);
                    if (subStr.isEmpty()) break;
                    if (!(flags & QTextDocument::FindWholeWords))
                    {
                        /* when the first string doesn't end the previous block... */
                        if (!cursor.block().text().endsWith (subStr, cs))
                        {
                            /* ... reset the loop */
                            cursor.setPosition (endPos);
                            if (!cursor.movePosition (QTextCursor::PreviousBlock))
                                return QTextCursor();
                            cursor.movePosition (QTextCursor::EndOfBlock);
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (cursor.anchor() - subStr.size());
                        break;
                    }
                    else
                    {
                        found = cursor; // block end
                        if (!findBackward (document(), subStr, found, flags)
                            || found.position() != cursor.position())
                        {
                            cursor.setPosition (endPos);
                            if (!cursor.movePosition (QTextCursor::PreviousBlock))
                                return QTextCursor();
                            cursor.movePosition (QTextCursor::EndOfBlock);
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (found.anchor());
                        break;
                    }
                }
            }
            res.setPosition (cursor.anchor());
            res.setPosition (endPos, QTextCursor::KeepAnchor);
        }
    }
    else // there's no line break
    {
        if (!(flags & QTextDocument::FindBackward))
            findForward (document(), str, res, flags, end);
        else
            findBackward (document(), str, res, flags);
    }

    return res;
}
/************************************
 ***** End of search functions. *****
 ************************************/

void TextEdit::setSelectionHighlighting (bool enable)
{
    selectionHighlighting_ = enable;
    highlightThisSelection_ = true; // reset
    removeSelectionHighlights_ = true; // start without highlighting if "enable" is true
    if (enable)
    {
        connect (document(), &QTextDocument::contentsChange, this, &TextEdit::onContentsChange);
        connect (this, &TextEdit::updateRect, this, &TextEdit::selectionHlight);
        connect (this, &TextEdit::resized, this, &TextEdit::selectionHlight);
    }
    else
    {
        disconnect (document(), &QTextDocument::contentsChange, this, &TextEdit::onContentsChange);
        disconnect (this, &TextEdit::updateRect, this, &TextEdit::selectionHlight);
        disconnect (this, &TextEdit::resized, this, &TextEdit::selectionHlight);
        /* remove all blue highlights */
        if (!blueSel_.isEmpty())
        {
            QList<QTextEdit::ExtraSelection> es = extraSelections();
            int nCol = colSel_.count();
            int nRed = redSel_.count();
            int n = blueSel_.count();
            while (n > 0 && es.size() - nCol - nRed > 0)
            {
                es.removeAt (es.size() - 1 - nCol - nRed);
                --n;
            }
            blueSel_.clear();
            setExtraSelections (es);
        }
    }
}
/*************************/
// Set the blue selection highlights (before the red bracket highlights).
void TextEdit::selectionHlight()
{
    if (!selectionHighlighting_) return;

    QList<QTextEdit::ExtraSelection> es = extraSelections();
    QTextCursor selCursor = textCursor();
    int minSel = std::min (selCursor.anchor(), selCursor.position());
    int maxSel = std::max (selCursor.anchor(), selCursor.position());
    int nCol = colSel_.count(); // column highlight (comes last but one)
    int nRed = redSel_.count(); // bracket highlights (come last)

    /* remove all blue highlights */
    int n = blueSel_.count();
    while (n > 0 && es.size() - nCol - nRed > 0)
    {
        es.removeAt (es.size() - 1 - nCol - nRed);
        --n;
    }

    if (removeSelectionHighlights_ || maxSel == minSel || maxSel - minSel > 100000)
    {
        /* avoid the computations of QWidgetTextControl::setExtraSelections
           as far as possible */
        if (!blueSel_.isEmpty())
        {
            blueSel_.clear();
            setExtraSelections (es);
        }
        return;
    }

    /* first put the start cursor at the top left corner... */
    QPoint Point (0, 0);
    QTextCursor start = cursorForPosition (Point);
    /* ... then move it backward by the selection length */
    int startPos = start.position() - maxSel + minSel;
    if (startPos >= 0)
        start.setPosition (startPos);
    else
        start.setPosition (0);
    /* also, move the start cursor outside the selection */
    if (start.position() >= minSel && start.position() < maxSel)
        start.setPosition (maxSel);

    /* put the end cursor at the bottom right corner... */
    Point = QPoint (geometry().width(), geometry().height());
    QTextCursor end = cursorForPosition (Point);
    int endLimit = end.anchor();
    /* ... and move it forward by the selection length */
    int endPos = end.position() + maxSel - minSel;
    end.movePosition (QTextCursor::End);
    if (endPos <= end.position())
        end.setPosition (endPos);
    /* also, move the end cursor outside the selection */
    if (end.position() > minSel && end.position() <= maxSel)
        end.setPosition (minSel);

    /* don't waste time if the selected text is larger that the available space */
    if (end.position() - start.position() < maxSel - minSel)
    {
        if (!blueSel_.isEmpty())
        {
            blueSel_.clear();
            setExtraSelections (es);
        }
        return;
    }

    blueSel_.clear();

    const QString selTxt = selCursor.selection().toPlainText();
    QTextDocument::FindFlags searchFlags = (QTextDocument::FindWholeWords | QTextDocument::FindCaseSensitively);
    QColor color = hasDarkScheme() ? QColor (0, 77, 160) : QColor (130, 255, 255); // blue highlights
    QTextCursor found;

    while (!(found = finding (selTxt, start, searchFlags, false, endLimit)).isNull())
    {
        if (found.anchor() >= maxSel || found.position() <= minSel)
        {
            QTextEdit::ExtraSelection extra;
            extra.format.setBackground (color);
            extra.cursor = found;
            blueSel_.append (extra);
            es.insert (es.size() - nCol - nRed, extra);
        }
        start.setPosition (found.position());
    }
    setExtraSelections (es);
}
/*************************/
void TextEdit::onContentsChange (int /*position*/, int charsRemoved, int charsAdded)
{
    if (!selectionHighlighting_) return;
    if (charsRemoved > 0 || charsAdded > 0)
    {
        /* wait until the document's layout manager is notified about the change;
           otherwise, the end cursor might be out of range */
        QTimer::singleShot (0, this, &TextEdit::selectionHlight);
    }
}
/*************************/
bool TextEdit::toSoftTabs()
{
    bool res = false;
    QString tab = QString (QChar (QChar::Tabulation));
    QTextCursor orig = textCursor();
    orig.setPosition (orig.anchor());
    setTextCursor (orig);
    QTextCursor found;
    QTextCursor start = orig;
    start.beginEditBlock();
    start.setPosition (0);
    while (!(found = finding (tab, start)).isNull())
    {
        res = true;
        start.setPosition (found.anchor());
        QString softTab = remainingSpaces (textTab_, start);
        start.setPosition (found.position(), QTextCursor::KeepAnchor);
        start.insertText (softTab);
        start.setPosition (start.position());
    }
    start.endEditBlock();
    return res;
}

/*******************************************************************************
 ***** The scrollbar position can't be restored precisely in a direct way  *****
 ***** on reloading because text wrapping can make it unreliable. However, *****
 ***** we can restore it as precisely as possible by finding and restoring *****
 ***** the top, middle and bottom cursors in the following two functions.  *****
 *******************************************************************************/
TextEdit::viewPosition TextEdit::getViewPosition() const
{
    viewPosition vPos;
    if (auto vp = viewport())
    {
        QRect vr = vp->rect();

        /* top cursor (immediately below the top edge of the viewport) */
        int h = QFontMetrics (document()->defaultFont()).lineSpacing();
        QTextCursor topCur = cursorForPosition (QPoint (vr.left(), vr.top() + h / 2));
        if (topCur.block().textDirection() == Qt::RightToLeft)
            topCur = cursorForPosition (QPoint (vr.right() + 1, vr.top() + h / 2));
        QRect cRect = cursorRect (topCur);
        int top = cRect.top();
        vPos.topPos = topCur.position();

        /* current cursor
           NOTE: The top edge of its rectangle may be a little above the
                 top edge of the viewport. So, the position of the top cursor
                 is used for knowing whether it isn't above the viewport. */
        int curPosition = textCursor().position();
        if (curPosition >= vPos.topPos)
        {
            cRect = cursorRect();
            if (cRect.bottom() <= vr.bottom()
                && cRect.left() >= vr.left() && cRect.right() <= vr.right())
            {
                vPos.curPos = curPosition; // otherwise, "vPos.curPos" is -1
            }
        }

        /* bottom cursor (immediately above the bottom edge of the viewport) */
        QTextCursor bottomCur = cursorForPosition (QPoint (vr.left(), vr.bottom()));
        if (bottomCur.block().textDirection() == Qt::RightToLeft)
            bottomCur = cursorForPosition (QPoint (vr.right() + 1, vr.bottom()));
        cRect = cursorRect (bottomCur);
        int bottom = cRect.bottom();
        QTextCursor tmp = bottomCur;
        while (!bottomCur.atStart() && bottom > vr.bottom())
        {
            tmp.setPosition (std::max (bottomCur.position() - 1, 0));
            cRect = cursorRect (tmp);
            if (tmp.block().textDirection() == Qt::RightToLeft)
                tmp = cursorForPosition (QPoint (vr.right() + 1, cRect.center().y()));
            else
                tmp = cursorForPosition (QPoint (vr.left(), cRect.center().y()));
            if (cRect.bottom() >= bottom || cRect.top() < vr.top())
                break;
            bottomCur.setPosition (tmp.position());
            bottom = cRect.bottom();
        }
        vPos.bottomPos = bottomCur.position();

        /* middle cursor */
        int midHeight = (top + bottom + 1) / 2
                        /* at the top of the document, the vertical
                           offset should be taken into account */
                        - (topCur.position() == 0 ? contentOffset().y() : 0);
        tmp = cursorForPosition (QPoint (vr.left(), midHeight));
        if (tmp.block().textDirection() == Qt::RightToLeft)
            tmp = cursorForPosition (QPoint (vr.right() + 1, midHeight));
        cRect = cursorRect (tmp);
        if (cRect.top() >= midHeight)
        {
            tmp.setPosition (std::max (tmp.position() - 1, 0));
            cRect = cursorRect (tmp);
            if (tmp.block().textDirection() == Qt::RightToLeft)
                tmp = cursorForPosition (QPoint (vr.right() + 1, cRect.center().y()));
            else
                tmp = cursorForPosition (QPoint (vr.left(), cRect.center().y()));
        }
        vPos.midPos = tmp.position();
    }
    return vPos;
}
/*************************/
void TextEdit::setViewPostion (const viewPosition vPos)
{
    QTextCursor cur = textCursor();
    cur.movePosition (QTextCursor::End);
    int endPos = cur.position();
    if (vPos.midPos < 0)
    {
        if (vPos.curPos >= 0)
        {
            cur.setPosition (std::min (vPos.curPos, endPos));
            setTextCursor (cur);
        }
        return;
    }

    /* first center the middle cursor */
    cur.setPosition (std::min (vPos.midPos, endPos));
    setTextCursor (cur);
    centerCursor();

    /* if the middle cursor isn't at the line start, the text has changed */
    if (auto vp = viewport())
    {
        QRect vr = vp->rect();
        QRect cRect = cursorRect (cur);
        QTextCursor tmp;
        if (cur.block().textDirection() == Qt::RightToLeft)
            tmp = cursorForPosition (QPoint (vr.right() + 1, cRect.center().y()));
        else
            tmp = cursorForPosition (QPoint (vr.left(), cRect.center().y()));
        if (tmp != cur)
        {
            setTextCursor (tmp);
            centerCursor();
            if (vPos.curPos >= 0)
            {
                tmp.setPosition (std::min (vPos.curPos, endPos));
                setTextCursor (tmp);
            }
            return;
        }
    }

    /* also ensure that the top and bottom cursors are visible */
    QTextCursor tmp = cur;
    if (vPos.topPos >= 0)
    {
        tmp.setPosition (std::min (vPos.topPos, endPos));
        setTextCursor (tmp);
    }
    if (vPos.bottomPos >= 0)
    {
        tmp.setPosition (std::min (vPos.bottomPos, endPos));
        setTextCursor (tmp);
    }

    /* restore the original text cursor if it's visible;
       otherwise, go back to the middle cursor */
    if (vPos.curPos >= 0)
        cur.setPosition (std::min (vPos.curPos, endPos));
    setTextCursor (cur);
}

}
