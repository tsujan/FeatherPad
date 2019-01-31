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

#include <QApplication>
#include <QTimer>
#include <QDateTime>
#include <QPainter>
#include <QMenu>
#include <QDesktopServices>
#include <QProcess>
#include <QRegularExpression>
#include <QClipboard>
#include "textedit.h"
#include "vscrollbar.h"

#define UPDATE_INTERVAL 50 // in ms
#define SCROLL_FRAMES_PER_SEC 60
#define SCROLL_DURATION 300 // in ms

namespace FeatherPad {

TextEdit::TextEdit (QWidget *parent, int bgColorValue) : QPlainTextEdit (parent)
{
    prevAnchor = prevPos = -1;
    autoIndentation = true;
    autoBracket = false;
    scrollJumpWorkaround = false;
    drawIndetLines = false;
    saveCursor_ = false;
    vLineDistance_ = 0;
    matchedBrackets_ = false;

    inertialScrolling_ = false;
    wheelEvent_ = nullptr;
    scrollTimer_ = nullptr;

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
        darkValue = bgColorValue;
        /* a quadratic equation for bgColorValue -> opacity: 0 -> 20,  27 -> 8, 50 -> 2 */
        int opacity = qBound (1, qRound (static_cast<qreal>(bgColorValue * (19 * bgColorValue - 2813)) / static_cast<qreal>(5175)) + 20, 30);
        lineHColor = QColor (255, 255, 255, opacity);
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
    }
    else
    {
        darkValue = -1;
        lineHColor = QColor (0, 0, 0, 4);
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
    }

    resizeTimerId = 0;
    updateTimerId = 0;
    Dy = 0;
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

    lineNumberArea = new LineNumberArea (this);
    lineNumberArea->setToolTip (tr ("Double click to center current line"));
    lineNumberArea->hide();
    lineNumberArea->installEventFilter (this);

    connect (this, &QPlainTextEdit::updateRequest, this, &TextEdit::onUpdateRequesting);
    connect (this, &QPlainTextEdit::cursorPositionChanged, this, &TextEdit::updateBracketMatching);
    connect (this, &QPlainTextEdit::selectionChanged, this, &TextEdit::onSelectionChanged);

