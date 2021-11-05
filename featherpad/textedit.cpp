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

#include <QApplication>
#include <QTimer>
#include <QDateTime>
#include <QPainter>
#include <QMenu>
#include <QDesktopServices>
#include <QProcess>
#include <QRegularExpression>
#include <QClipboard>
#include <QTextDocumentFragment>
#include "textedit.h"
#include "vscrollbar.h"

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

    pasting_ = false;
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;

    prog_ = "url"; // the default language

    textTab_ = "    "; // the default text tab is four spaces

    setMouseTracking (true);
    //document()->setUseDesignMetrics (true);

    /* set the backgound color and ensure enough contrast
       between the selection and line highlight colors */
    QPalette p = palette();
    bgColorValue = qBound (0, bgColorValue, 255);
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
           document terminators. The real text will be formatted by the highlgihter. */
        separatorColor_ = Qt::white;
        separatorColor_.setAlpha (95 - qRound (3 * static_cast<qreal>(darkValue_) / 5));
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
        separatorColor_.setAlpha (2 * qRound (static_cast<qreal>(bgColorValue) / 5) - 32);
    }
    setCurLineHighlight (-1);

    resizeTimerId_ = 0;
    selectionTimerId_ = 0;
    selectionHighlighting_ = false;
    highlightThisSelection_ = true;
    removeSelectionHighlights_ = false;
    size_ = 0;
    wordNumber_ = -1; // not calculated yet
    encoding_= "UTF-8";
    uneditable_ = false;
    highlighter_ = nullptr;
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
    });
    connect (this, &QPlainTextEdit::selectionChanged, this, &TextEdit::onSelectionChanged);

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
        int opacity = qBound (1, qRound (static_cast<qreal>(darkValue_ * (19 * darkValue_ - 2813)) / static_cast<qreal>(5175)) + 20, 30);
        lineHColor_ = QColor (255, 255, 255, opacity);

    }
}
/*************************/
bool TextEdit::eventFilter (QObject *watched, QEvent *event)
{
    if (watched == lineNumberArea_ && event->type() == QEvent::Wheel)
    {
        if (QWheelEvent *we = static_cast<QWheelEvent*>(event))
        {
            wheelEvent (we);
            return false;
        }
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
    int max = qMax (1, blockCount());
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
    {// this is more intuitive to me
        if (cusror.anchor() <= cusror.position())
            cusror.setPosition (cusror.anchor());
        else
            cusror.setPosition (cusror.position());
    }
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
    qreal spaceL = fm.horizontalAdvance (" ");
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    { // find tab widths in terms of spaces
        tmp.setPosition (tmp.block().position() + i);
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<qreal>(cursorRect (tmp).right()) - x;
        n += qMax (qRound (qAbs (x) / spaceL) - 1, 0); // x is negative for RTL
        ++i;
    }
    n += txt.count();
    n = spaceTab.count() - n % spaceTab.count();
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
    qreal spaceL = fm.horizontalAdvance (" ");
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    { // find tab widths in terms of spaces
        tmp.setPosition (tmp.block().position() + i);
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<qreal>(cursorRect (tmp).right()) - x;
        n += qMax (qRound (qAbs (x) / spaceL) - 1, 0);
        ++i;
    }
    n += txt.count();
    n = n % textTab_.count();
    if (n == 0) n = textTab_.count();

    if (twoSpace) n = qMin (n, 2);

    tmp.setPosition (txtStart);
    QChar ch = blockText.at (indx - 1);
    if (ch == QChar (QChar::Space))
        tmp.setPosition (txtStart - n, QTextCursor::KeepAnchor);
    else // the previous character is a tab
    {
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (txtStart - 1, QTextCursor::KeepAnchor);
        x -= static_cast<qreal>(cursorRect (tmp).right());
        n -= qRound (qAbs (x) / spaceL);
        if (n < 0) n = 0; // impossible without "twoSpace"
        tmp.setPosition (tmp.position() - n, QTextCursor::KeepAnchor);
    }

    return tmp;
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
                        viewport()->setCursor (Qt::IBeamCursor);
                    else
                        viewport()->setCursor (Qt::PointingHandCursor);
                    QPlainTextEdit::keyPressEvent (event);
                    return;
                }
            }
        }
        if (event->key() != Qt::Key_Control) // another modifier/key is pressed
        {
            if (highlighter_)
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
        bool hadSelection (textCursor().hasSelection());
        QPlainTextEdit::keyPressEvent (event);
        if (!hadSelection)
            emit updateBracketMatching(); // isn't emitted in another way
        return;
    }

    if (event->key() == Qt::Key_Backspace)
    {
        keepTxtCurHPos_ = true;
        if (txtCurHPos_ < 0)
        {
            QTextCursor startCur = textCursor();
            startCur.movePosition (QTextCursor::StartOfLine);
            txtCurHPos_ = qAbs (cursorRect().left() - cursorRect (startCur).left()); // is negative for RTL
        }

    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        keepTxtCurHPos_ = true;
        if (txtCurHPos_ < 0)
        {
            QTextCursor startCur = textCursor();
            startCur.movePosition (QTextCursor::StartOfLine);
            txtCurHPos_ = qAbs (cursorRect().left() - cursorRect (startCur).left());
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
                cursor.setPosition (qMin (anch, pos));
                cursor.movePosition (QTextCursor::StartOfBlock);
                cursor.setPosition (qMax (anch, pos), QTextCursor::KeepAnchor);
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
                cursor.setPosition (qMax (anch, pos));
                cursor.movePosition (QTextCursor::EndOfBlock);
                cursor.setPosition (qMin (anch, pos), QTextCursor::KeepAnchor);
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
                hPos = qAbs (cursorRect().left() - cursorRect (startCur).left()); // is negative for RTL
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
                    bool rtl (cursor.block().text().isRightToLeft());
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
                    bool rtl (cursor.block().text().isRightToLeft());
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
            cursor.setPosition (qMin (cursor.anchor(), cursor.position())); // go to the first block
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
            QTextCursor tmp (cursor);
            tmp.setPosition (qMin (tmp.anchor(), tmp.position()));
            cursor.insertText (remainingSpaces (event->modifiers() & Qt::MetaModifier
                                                ? "  " : textTab_, tmp));
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_Backtab)
    {
        QTextCursor cursor = textCursor();
        int newLines = cursor.selectedText().count (QChar (QChar::ParagraphSeparator));
        cursor.setPosition (qMin (cursor.anchor(), cursor.position()));
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

    pasting_ = true;  // see insertFromMimeData()
    QPlainTextEdit::paste();
    pasting_ = false;
}
void TextEdit::selectAll()
{
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1; // Qt bug: cursorPositionChanged() isn't emitted with selectAll()
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
// We want to pass dropping of files to the main widget with a custom signal.
// We also want to control whether the pasted URLs should be opened.
bool TextEdit::canInsertFromMimeData (const QMimeData* source) const
{
    return source->hasUrls() || QPlainTextEdit::canInsertFromMimeData (source);
}
void TextEdit::insertFromMimeData (const QMimeData* source)
{
    keepTxtCurHPos_ = false;
    if (source->hasUrls())
    {
        const QList<QUrl> urlList = source->urls();
        bool multiple (urlList.count() > 1);
        if (pasting_ && pastePaths_)
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
                emit fileDropped (file, 0, 0, multiple);
            }
        }
    }
    else
        QPlainTextEdit::insertFromMimeData (source);
}
/*************************/
void TextEdit::keyReleaseEvent (QKeyEvent *event)
{
    /* deal with hyperlinks */
    if (highlighter_ && event->key() == Qt::Key_Control)
        viewport()->setCursor (Qt::IBeamCursor);
    QPlainTextEdit::keyReleaseEvent (event);
}
/*************************/
void TextEdit::wheelEvent (QWheelEvent *event)
{
    QPoint anglePoint = event->angleDelta();
    if (event->modifiers() & Qt::ControlModifier)
    {
        float delta = anglePoint.y() / 120.f;
        zooming (delta);
        return;
    }

    bool horizontal (qAbs (anglePoint.x()) > qAbs (anglePoint.y()));
    if (event->modifiers() & Qt::ShiftModifier)
    { // line-by-line scrolling when Shift is pressed
        int delta = horizontal ? anglePoint.x()
                               : anglePoint.y();
        if (qAbs (delta) >= QApplication::wheelScrollLines())
        {
#if (QT_VERSION >= QT_VERSION_CHECK(5,15,0))
            QWheelEvent e (event->position(),
                           event->globalPosition(),
#else
            QWheelEvent e (event->posF(),
                           event->globalPosF(),
#endif
                           event->pixelDelta(),
                           QPoint (0, delta / QApplication::wheelScrollLines()),
                           event->buttons(),
                           Qt::NoModifier,
                           event->phase(),
                           false,
                           event->source());
            QCoreApplication::sendEvent (horizontal
                                             ? horizontalScrollBar()
                                             : verticalScrollBar(), &e);
        }
        return;
    }

    /* inertial scrolling */
    if (inertialScrolling_
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
            if (qAbs (delta) >= 120)
            {
                if (qAbs (delta * 3) >= QApplication::wheelScrollLines())
                    delta = delta * 3 / QApplication::wheelScrollLines();
            }
            else if (qAbs (delta) >= QApplication::wheelScrollLines())
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
            int steps = qMax (SCROLL_FRAMES_PER_SEC / wheelEvents.size(), 5)
                        * SCROLL_DURATION / 1000;

            /* wait until the angle delta reaches an acceptable value */
            static int _delta = 0;
            _delta += delta;
            if (qAbs (_delta) < steps)
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
        totalDelta += qRound (static_cast<qreal>(it->delta) / static_cast<qreal>(it->totalSteps));
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
    lineNumberArea_->setGeometry (QRect (QApplication::layoutDirection() == Qt::RightToLeft ? cr.width() - lineNumberAreaWidth() : cr.left(),
                                         cr.top(), lineNumberAreaWidth(), cr.height()));

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
    }
}
/*******************************************************
***** Workaround for the RTL bug in QPlainTextEdit *****
********************************************************/
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

// Exactly like QPlainTextEdit::paintEvent(),
// except for setting layout text option for RTL
// and drawing vertical indentation lines (if needed).
void TextEdit::paintEvent (QPaintEvent *event)
{
    QPainter painter (viewport());
    Q_ASSERT (qobject_cast<QPlainTextDocumentLayout*>(document()->documentLayout()));

    QPointF offset (contentOffset());

    QRect er = event->rect();
    QRect viewportRect = viewport()->rect();
    qreal maximumWidth = document()->documentLayout()->documentSize().width();
    painter.setBrushOrigin (offset);

    int maxX = static_cast<int>(offset.x() + qMax (static_cast<qreal>(viewportRect.width()), maximumWidth)
                                - document()->documentMargin());
    er.setRight (qMin (er.right(), maxX));
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
            /* take care of RTL */
            bool rtl (block.text().isRightToLeft());
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
                contentsRect.setWidth (qMax (r.width(), maximumWidth));
                fillBackground (&painter, contentsRect, bg);
            }

            if (lineNumberArea_->isVisible() && (opt.flags() & QTextOption::ShowLineAndParagraphSeparators))
            {
                /* "QTextFormat::FullWidthSelection" isn't respected when new-lines are shown.
                   This is a workaround. */
                QRectF contentsRect = r;
                contentsRect.setWidth (qMax (r.width(), maximumWidth));
                if (contentsRect.contains (cursorRect().center()))
                {
                    contentsRect.setTop (cursorRect().top());
                    contentsRect.setBottom (cursorRect().bottom());
                    fillBackground (&painter, contentsRect, lineHColor_);
                }
            }

            QVector<QTextLayout::FormatRange> selections;
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
                    int yTop = qRound (r.topLeft().y());
                    int yBottom =  qRound (r.height() >= static_cast<qreal>(2) * fm.lineSpacing()
                                               ? yTop + fm.height()
                                               : r.bottomLeft().y() - static_cast<qreal>(1));
                    qreal tabWidth = fm.horizontalAdvance (textTab_);
                    if (rtl)
                    {
                        qreal leftMost = cursorRect (cur).left();
                        qreal x = r.topRight().x();
                        x -= tabWidth;
                        while (x >= leftMost)
                        {
                            painter.drawLine (QLine (qRound (x), yTop, qRound (x), yBottom));
                            x -= tabWidth;
                        }
                    }
                    else
                    {
                        qreal rightMost = cursorRect (cur).right();
                        qreal x = r.topLeft().x();
                        x += tabWidth;
                        while (x <= rightMost)
                        {
                            painter.drawLine (QLine (qRound (x), yTop, qRound (x), yBottom));
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
                qreal rulerSpace = fm.horizontalAdvance (' ') * static_cast<qreal>(vLineDistance_);
                int yTop = qRound (r.topLeft().y());
                int yBottom =  qRound (r.height() >= static_cast<qreal>(2) * fm.lineSpacing()
                                       ? yTop + fm.height()
                                       : r.bottomLeft().y() - static_cast<qreal>(1));
                qreal rightMost = er.right();
                qreal x = static_cast<qreal>(cursorRect (cur).right());
                x += rulerSpace;
                while (x <= rightMost)
                {
                    painter.drawLine (QLine (qRound (x), yTop, qRound (x), yBottom));
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
/************************************************
***** End of the Workaround for the RTL bug *****
*************************************************/

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
                int cur = cursorRect().center().y();
                painter.setFont (bf);
                painter.setPen (currentLineFg);
                painter.fillRect (0, cur - h / 2, w, h, currentLineBg);
                QTextCursor tmp = textCursor();
                tmp.movePosition (QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
                if (!tmp.atBlockStart())
                {
                    painter.drawText (left, cur - h / 2, w - 3, h,
                                      Qt::AlignRight, rtl ? "↲" : "↳");
                    painter.setPen (currentBlockFg);
                    if (tmp.movePosition (QTextCursor::Up, QTextCursor::MoveAnchor) // always true
                        && !tmp.atBlockStart())
                    {
                        cur = cursorRect (tmp).center().y();
                        painter.drawText (left, cur - h / 2, w - 3, h,
                                          Qt::AlignRight, number);
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

    /* selection highlighting */
    if (!selectionHighlighting_) return;
    if (highlightThisSelection_)
        removeSelectionHighlights_ = false; // reset
    else
    {
        removeSelectionHighlights_ = true;
        highlightThisSelection_ = true; // reset
    }
    if (selectionTimerId_)
    {
        killTimer (selectionTimerId_);
        selectionTimerId_ = 0;
    }
    selectionTimerId_ = startTimer (UPDATE_INTERVAL);
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
        f.setPointSizeF (static_cast<qreal>(newSize));
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
    cursor.setPosition (qMin (anch, pos));
    cursor.movePosition (QTextCursor::StartOfBlock);
    cursor.setPosition (qMax (anch, pos), QTextCursor::KeepAnchor);
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
    if (text.length() <= 10000) // otherwise, too long
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
void TextEdit::mouseMoveEvent (QMouseEvent *event)
{
    QPlainTextEdit::mouseMoveEvent (event);

    if (!highlighter_) return;
    if (!(qApp->keyboardModifiers() & Qt::ControlModifier))
    {
        viewport()->setCursor (Qt::IBeamCursor);
        return;
    }

    if (getUrl (cursorForPosition (event->pos()).position()).isEmpty())
        viewport()->setCursor (Qt::IBeamCursor);
    else
        viewport()->setCursor (Qt::PointingHandCursor);
}
/*************************/
void TextEdit::mousePressEvent (QMouseEvent *event)
{
    /* forget the last cursor position */
    keepTxtCurHPos_ = false;
    txtCurHPos_ = -1;

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
            if (!(qApp->keyboardModifiers() & Qt::ControlModifier))
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

    QPlainTextEdit::mousePressEvent (event);

    if (highlighter_
        && event->button() == Qt::LeftButton
        && (qApp->keyboardModifiers() & Qt::ControlModifier))
    {
        pressPoint_ = event->pos();
    }
}
/*************************/
void TextEdit::mouseReleaseEvent (QMouseEvent *event)
{
    pasting_ = true; // see insertFromMimeData()
    QPlainTextEdit::mouseReleaseEvent (event);
    pasting_ = false;

    if (event->button() != Qt::LeftButton
        || !highlighter_
        || !(qApp->keyboardModifiers() & Qt::ControlModifier)
        /* another key may also be pressed (-> keyPressEvent) */
        || viewport()->cursor().shape() != Qt::PointingHandCursor)
    {
        return;
    }

    QTextCursor cur = cursorForPosition (event->pos());
    if (cur == cursorForPosition (pressPoint_))
    {
        QString str = getUrl (cur.position());
        if (!str.isEmpty())
        {
            QUrl url (str);
            if (url.isRelative()) // treat relative URLs as local paths (not needed here)
                url = QUrl::fromUserInput (str, "/");
            if (!QProcess::startDetached ("gio", QStringList() << "open" << url.toString()))
                QDesktopServices::openUrl (url);
        }
    }
    pressPoint_ = QPoint();
}
/*************************/
void TextEdit::mouseDoubleClickEvent (QMouseEvent *event)
{
    tripleClickTimer_.start();
    QPlainTextEdit::mouseDoubleClickEvent (event);
}
/*************************/
bool TextEdit::event (QEvent *event)
{
    if (highlighter_
        && ((event->type() == QEvent::WindowDeactivate && hasFocus()) // another window is activated
            || event->type() == QEvent::FocusOut)) // another widget has been focused
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
        QTextCursor cursor = start;
        QRegularExpressionMatch match;
        if (!(flags & QTextDocument::FindBackward))
        {
            cursor.setPosition (qMax (cursor.anchor(), cursor.position())); // as with ordinary search
            while (!cursor.atEnd())
            {
                if (!cursor.atBlockEnd()) // otherwise, it'll be returned with ".*"
                {
                    if (end > 0 && cursor.anchor() > end)
                        break;
                    int indx = cursor.block().text().indexOf (regexp, cursor.positionInBlock(), &match);
                    if (indx > -1)
                    {
                        if (match.capturedLength() == 0) // no empty match (with "\w*", for example)
                        {
                            cursor.setPosition (cursor.position() + 1);
                            continue;
                        }
                        if (end > 0 && indx + cursor.block().position() > end)
                            break;
                        res.setPosition (indx + cursor.block().position());
                        res.setPosition (res.position() + match.capturedLength(), QTextCursor::KeepAnchor);
                        return  res;
                    }
                }
                if (!cursor.movePosition (QTextCursor::NextBlock))
                    break;
            }
        }
        else // with a backward search, the block/doc start should also be checked
        {
            cursor.setPosition (cursor.anchor()); // as with ordinary search
            while (true)
            {
                const int bp = cursor.block().position();
                int indx = cursor.block().text().lastIndexOf (regexp, cursor.position() - bp, &match);
                if (indx > -1)
                {
                    if (match.capturedLength() == 0 // no empty match
                        /* the match start should be before the search start */
                        || bp + indx == start.anchor())
                    {
                        if (cursor.atBlockStart())
                        {
                            if (!cursor.movePosition (QTextCursor::PreviousBlock))
                                break;
                            cursor.movePosition (QTextCursor::EndOfBlock);
                        }
                        else
                            cursor.setPosition (cursor.position() - 1);
                        continue;
                    }
                    res.setPosition (indx + bp);
                    res.setPosition (res.position() + match.capturedLength(), QTextCursor::KeepAnchor);
                    return  res;
                }
                if (!cursor.movePosition (QTextCursor::PreviousBlock))
                    break;
                cursor.movePosition (QTextCursor::EndOfBlock);
            }
        }
        return QTextCursor();
    }
    else if (str.contains ('\n'))
    {
        QTextCursor cursor = start;
        QTextCursor found;
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
                        if ((found = document()->find (subStr, cursor, flags)).isNull())
                            return QTextCursor();
                        if (end > 0 && found.anchor() > end)
                            return QTextCursor();
                        cursor.setPosition (found.position());
                        /* if the match doesn't end the block... */
                        while (!cursor.atBlockEnd())
                        {
                            /* ... move the cursor to right and search until a match is found */
                            cursor.movePosition (QTextCursor::EndOfBlock);
                            cursor.setPosition (cursor.position() - subStr.length());
                            if ((found = document()->find (subStr, cursor, flags)).isNull())
                                return QTextCursor();
                            if (end > 0 && found.anchor() > end)
                                return QTextCursor();
                            cursor.setPosition (found.position());
                        }

                        res.setPosition (found.anchor());
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
                        cursor.setPosition (cursor.anchor() + subStr.count());
                        break;
                    }
                    else
                    {
                        if ((found = document()->find (subStr, cursor, flags)).isNull()
                            || found.anchor() != cursor.position())
                        {
                            cursor.setPosition (res.position());
                            if (!cursor.movePosition (QTextCursor::NextBlock))
                                return QTextCursor();
                            i = 0;
                            continue;
                        }
                        cursor.setPosition (found.position());
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
                            cursor.setPosition (cursor.block().position() + subStr.count());
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
                        cursor.setPosition (cursor.anchor() - subStr.count());
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
        {
            res = document()->find (str, start, flags);
            if (end > 0 && res.anchor() > end)
                return QTextCursor();
        }
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
        if (selectionTimerId_)
        {
            killTimer (selectionTimerId_);
            selectionTimerId_ = 0;
        }
        /* remove all blue highlights */
        if (!blueSel_.isEmpty())
        {
            QList<QTextEdit::ExtraSelection> es = extraSelections();
            int nRed = redSel_.count();
            int n = blueSel_.count();
            while (n > 0 && es.size() - nRed > 0)
            {
                es.removeAt (es.size() - 1 - nRed);
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
    const QString selTxt = selCursor.selection().toPlainText();
    int nRed = redSel_.count(); // bracket highlights (come last)

    /* remove all blue highlights */
    int n = blueSel_.count();
    while (n > 0 && es.size() - nRed > 0)
    {
        es.removeAt (es.size() - 1 - nRed);
        --n;
    }

    if (removeSelectionHighlights_ || selTxt.isEmpty())
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

    blueSel_.clear();

    /* first put a start cursor at the top left edge... */
    QPoint Point (0, 0);
    QTextCursor start = cursorForPosition (Point);
    /* ... then move it backward by the search text length */
    int startPos = start.position() - selTxt.length();
    if (startPos >= 0)
        start.setPosition (startPos);
    else
        start.setPosition (0);
    /* get the visible text to check if the search string is inside it */
    Point = QPoint (geometry().width(), geometry().height());
    QTextCursor end = cursorForPosition (Point);
    int endLimit = end.anchor();
    int endPos = end.position() + selTxt.length();
    end.movePosition (QTextCursor::End);
    if (endPos <= end.position())
        end.setPosition (endPos);
    QTextCursor visCur = start;
    visCur.setPosition (end.position(), QTextCursor::KeepAnchor);
    const QString str = visCur.selection().toPlainText(); // '\n' is included in this way
    if (str.contains (selTxt)) // don't waste time if the selected text isn't visible
    {
        QTextDocument::FindFlags searchFlags = (QTextDocument::FindWholeWords | QTextDocument::FindCaseSensitively);
        QColor color = hasDarkScheme() ? QColor (0, 77, 160) : QColor (130, 255, 255); // blue highlights
        QTextCursor found;

        while (!(found = finding (selTxt, start, searchFlags,  false, endLimit)).isNull())
        {
            if (selCursor.anchor() <= selCursor.position()
                    ? (found.anchor() != selCursor.anchor() || found.position() != selCursor.position())
                    : (found.anchor() != selCursor.position() || found.position() != selCursor.anchor()))
            {
                QTextEdit::ExtraSelection extra;
                extra.format.setBackground (color);
                extra.cursor = found;
                blueSel_.append (extra);
                es.insert (es.size() - nRed, extra);
            }
            start.setPosition (found.position());
        }
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

}
