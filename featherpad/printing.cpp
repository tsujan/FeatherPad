/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2020-2024 <tsujan2000@gmail.com>
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

#include "printing.h"
#include <QPainter>
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>

#include <algorithm>
#include <cmath>

namespace FeatherPad {

Printing::Printing (QTextDocument *document, const QString &fileName,
                    const QColor &textColor, int darkValue,
                    double sourceDpiX, double sourceDpiY)
{
    origDoc_ = document;
    clonedDoc_ = nullptr;

    textColor_ = textColor;
    darkValue_ = darkValue;
    sourceDpiX_ = sourceDpiX;
    sourceDpiY_ = sourceDpiY;
    if (darkValue > -1)
        darkColor_ = QColor (darkValue, darkValue, darkValue);

    printer_ = new QPrinter (QPrinter::HighResolution);
    if (printer_->outputFormat() == QPrinter::PdfFormat)
        printer_->setOutputFileName (fileName);
}
/*************************/
Printing::~Printing()
{
    if (clonedDoc_ != nullptr)
        clonedDoc_->deleteLater();
    delete printer_;
}
/*************************/
static void printPage (int index, QPainter *painter, const QTextDocument *doc,
                       const QRectF &body, const QPointF &pageNumberPos,
                       const QColor &textColor, const QColor &darkColor)
{
    painter->save();
    painter->translate (body.left(), body.top() - (index - 1) * body.height());
    QRectF view (0, (index - 1) * body.height(), body.width(), body.height());

    QAbstractTextDocumentLayout *layout = doc->documentLayout();
    QAbstractTextDocumentLayout::PaintContext ctx;

    painter->setClipRect (view);

    if (darkColor.isValid()) // make the page dark with the dark scheme
        painter->fillRect (view, darkColor);

    ctx.clip = view;
    /* with syntax highlighting, this is the color of
       line/document ends because the ordinary text is formatted */
    ctx.palette.setColor (QPalette::Text, textColor);
    layout->draw (painter, ctx);

    if (!pageNumberPos.isNull())
    {
        if (darkColor.isValid())
            painter->setPen (Qt::white);
        painter->setClipping (false);
        painter->setFont (QFont (doc->defaultFont()));
        const QString pageString = QString::number (index);

        painter->drawText(std::round (pageNumberPos.x() - painter->fontMetrics().horizontalAdvance (pageString) / 2),
                          std::round (pageNumberPos.y() + view.top()),
                          pageString);
    }

    painter->restore();
}

// Control printing to make it work fine with the dark color scheme and also
// to enable printing in the reverse order. This is adapted from "QTextDocument::print".
void Printing::run()
{
    if (!origDoc_) return;

    QPainter p (printer_);
    if (!p.isActive()) return;

    if (printer_->pageLayout().paintRectPixels (printer_->resolution()).isEmpty())
        return;

    /* clone the document and move it to the printing thread */
    clonedDoc_ = origDoc_->clone();
    clonedDoc_->moveToThread (QThread::currentThread());
    /* ensure that there's a layout
       (QTextDocument::documentLayout will create the standard layout if there's none) */
    (void)clonedDoc_->documentLayout();
    /* copy the original formats, considering the dark background */
    for (QTextBlock srcBlock = origDoc_->firstBlock(), dstBlock = clonedDoc_->firstBlock();
         srcBlock.isValid() && dstBlock.isValid();
         srcBlock = srcBlock.next(), dstBlock = dstBlock.next())
    {
        QList<QTextLayout::FormatRange> formatList = srcBlock.layout()->formats();
        if (darkValue_ > -1)
        {
            for (int i = formatList.count() - 1; i >= 0; --i)
            {
                QTextCharFormat &format = formatList[i].format;
                format.setBackground (darkColor_);
            }
        }
        dstBlock.layout()->setFormats (formatList);
    }

    clonedDoc_->documentLayout()->setPaintDevice (p.device());

    const double dpiScaleY = static_cast<double>(printer_->logicalDpiY()) / sourceDpiY_;

    const int horizontalMargin = static_cast<int>((2/2.54) * sourceDpiX_);
    const int verticalMargin = static_cast<int>((2/2.54) * sourceDpiY_);
    QTextFrameFormat fmt = clonedDoc_->rootFrame()->frameFormat();
    fmt.setLeftMargin (horizontalMargin);
    fmt.setRightMargin (horizontalMargin);
    fmt.setTopMargin (verticalMargin);
    fmt.setBottomMargin (verticalMargin);
    clonedDoc_->rootFrame()->setFrameFormat (fmt);

    const int dpiy = p.device()->logicalDpiY();
    QRectF body (0, 0, printer_->width(), printer_->height());
    QPointF pageNumberPos (body.width() / 2,
                           body.height() - verticalMargin * dpiScaleY
                               + QFontMetrics (clonedDoc_->defaultFont(), p.device()).ascent()
                               + 5 * dpiy / 72.0);
    clonedDoc_->setPageSize (body.size());

    int fromPage = printer_->fromPage();
    int toPage = printer_->toPage();

    if (fromPage == 0 && toPage == 0)
    {
        fromPage = 1;
        toPage = clonedDoc_->pageCount();
    }

    fromPage = std::max (1, fromPage);
    toPage = std::min (clonedDoc_->pageCount(), toPage);

    if (toPage < fromPage) return;

    bool reverse (printer_->pageOrder() == QPrinter::LastPageFirst);
    int page = reverse ? toPage : fromPage;
    while (true)
    {
        printPage (page, &p, clonedDoc_, body, pageNumberPos, textColor_, darkColor_);
        if (reverse)
        {
            if (page == fromPage) break;
            --page;
        }
        else
        {
            if (page == toPage) break;
            ++page;
        }
        if (!printer_->newPage())
            return;
    }
}

}