    setContextMenuPolicy (Qt::CustomContextMenu);
    connect (this, &QWidget::customContextMenuRequested, this, &TextEdit::showContextMenu);
}
/*************************/
bool TextEdit::eventFilter (QObject *watched, QEvent *event)
{
    if (watched == lineNumberArea && event->type() == QEvent::Wheel)
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
#if QT_VERSION < 0x051000
    opt.setTabStop (metrics.width (textTab_));
#else
    opt.setTabStopDistance (metrics.width (textTab_));
#endif
    document()->setDefaultTextOption (opt);

    /* the line number is bold only for the current line */
    QFont F(f);
    if (f.bold())
    {
        F.setBold (false);
        lineNumberArea->setFont (F);
    }
    else
        lineNumberArea->setFont (f);
    /* find the widest digit (used in calculating line number area width)*/
    F.setBold (true);
    widestDigit = 0;
    for (int i = 0; i < 10; ++i)
    {
         if (QFontMetrics (F).width (QString::number (i)) > widestDigit)
             widestDigit = i;
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
    delete lineNumberArea;
}
/*************************/
void TextEdit::showLineNumbers (bool show)
{
    if (show)
    {
        lineNumberArea->show();
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

        lineNumberArea->hide();
        setViewportMargins (0, 0, 0, 0);
        QList<QTextEdit::ExtraSelection> es = extraSelections();
        if (!es.isEmpty() && !currentLine.cursor.isNull())
            es.removeFirst();
        setExtraSelections (es);
        currentLine.cursor = QTextCursor(); // nullify currentLine
        lastCurrentLine = QRect();
    }
}
/*************************/
int TextEdit::lineNumberAreaWidth()
{
    QString digit = QString::number (widestDigit);
    QString num = digit;
    int max = qMax (1, blockCount());
    while (max >= 10)
    {
        max /= 10;
        num += digit;
    }
    QFont f = font();
    f.setBold (true);
    return (6 + QFontMetrics (f).width (num)); // 6 = 3 + 3 (-> lineNumberAreaPaintEvent)
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
        lineNumberArea->scroll (0, dy);
    else
    {
        /* since the current line number is made bold and italic,
           it should be updated also when the line is wrapped */
        if (lastCurrentLine.isValid())
            lineNumberArea->update (0, lastCurrentLine.y(), lineNumberArea->width(), lastCurrentLine.height());
        QRect totalRect;
        QTextCursor cur = cursorForPosition (rect.center());
        if (rect.contains (cursorRect (cur).center()))
        {
            QRectF blockRect = blockBoundingGeometry (cur.block()).translated (contentOffset());
            totalRect = rect.united (blockRect.toRect());
        }
        else
            totalRect = rect;
        lineNumberArea->update (0, totalRect.y(), lineNumberArea->width(), totalRect.height());
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
void TextEdit::removeGreenHighlights()
{
    setGreenSel (QList<QTextEdit::ExtraSelection>());
    if (getSearchedText().isEmpty()) // FPwin::hlight() won't be called
    {
        QList<QTextEdit::ExtraSelection> es;
        es.prepend (currentLineSelection());
        es.append (getRedSel());
        setExtraSelections (es);
    }
}
/*************************/
// Finds the (remaining) spaces that should be inserted with Ctrl+Tab.
QString TextEdit::remainingSpaces (const QString& spaceTab, const QTextCursor& cursor) const
{
    QTextCursor tmp = cursor;
    QString txt = cursor.block().text().left (cursor.positionInBlock());
    QFontMetricsF fm = QFontMetricsF (document()->defaultFont());
    qreal spaceL = fm.width (" ");
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    { // find tab widths in terms of spaces
        tmp.setPosition (tmp.block().position() + i);
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<qreal>(cursorRect (tmp).right()) - x;
        n += qMax (qRound (x / spaceL) - 1, 0);
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
    qreal spaceL = fm.width (" ");
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    { // find tab widths in terms of spaces
        tmp.setPosition (tmp.block().position() + i);
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<qreal>(cursorRect (tmp).right()) - x;
        n += qMax (qRound (x / spaceL) - 1, 0);
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
        n -= qRound (x / spaceL);
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
            /* handle undoing */
            else if (!isReadOnly() && event->key() == Qt::Key_Z)
            {
                /* QWidgetTextControl::undo() callls ensureCursorVisible() even when there's nothing to undo.
                   Users may press Ctrl+Z just to know whether a document is in its original state and
                   a scroll jump can confuse them when there's nothing to undo. */
                if (!document()->isUndoAvailable())
                {
                    event->accept();
                    return;
                }
                /* always remove replacing highlights before undoing */
                removeGreenHighlights();
            }
        }
        if (event->key() != Qt::Key_Control) // another modifier/key is pressed
        {
            if (highlighter_)
                viewport()->setCursor (Qt::IBeamCursor);
            /* QWidgetTextControl::redo() calls ensureCursorVisible() even when there's nothing to redo.
               That may cause a scroll jump, which can be confusing when nothing else has happened. */
            if (!isReadOnly() && (event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_Z
                && !document()->isRedoAvailable())
            {
                event->accept();
                return;
            }
        }
    }

    if (isReadOnly())
    {
        QPlainTextEdit::keyPressEvent (event);
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        QTextCursor cur = textCursor();
        bool isBracketed (false);
        QString selTxt = cur.selectedText();
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
        }
        else
        {
            /* find the indentation */
            if (autoIndentation)
                indent = computeIndentation (cur);
            /* check whether a bracketed text is selected
               so that the cursor position is at its start */
            QTextCursor anchorCur = cur;
            anchorCur.setPosition (cur.anchor());
            if (autoBracket
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

        if (withShift || autoIndentation || isBracketed)
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
        if (autoBracket)
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
        /*if (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::ShiftModifier)
        {
            // NOTE: This reverses a Qt feature with Down/Up after Backspace/Enter and is commented out
            QTextCursor cursor = textCursor();
            if (!cursor.hasSelection())
            { // go to the same position in the next/previous line
                int hPos = cursorRect().center().x();
                QTextCursor::MoveMode mode = (event->modifiers() == Qt::ShiftModifier
                                                  ? QTextCursor::KeepAnchor
                                                  : QTextCursor::MoveAnchor);
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
                    cursor.setPosition (cursorForPosition (QPoint (hPos, cursorRect(cursor).center().y())).position(), mode);
                }
                setTextCursor (cursor);
                ensureCursorVisible();
                event->accept();
                return;
            }
        }*/
        if (event->modifiers() == Qt::ControlModifier)
        {
            if (QScrollBar* vbar = verticalScrollBar())
            { // scroll without changing the cursor position
                vbar->setValue(vbar->value() + (event->key() == Qt::Key_Down ? 1 : -1));
                event->accept();
                return;
            }
        }
        else if (event->modifiers() == Qt::MetaModifier || event->modifiers() == (Qt::ShiftModifier | Qt::MetaModifier))
        { // go to the same position in the next/previous block
            QTextCursor cursor = textCursor();
            int hPos = cursorRect().center().x();
            QTextCursor::MoveMode mode = ((event->modifiers() & Qt::ShiftModifier)
                                              ? QTextCursor::KeepAnchor
                                              : QTextCursor::MoveAnchor);
            cursor.movePosition (event->key() == Qt::Key_Down
                                     ? QTextCursor::EndOfBlock
                                     : QTextCursor::StartOfBlock,
                                 mode);
            if (cursor.movePosition (event->key() == Qt::Key_Down
                                         ? QTextCursor::NextBlock
                                         : QTextCursor::PreviousBlock,
                                     mode))
            {
                cursor.setPosition (cursorForPosition (QPoint (hPos, cursorRect(cursor).center().y())).position(), mode);
            }
            setTextCursor (cursor);
            ensureCursorVisible();
            event->accept();
            return;
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
    }
    else if (event->key() == Qt::Key_Tab)
    {
        QTextCursor cursor = textCursor();
        int newLines = cursor.selectedText().count (QChar (QChar::ParagraphSeparator));
        if (newLines > 0)
        {
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
                if ((event->modifiers() & Qt::ControlModifier) || tabInsertSpaces_)
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
            event->accept();
            return;
        }
        else if ((event->modifiers() & Qt::ControlModifier) || tabInsertSpaces_)
        {
            QTextCursor tmp (cursor);
            tmp.setPosition (qMin (tmp.anchor(), tmp.position()));
            cursor.insertText (remainingSpaces (event->modifiers() & Qt::MetaModifier
                                                ? "  " : textTab_, tmp));
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_Backtab)
    {
        QTextCursor cursor = textCursor();
        int newLines = cursor.selectedText().count (QChar (QChar::ParagraphSeparator));
        cursor.setPosition (qMin (cursor.anchor(), cursor.position()));
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

        /* otherwise, do nothing with SHIFT+TAB */
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_Insert)
    {
        setOverwriteMode (!overwriteMode());
        if (!overwriteMode())
            update(); // otherwise, a part of the thick cursor might remain
    }
    /* because of a bug in Qt5, the non-breaking space (ZWNJ) isn't inserted with SHIFT+SPACE */
    else if (event->key() == 0x200c)
    {
        insertPlainText (QChar (0x200C));
        event->accept();
        return;
    }

    QPlainTextEdit::keyPressEvent (event);
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
// A workaround for Qt5's scroll jump bug
void TextEdit::wheelEvent (QWheelEvent *event)
{
    if (scrollJumpWorkaround && event->angleDelta().manhattanLength() > 240)
        event->ignore();
    else
    {
        if (event->modifiers() & Qt::ControlModifier)
        {
            float delta = event->angleDelta().y() / 120.f;
            zooming (delta);
            return;
        }
        if (event->modifiers() & Qt::ShiftModifier)
        { // line-by-line scrolling when Shift is pressed
            QWheelEvent e (event->pos(), event->globalPos(),
                           event->angleDelta().y() / QApplication::wheelScrollLines(),
                           event->buttons(), Qt::NoModifier, Qt::Vertical);
            QCoreApplication::sendEvent (verticalScrollBar(), &e);
            return;
        }
        if (!inertialScrolling_
            || !event->spontaneous()
            || (event->modifiers() & Qt::AltModifier))
        { // proceed as in QPlainTextEdit::wheelEvent()
            QAbstractScrollArea::wheelEvent (event);
            updateMicroFocus();
            return;
        }

        if (QScrollBar* vbar = verticalScrollBar())
        {
            /* always set the initial speed to 3 lines per wheel turn */
            int delta = event->angleDelta().y() * 3 / QApplication::wheelScrollLines();
            if((delta > 0 && vbar->value() == vbar->minimum())
               || (delta < 0 && vbar->value() == vbar->maximum()))
            {
                return; // the scrollbar can't move
            }
            /* keep track of the wheel event */
            wheelEvent_ = event;
            /* find the number of wheel events in 500 ms
               and set the scroll frames per second accordingly */
            static QList<qint64> wheelEvents;
            wheelEvents << QDateTime::currentMSecsSinceEpoch();
            while (wheelEvents.last() - wheelEvents.first() > 500)
                wheelEvents.removeFirst();
            int fps = qMax (SCROLL_FRAMES_PER_SEC / wheelEvents.size(), 5);

            /* set the data for inertial scrolling */
            scrollData data;
            data.delta = delta;
            data.totalSteps = data.leftSteps = fps * SCROLL_DURATION / 1000;
            queuedScrollSteps_.append (data);
            if (!scrollTimer_)
            {
                scrollTimer_ = new QTimer();
                scrollTimer_->setTimerType (Qt::PreciseTimer);
                connect (scrollTimer_, &QTimer::timeout, this, &TextEdit::scrollWithInertia);
            }
            scrollTimer_->start (1000 / SCROLL_FRAMES_PER_SEC);
        }
    }
}
/*************************/
void TextEdit::scrollWithInertia()
{
    if (!wheelEvent_ || !verticalScrollBar()) return;

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
    /* -> qevent.cpp -> QWheelEvent::QWheelEvent() */
    QWheelEvent e (wheelEvent_->pos(), wheelEvent_->globalPos(),
                   totalDelta,
                   wheelEvent_->buttons(), Qt::NoModifier, Qt::Vertical);
    QCoreApplication::sendEvent (verticalScrollBar(), &e);
    if (queuedScrollSteps_.empty())
        scrollTimer_->stop();
}
/*************************/
void TextEdit::resizeEvent (QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent (e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry (QRect (QApplication::layoutDirection() == Qt::RightToLeft ? cr.width() - lineNumberAreaWidth() : cr.left(),
                                        cr.top(), lineNumberAreaWidth(), cr.height()));

    if (resizeTimerId)
    {
        killTimer (resizeTimerId);
        resizeTimerId = 0;
    }
    resizeTimerId = startTimer (UPDATE_INTERVAL);
}
/*************************/
void TextEdit::timerEvent (QTimerEvent *e)
{
    QPlainTextEdit::timerEvent (e);

    if (e->timerId() == resizeTimerId)
    {
        killTimer (e->timerId());
        resizeTimerId = 0;
        emit resized();
    }
    else if (e->timerId() == updateTimerId)
    {
        killTimer (e->timerId());
        updateTimerId = 0;
        /* we use TextEdit's rect because the last rect that
           updateRequest() provides after 50ms may be null */
        emit updateRect (rect(), Dy);
        /* the text-edit may have been invisible before and so,
           FPwin::matchBrackets() may have not be called for it */
        if (!matchedBrackets_ && isVisible())
            emit updateBracketMatching();
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
                opt.setAlignment (Qt::AlignRight);
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

            if (opt.flags() & QTextOption::ShowLineAndParagraphSeparators)
            {
                /* "QTextFormat::FullWidthSelection" isn't respected when new-lines are shown.
                   This is a workaround. */
                QRectF contentsRect = r;
                contentsRect.setWidth (qMax (r.width(), maximumWidth));
                if (contentsRect.contains (cursorRect().center()))
                {
                    contentsRect.setTop (cursorRect().top() - 1);
                    contentsRect.setBottom (cursorRect().bottom() + 1);
                    fillBackground (&painter, contentsRect, lineHColor);
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
                    if (darkValue > -1)
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
                    /* Use alpha with the painter to gray out the paragraph separators and
                       document terminators. The real text will be formatted by the highlgihter. */
                    QColor col;
                    if (darkValue > -1)
                    {
                        col = Qt::white;
                        col.setAlpha (100);
                    }
                    else
                    {
                        col = Qt::black;
                        col.setAlpha (80);
                    }
                    painter.setPen (col);
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
            if (drawIndetLines)
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
                    qreal tabWidth = fm.width (textTab_);
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
                if (darkValue > -1)
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
                qreal rulerSpace = fm.width (' ') * static_cast<qreal>(vLineDistance_);
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
        painter.fillRect (QRect (QPoint (static_cast<int>(er.left()), static_cast<int>(offset.y())), er.bottomRight()), palette().background());
    }
}
/************************************************
***** End of the Workaround for the RTL bug *****
*************************************************/
void TextEdit::highlightCurrentLine()
{
    /* keep yellow and green highlights
       (related to searching and replacing) */
    QList<QTextEdit::ExtraSelection> es = extraSelections();
    if (!es.isEmpty() && !currentLine.cursor.isNull())
        es.removeFirst(); // line highlight always comes first when it exists

    currentLine.format.setBackground (document()->defaultTextOption().flags() & QTextOption::ShowLineAndParagraphSeparators
                                      ? Qt::transparent // workaround for a Qt bug (see TextEdit::paintEvent)
                                      : lineHColor);
    currentLine.format.setProperty (QTextFormat::FullWidthSelection, true);
    currentLine.cursor = textCursor();
    currentLine.cursor.clearSelection();
    es.prepend (currentLine);

    setExtraSelections (es);
}
/*************************/
void TextEdit::lineNumberAreaPaintEvent (QPaintEvent *event)
{
    QPainter painter (lineNumberArea);
    QColor currentBlockFg, currentLineBg, currentLineFg;
    if (darkValue > -1)
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
    int w = lineNumberArea->width();
    int left = rtl ? 3 : 0;

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry (block).translated (contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect (block).height());
    int curBlock = textCursor().blockNumber();
    int h = fontMetrics().height();
    QFont bf = font();
    bf.setBold (true);

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            QString number = QString::number (blockNumber + 1);
            if (blockNumber == curBlock)
            {
                lastCurrentLine = QRect (0, top, 1, top + h);

                painter.save();
                int cur = cursorRect().center().y();
                int i = 0;
                while (top + i * h < cur)
                    ++i;
                if (i > 0) // always the case
                {
                    painter.setFont (bf);
                    painter.setPen (currentLineFg);
                    painter.fillRect (0, top + (i - 1) * h, w, h, currentLineBg);
                    if (i > 1)
                    {
                        painter.drawText (left, top + (i - 1) * h, w - 3, h,
                                          Qt::AlignRight, rtl ? "↲" : "↳");
                        painter.setPen (currentBlockFg);
                        if (i > 2)
                        {
                            painter.drawText (left, top + (i - 2) * h, w - 3, h,
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
    if (updateTimerId)
    {
        killTimer (updateTimerId);
        updateTimerId = 0;
        if (Dy == 0 || dy != 0) // dy can be zero at the end of 50ms
            Dy = dy;
    }
    else Dy = dy;

    updateTimerId = startTimer (UPDATE_INTERVAL);
}
/*************************/
// Bracket matching isn't only based on the signal "cursorPositionChanged()"
// because it isn't emitted when a selected text is removed while the cursor
// is at its start. This function emits an appropriate signal in such cases.
void TextEdit::onSelectionChanged()
{
    QTextCursor cur = textCursor();
    if (!cur.hasSelection())
    {
        if (cur.position() == prevPos && cur.position() < prevAnchor)
            emit updateBracketMatching();
        prevAnchor = prevPos = -1;
    }
    else
    {
        prevAnchor = cur.anchor();
        prevPos = cur.position();
    }
}
/*************************/
void TextEdit::zooming (float range)
{
    QFont f = document()->defaultFont();
    if (range == 0.f) // means unzooming
    {
        setEditorFont (font_, false);
        if (font_.pointSizeF() < f.pointSizeF())
            zoomedOut (this); // ses the explanation below
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
// Since the visible text rectangle is updated by a timer, if the text
// page is first shown for a very short time (when, for example, the
// active tab is changed quickly several times), "updateRect()" might
// be emitted when the text page isn't visible, while "updateRequest()"
// might not be emitted when it becomes visible again. That will result in
// an incomplete syntax highlighting. Therefore, we restart "updateTimerId"
// and give a positive value to "Dy" whenever the text page is shown.
void TextEdit::showEvent (QShowEvent *event)
{
    QPlainTextEdit::showEvent (event);
    if (updateTimerId)
    {
        killTimer (updateTimerId);
        updateTimerId = 0;
    }
    Dy = 1;
    updateTimerId = startTimer (UPDATE_INTERVAL);
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
    lines.sort();
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
/*************************/
void TextEdit::showContextMenu (const QPoint &p)
{
    /* put the cursor at the right-click position if it has no selection */
    if (!textCursor().hasSelection())
        setTextCursor (cursorForPosition (p));

    QMenu *menu = createStandardContextMenu (p);
    const QList<QAction*> actions = menu->actions();
    if (!actions.isEmpty())
    {
        for (QAction* const thisAction : actions)
        { // remove the shortcut strings because shortcuts may change
            QString txt = thisAction->text();
            if (!txt.isEmpty())
                txt = txt.split ('\t').first();
            if (!txt.isEmpty())
                thisAction->setText(txt);
        }
        QString str = getUrl (textCursor().position());
        if (!str.isEmpty())
        {
            QAction *sep = menu->insertSeparator (actions.first());
            QAction *openLink = new QAction (tr ("Open Link"), menu);
            menu->insertAction (sep, openLink);
            connect (openLink, &QAction::triggered, [str] {
                QUrl url (str);
                if (url.isRelative())
                    url = QUrl::fromUserInput (str, "/");
                /* QDesktopServices::openUrl() may resort to "xdg-open", which isn't
                   the best choice. "gio" is always reliable, so we check it first. */
                if (!QProcess::startDetached ("gio", QStringList() << "open" << url.toString()))
                    QDesktopServices::openUrl (url);
            });
            if (str.startsWith ("mailto:")) // see getUrl()
                str.remove (0, 7);
            QAction *copyLink = new QAction (tr ("Copy Link"), menu);
            menu->insertAction (sep, copyLink);
            connect (copyLink, &QAction::triggered, [str] {
                QApplication::clipboard()->setText (str);
            });

        }
        menu->addSeparator();
    }
    if (!isReadOnly())
    {
        if (textCursor().hasSelection())
        {
            QAction *action = menu->addAction (tr ("To Upper Case"));
            connect (action, &QAction::triggered, [this] {
                insertPlainText (locale().toUpper (textCursor().selectedText()));
            });
            action = menu->addAction (tr ("To Lower Case"));
            connect (action, &QAction::triggered, [this] {
                insertPlainText (locale().toLower (textCursor().selectedText()));
            });
            if (textCursor().selectedText().contains (QChar (QChar::ParagraphSeparator)))
            {
                menu->addSeparator();
                action = menu->addAction (tr ("Sort Lines"));
                connect (action, &QAction::triggered, [this] {
                    sortLines();
                });
                action = menu->addAction (tr ("Sort Lines Reversely"));
                connect (action, &QAction::triggered, [this] {
                    sortLines (true);
                });
            }
            menu->addSeparator();
        }
        QAction *action = menu->addAction (tr ("Paste Date and Time"));
        connect (action, &QAction::triggered, [this] {
            insertPlainText (QDateTime::currentDateTime().toString (dateFormat_.isEmpty() ? "MMM dd, yyyy, hh:mm:ss" : dateFormat_));
        });
    }
    menu->exec (viewport()->mapToGlobal (p));
    delete menu;
}

/*****************************************************
***** The following functions are for hyperlinks *****
******************************************************/

QString TextEdit::getUrl (const int pos) const
{
    static const QRegularExpression urlPattern ("[A-Za-z0-9_]+://((?!&quot;|&gt;|&lt;)[A-Za-z0-9_.+/\\?\\=~&%#\\-:\\(\\)\\[\\]])+(?<!\\.|\\?|:|\\(|\\)|\\[|\\])|([A-Za-z0-9_.\\-]+@[A-Za-z0-9_\\-]+\\.[A-Za-z0-9.]+)+(?<!\\.)");

    QString url;
    QTextBlock block = document()->findBlock (pos);
    QString text = block.text();
    if (text.length() <= 50000) // otherwise, too long
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
    QPlainTextEdit::mousePressEvent (event);
    if (highlighter_
        && (event->button() & Qt::LeftButton)
        && (qApp->keyboardModifiers() & Qt::ControlModifier))
    {
        pressPoint_ = event->pos();
    }
}
/*************************/
void TextEdit::mouseReleaseEvent (QMouseEvent *event)
{
    QPlainTextEdit::mouseReleaseEvent (event);
    if (!highlighter_
        || !(event->button() & Qt::LeftButton)
        || !(qApp->keyboardModifiers() & Qt::ControlModifier)
        /* another key may also be pressed (-> keyPressEvent) */
        || viewport()->cursor().shape() != Qt::PointingHandCursor)
    {
        return;
    }

    QTextCursor cur = cursorForPosition (event->pos());
    QString str = getUrl (cur.position());
    if (!str.isEmpty() && cur == cursorForPosition (pressPoint_))
    {
        QUrl url (str);
        if (url.isRelative()) // treat relative URLs as local paths (not needed here)
            url = QUrl::fromUserInput (str, "/");
        if (!QProcess::startDetached ("gio", QStringList() << "open" << url.toString()))
            QDesktopServices::openUrl (url);
    }
    pressPoint_ = QPoint();
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

}
