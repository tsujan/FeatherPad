/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2020 <tsujan2000@gmail.com>
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

#include "highlighter.h"

namespace FeatherPad {

/* NOTE: Comments can be everywhere, inside and outside CSS blocks/values,
         but a start comment sign may be escaped by a quotation or URL inside
         a CSS value. Therefore, to know whether a position is commented out,
         we need to know whether it is inside a value or not and, for that,
         we have to divide the text line to regions of CSS values. */

static inline int getSectionStart (const int pos, const QList<int> &regions, bool *isInsideRegion)
{
    if (regions.isEmpty()
        || regions.contains (pos)
        || pos < regions.first())
    {
        *isInsideRegion = false;
        return 0;
    }
    for (int i = 0; i < regions.size() - 1; ++i)
    {
        if (pos > regions.at (i) && pos < regions.at (i + 1))
        {
            *isInsideRegion = (i % 2 == 0);
            return regions.at (i);
        }
    }
    *isInsideRegion = (regions.size() % 2 != 0);
    return regions.last();
}

bool Highlighter::isCSSCommented (const QString &text,
                                  const QList<int> &valueRegions,
                                  const int index,
                                  int prevQuote,
                                  bool prevUrl)
{
    if (index < 0)  return false;

    bool insideValue;
    int start = getSectionStart (index, valueRegions, &insideValue);

    bool res = false;
    int pos = - 1;
    int N;
    QRegularExpression commentExpression;
    int prevState = previousBlockState();
    if (start > 0
        || (prevState != commentState
            && prevState != commentInCssBlockState
            && prevState != commentInCssValueState))
    {
        N = 0;
        pos = start - 1;
        commentExpression = commentStartExpression;
    }
    else
    {
        N = 1;
        res = true;
        commentExpression = commentEndExpression;
    }

    while ((pos = text.indexOf (commentExpression, pos + 1)) >= 0)
    {
        /* skip formatted quotations and URLs (in values) */
        if (format (pos) == quoteFormat || format (pos) == altQuoteFormat)
            continue;

        ++N;
        if (pos >= index)
        {
            if (N % 2 == 0) res = true;
            else res = false;
            break;
        }

        if (N % 2 != 0) // a start comment...
        {
            /* ... inside an attribute selector... */
            if ((!insideValue && isInsideAttrSelector (text, start, pos))
                 /* ... or inside a quotation or URL */
                 || (insideValue  // quotations and URLs exist only inside values
                     && (isQuotedInCSSValue (text, start, pos, prevQuote, prevUrl) > 0
                         || isInsideCSSValueUrl (text, start, pos, prevQuote, prevUrl))))
            {
                --N;
                continue;
            }

            commentExpression = commentEndExpression;
            res = true;
        }
        else
        {
            commentExpression = commentStartExpression;
            res = false;
        }
    }

    return res;
}
/*************************/
// Also formats quotations.
int Highlighter::isQuotedInCSSValue (const QString &text,
                                     const int valueStart,
                                     const int index,
                                     int prevQuote,
                                     bool prevUrl)
{
    if (index < 0 || valueStart < 0 || index < valueStart)
        return 0;
    //if (format (index) == quoteFormat)
        //return ?;
    if (format (index) == altQuoteFormat)
        return 0;

    int res; // 1 for single quote, 2 for double quote
    int N;
    QRegularExpression quoteExpression;
    if (prevQuote > 0 && valueStart == 0)
    {
        if (prevQuote == 1)
        {
            res = 1;
            quoteExpression = singleQuoteMark;
        }
        else
        {
            res = 2;
            quoteExpression = quoteMark;
        }
        N = 1;
    }
    else
    {
        quoteExpression = mixedQuoteMark;
        res = 0;
        N = 0;
    }
    int pos = valueStart - 1;
    int start = 0;
    int nxtPos;
    while ((nxtPos = text.indexOf (quoteExpression, pos + 1)) >= 0)
    {
        ++N;
        if (nxtPos >= index)
        {
            if (N % 2 == 0)
            {
                if (text.at (nxtPos) == quoteMark.pattern().at (0))
                    res = 2;
                else
                    res = 1;
                setFormat (start, nxtPos - start + 1, quoteFormat);
            }
            else res = 0;
            break;
        }

        if (N % 2 != 0)
        {
            /* a start quote inside a comment or value URL */
            if (isCSSCommented (text, QList<int>() << valueStart, nxtPos, prevQuote, prevUrl)
                || isInsideCSSValueUrl (text, valueStart, nxtPos, prevQuote, prevUrl))
            {
                --N;
                pos = nxtPos;
                continue;
            }

            if (text.at (nxtPos) == quoteMark.pattern().at (0))
                res = 2;
            else
                res = 1;
            start = nxtPos;
        }
        else
        {
            res = 0;
            setFormat (start, nxtPos - start + 1, quoteFormat);
        }

        /* determine the next quotation mark */
        if (N % 2 != 0)
        {
            if (text.at (nxtPos) == quoteMark.pattern().at (0))
                quoteExpression = quoteMark;
            else
                quoteExpression = singleQuoteMark;
        }
        else
            quoteExpression = mixedQuoteMark;

        pos = nxtPos;
    }
    if (nxtPos == -1 && N % 2 != 0)
        setFormat (start, text.length() - start, quoteFormat);

    return res;
}
/*************************/
/* FIXME: This is temporary solution for url("...") and url('...')
          and only works with whole URLs in a line. */
static inline bool isWholeCSSdUrl (const QString &text, const int start, int &length)
{
    int indx = start + 4; // "url("
    while (indx < start + length && text.at (indx).isSpace())
        ++ indx;
    if (indx < start + length)
    {
        if (text.at (indx) == '\'')
        {
            int end = text.indexOf ('\'', indx + 1);
            if (end == -1) return false;
            if (end >= start + length)
            {
                end = text.indexOf (')', end + 1);
                if (end == -1) return false;
                length = end - start + 1;
            }
        }
        else if (text.at (indx) == '\"')
        {
            int end = text.indexOf ('\"', indx + 1);
            if (end == -1) return false;
            if (end >= start + length)
            {
                end = text.indexOf (')', end + 1);
                if (end == -1) return false;
                length = end - start + 1;
            }
        }
    }
    return true;
}

// Also formats URLs.
bool Highlighter::isInsideCSSValueUrl (const QString &text,
                                       const int valueStart,
                                       const int index,
                                       int prevQuote,
                                       bool prevUrl)
{
    if (index < 0 || valueStart < 0 || index < valueStart)
        return false;
    if (format (index) == altQuoteFormat)
        return true;
    if (format (index) == quoteFormat)
        return false;

    int indx;
    if (valueStart == 0 && prevUrl) // prevQuote is 0
    {
        /* format the first URL completely */
        indx = text.indexOf (QRegularExpression ("\\)"));
        int endIndx;
        if (indx == -1) endIndx = text.length();
        else endIndx = indx + 1;
        setFormat (0, endIndx, altQuoteFormat);

        if (indx == -1 || indx >= index)
            return true;
        return isInsideCSSValueUrl (text, indx + 1, index);
    }

    /* format whole URLs up to index */
    QRegularExpressionMatch match;
    static const QRegularExpression cssUrl ("\\burl\\([^\\)]*\\)");
    int urlIndx = text.indexOf (cssUrl, valueStart, &match);
    while (urlIndx < index
           && (isCSSCommented (text, QList<int>() << valueStart, urlIndx, prevQuote, prevUrl)
               || isQuotedInCSSValue (text, valueStart, urlIndx, prevQuote, prevUrl) > 0))
    {
        urlIndx = text.indexOf (cssUrl, urlIndx + 1, &match);
    }
    int L;
    while (urlIndx > -1 && urlIndx < index)
    {
        L = match.capturedLength();
        if (!isWholeCSSdUrl (text, urlIndx, L))
        {
            setFormat (urlIndx, text.length() - urlIndx, altQuoteFormat);
            return true;
        }
        setFormat (urlIndx, L, altQuoteFormat);
        if (urlIndx + L > index)
            return true;
        if (urlIndx + L == index)
            return false;
        urlIndx = text.indexOf (cssUrl, urlIndx + L, &match);
        while (urlIndx < index
               && (isCSSCommented (text, QList<int>() << valueStart, urlIndx, prevQuote, prevUrl)
                   || isQuotedInCSSValue (text, valueStart, urlIndx, prevQuote, prevUrl) > 0))
        {
            urlIndx = text.indexOf (cssUrl, urlIndx + 1, &match);
        }
    }

    static const QRegularExpression cssOpenUrl ("\\burl\\([^\\)]*$");
    const QString txt = text.left (index);
    indx = txt.indexOf (cssOpenUrl, valueStart);
    while (isCSSCommented (text, QList<int>() << valueStart, indx, prevQuote, prevUrl)
           || isQuotedInCSSValue (text, valueStart, indx, prevQuote, prevUrl) > 0)
    {
        indx = txt.indexOf (cssOpenUrl, indx + 1);
    }
    if (indx == -1) return false;

    /* also, format this URL completely if it's open */
    if (text.indexOf (')', indx) == -1)
        setFormat (indx, text.length() - indx, altQuoteFormat);

    return true;
}
/*************************/
// This formats attribute selectors with "quoteFormat" to skip start
// comment sings in multiLineComment().
// It is supposed that the section between "start" and "pos" is
// outside all values (because values can't contain attribute selectors).
void Highlighter::formatAttrSelectors (const QString &text, const int start , const int pos)
{
    if (pos <= start) return;
    static const QRegularExpression attrSelector ("\\[[^\\]]*(?=\\]|$)");
    QRegularExpressionMatch match;
    int indx = start;
    while ((indx = text.indexOf (attrSelector, indx, &match)) > -1 && indx <= pos)
    {
        if (format (indx+1) == quoteFormat) // a precaution (shouldn't be needed)
            return;
        while (isCSSCommented (text, QList<int>() << start << start, indx))
            indx = text.indexOf (attrSelector, indx + 1, &match);
        if (indx == -1) break;
        setFormat (indx + 1, match.capturedLength() - 1, quoteFormat);
        indx += match.capturedLength();
    }
}
/*************************/
// Multi-line attribute selectors aren't considered.
// It is supposed that the section between "start" and "pos" is
// outside all values (because values can't contain attribute selectors).
bool Highlighter::isInsideAttrSelector (const QString &text, const int start, const int pos)
{
    if (pos <= start) return false;
    if (format (pos) == quoteFormat) return true;
    static const QRegularExpression attrSelectorStart ("\\[[^\\]]*$");
    int indx = text.left (pos).indexOf (attrSelectorStart, start);
    if (indx > -1 && !isCSSCommented (text, QList<int>() << start << start, indx))
        return true;
    return false;
}
/*************************/
// This should come before multiline comments highlighting.
// It also highlights quotes and URLs inside CSS values.
void Highlighter::cssHighlighter (const QString &text, bool mainFormatting, const int start)
{
    /* NOTE: Since we need to know whether the previous value had an open quote or an
             open URL, we use the "OpenNests" variable to not add another one just for
             this case. Although it isn't intended for such a case, it can be safely
             used with values "1", "2" and "3" because it isn't used anywhere else
             with CSS or HTML. The next block will be rehighlighted at highlightBlock()
             (after "cssHighlighter (text, mainFormatting);") if it's changed. */

    /**************************
     * (Multiline) CSS Blocks *
     **************************/

    QRegularExpressionMatch cssStartMatch;
    QRegularExpression cssStartExpression ("\\{");
    QRegularExpressionMatch cssEndtMatch;
    QRegularExpression cssEndExpression ("\\}");

    /* it's supposed that a property can only contain letters, numbers, underlines and dashes */
    static const QRegularExpression cssValueStartExp ("(?<=^|\\{|;|\\s)[A-Za-z0-9_\\-]+\\s*:(?!:)");
    static const QRegularExpression cssValueEndExp (";|\\}");

    int blockStartIndex = start;
    int blockEndIndex = start;

    int valueStartIndex;
    int valueEndIndex;

    QList<int> valueRegions;

    QTextCharFormat cssValueFormat;
    cssValueFormat.setFontItalic (true);
    cssValueFormat.setForeground (Verda);


    int prevQuote = 0;
    bool prevUrl = false;
    QTextBlock prevBlock = currentBlock().previous();
    if (prevBlock.isValid())
    {
        if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
        {
            if (prevData->openNests() == 1)
                prevQuote = 1; // single quote
            else if (prevData->openNests() == 2)
                prevQuote = 2; // double quote
            else if (prevData->openNests() == 3)
                prevUrl = true;
        }
    }

    int prevState = previousBlockState();
    if (blockStartIndex > 0
        || (prevState != cssBlockState
            && prevState != cssValueState
            && prevState != commentInCssBlockState
            && prevState != commentInCssValueState))
    {
        blockStartIndex = text.indexOf (cssStartExpression, start, &cssStartMatch);
        while (isCSSCommented (text, QList<int>() << start << start, blockStartIndex)
               || isInsideAttrSelector (text, start, blockStartIndex))
        {
            blockStartIndex = text.indexOf (cssStartExpression, blockStartIndex + 1, &cssStartMatch);
        }
        if (blockStartIndex == -1)
            formatAttrSelectors (text, start, text.length());
    }

    while (blockStartIndex >= 0)
    {
        formatAttrSelectors (text, blockEndIndex, blockStartIndex);

        int realBlockStart;
        /* when the css block starts in the previous line
           and the search for its end has just begun... */
        if ((prevState == cssBlockState
             || prevState == cssValueState // subset of cssBlockState
             || prevState == commentInCssBlockState
             || prevState == commentInCssValueState)
            && blockStartIndex == 0)
        {
            /* ... search for its end from the line start */
            realBlockStart = 0;
        }
        else
            realBlockStart = blockStartIndex + cssStartMatch.capturedLength();
        blockEndIndex = text.indexOf (cssEndExpression, realBlockStart, &cssEndtMatch);

        if (blockEndIndex == -1)
        {
            /* temporarily, to find value regions and format
               their quotes and URLs in the following loop */
            blockEndIndex = text.length();
        }

        /* see if the end brace should be skipped after
           finding CSS value regions up to this point */
        QList<int> regions;
        valueStartIndex = realBlockStart;
        QRegularExpressionMatch match;
        if (realBlockStart > 0
            || (valueStartIndex == 0
                && prevState != cssValueState
                && prevState != commentInCssValueState))
        {
            valueStartIndex = text.indexOf (cssValueStartExp, realBlockStart, &match);
            while (isCSSCommented (text, QList<int>() << realBlockStart << realBlockStart, valueStartIndex)
                   || isInsideAttrSelector (text, realBlockStart, valueStartIndex))
            {
                valueStartIndex = text.indexOf (cssValueStartExp, valueStartIndex + match.capturedLength(), &match);
            }
            if (valueStartIndex > -1 && valueStartIndex <= blockEndIndex)
                formatAttrSelectors (text, realBlockStart, valueStartIndex);
        }
        while (blockEndIndex > -1)
        {
            while (valueStartIndex > -1 && valueStartIndex <= blockEndIndex)
            {
                /**************************
                 * (Multiline) CSS Values *
                 **************************/

                valueStartIndex += match.capturedLength();
                regions << valueStartIndex;
                if (valueStartIndex == 0
                    && (prevState == cssValueState
                        || prevState == commentInCssValueState))
                { // first loop
                    valueEndIndex = text.indexOf (cssValueEndExp, 0, &cssEndtMatch);
                    while (valueEndIndex > -1
                           && (isCSSCommented (text, QList<int>() << 0, valueEndIndex, prevQuote, prevUrl)
                               || isQuotedInCSSValue (text, 0, valueEndIndex, prevQuote, prevUrl) > 0
                               || isInsideCSSValueUrl (text, 0, valueEndIndex, prevQuote, prevUrl)))
                    {
                        valueEndIndex = text.indexOf (cssValueEndExp, valueEndIndex + cssEndtMatch.capturedLength(), &cssEndtMatch);
                    }
                }
                else
                {
                    valueEndIndex = text.indexOf (cssValueEndExp, valueStartIndex, &cssEndtMatch);
                    while (valueEndIndex > -1
                           && (isCSSCommented (text, QList<int>() << valueStartIndex, valueEndIndex)
                               || isQuotedInCSSValue (text, valueStartIndex, valueEndIndex) > 0
                               || isInsideCSSValueUrl (text, valueStartIndex, valueEndIndex)))
                    {
                        valueEndIndex = text.indexOf (cssValueEndExp, valueEndIndex + cssEndtMatch.capturedLength(), &cssEndtMatch);
                    }
                }

                if (valueEndIndex > -1)
                {
                    regions << valueEndIndex;
                    valueStartIndex = text.indexOf (cssValueStartExp, valueEndIndex + cssEndtMatch.capturedLength(), &match);
                    while (isCSSCommented (text, QList<int>() << valueEndIndex << valueEndIndex, valueStartIndex)
                           || isInsideAttrSelector (text, valueEndIndex, valueStartIndex))
                    {
                        valueStartIndex = text.indexOf (cssValueStartExp, valueStartIndex + match.capturedLength(), &match);
                    }
                    if (valueStartIndex > -1 && valueStartIndex <= blockEndIndex)
                        formatAttrSelectors (text, valueEndIndex, valueStartIndex);
                }
                else
                {
                    int q = isQuotedInCSSValue (text, valueStartIndex, text.length(), prevQuote, prevUrl);
                    if (q > 0)
                    {
                        if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
                            data->insertNestInfo (q);
                    }
                    else if (isInsideCSSValueUrl (text, valueStartIndex, text.length(), prevQuote, prevUrl))
                    {
                        if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
                            data->insertNestInfo (3);
                    }
                    valueStartIndex = -1; // exit the loop
                }
            }

            if (blockEndIndex == text.length())
            { // value regions are found and their quotes and URLs are formatted
                blockEndIndex = -1;
                break;
            }
            if (regions.size() % 2 != 0 // quoted or inside a URL
                || (!regions.isEmpty() && (regions.last() > blockEndIndex + 1 // quoted or inside a URL
                                           || isCSSCommented (text, regions, blockEndIndex, prevQuote, prevUrl
                                           || isInsideAttrSelector (text, regions.last(), blockEndIndex))))
                || (regions.isEmpty() && (isCSSCommented (text, QList<int>() << realBlockStart << realBlockStart, blockEndIndex)
                                          || isInsideAttrSelector (text, realBlockStart, blockEndIndex))))
            {
                blockEndIndex = text.indexOf (cssEndExpression, blockEndIndex + 1, &cssEndtMatch);
                if (blockEndIndex == -1 && valueStartIndex > -1)
                    blockEndIndex = text.length();  // to find the remaining value regions and format their quotes and URLs
            }
            else break;
        }

        if (mainFormatting)
            valueRegions << regions;

        int cssLength;
        if (blockEndIndex == -1)
        {
            blockEndIndex = text.length();
            if (regions.size() % 2 != 0)
                setCurrentBlockState (cssValueState);
            else
                setCurrentBlockState (cssBlockState);
            cssLength = text.length() - blockStartIndex;
        }
        else
        {
            cssLength = blockEndIndex - blockStartIndex
                        + cssEndtMatch.capturedLength();
        }

        if (regions.isEmpty())
            formatAttrSelectors (text, realBlockStart, blockEndIndex);
        else if (regions.size() % 2 == 0)
            formatAttrSelectors (text, regions.last(), blockEndIndex);

        /* at first, we suppose all syntax is wrong (but comment
           start signs have "quoteFormat" inside attribute selectors) */
        setFormatWithoutOverwrite (realBlockStart, blockEndIndex - realBlockStart, neutralFormat, quoteFormat);

        if (mainFormatting)
        {
            /* css property format (before :...;) */
            QTextCharFormat cssPropFormat;
            cssPropFormat.setFontItalic (true);
            cssPropFormat.setForeground (Blue);
            static const QRegularExpression cssProp ("(?<=^|\\{|;|\\s)[A-Za-z0-9_\\-]+(?=\\s*(?<!:):(?!:))");
            int indxTmp = text.indexOf (cssProp, realBlockStart, &match);
            while (format (indxTmp) == quoteFormat || format (indxTmp) == altQuoteFormat)
                indxTmp = text.indexOf (cssProp, indxTmp + match.capturedLength(), &match);
            while (indxTmp >= 0 && indxTmp < blockEndIndex)
            {
                setFormat (indxTmp, match.capturedLength(), cssPropFormat);
                indxTmp = text.indexOf (cssProp, indxTmp + match.capturedLength(), &match);
                while (format (indxTmp) == quoteFormat || format (indxTmp) == altQuoteFormat)
                    indxTmp = text.indexOf (cssProp, indxTmp + match.capturedLength(), &match);
            }
        }

        blockStartIndex = text.indexOf (cssStartExpression, blockStartIndex + cssLength, &cssStartMatch);
        while (isCSSCommented (text, QList<int>() << blockEndIndex << blockEndIndex, blockStartIndex)
               || isInsideAttrSelector (text, blockEndIndex, blockStartIndex))
        {
            blockStartIndex = text.indexOf (cssStartExpression, blockStartIndex + 1, &cssStartMatch);
        }

        if (blockStartIndex == -1)
            formatAttrSelectors (text, blockEndIndex, text.length());
    }

    /*******************
     * Main Formatting *
     *******************/

    if (mainFormatting)
    {
        for (int i = 0; i < valueRegions.size(); ++i)
        {
            int cssLength;
            if (i % 2 != 0)
            {
                valueStartIndex = valueRegions.at (i - 1);
                cssLength = valueRegions.at (i) - valueRegions.at (i - 1);
            }
            else
            {
                if (i == valueRegions.size() - 1)
                {
                    valueStartIndex = valueRegions.at (i);
                    cssLength = text.length() - valueRegions.at (i);
                }
                else continue;
            }

            /* css value format (skips altQuoteFormat too) */
            setFormatWithoutOverwrite (valueStartIndex, cssLength, cssValueFormat, quoteFormat);

            /* numbers in css values */
            QTextCharFormat numFormat;
            numFormat.setFontItalic (true);
            numFormat.setForeground (Brown);
            QRegularExpressionMatch numMatch;
            QRegularExpression numExpression ("(-|\\+){0,1}\\b\\d*\\.{0,1}\\d+");
            int nIndex = text.indexOf (numExpression, valueStartIndex, &numMatch);
            while (format (nIndex) == quoteFormat || format (nIndex) == altQuoteFormat)
                nIndex = text.indexOf (numExpression, nIndex + numMatch.capturedLength(), &numMatch);
            while (nIndex > -1
                   && nIndex + numMatch.capturedLength() <= valueStartIndex + cssLength)
            {
                setFormat (nIndex, numMatch.capturedLength(), numFormat);
                nIndex = text.indexOf (numExpression, nIndex + numMatch.capturedLength(), &numMatch);
                while (format (nIndex) == quoteFormat || format (nIndex) == altQuoteFormat)
                    nIndex = text.indexOf (numExpression, nIndex + numMatch.capturedLength(), &numMatch);
            }
        }

        /* color value format (#xyz, #abcdef, #abcdefxy) */
        QTextCharFormat cssColorFormat;
        cssColorFormat.setForeground (Verda);
        cssColorFormat.setFontWeight (QFont::Bold);
        cssColorFormat.setFontItalic (true);
        QRegularExpressionMatch match;
        // previously: "#\\b([A-Za-z0-9]{3}){0,4}(?![A-Za-z0-9_]+)"
        static const QRegularExpression colorValue ("#([A-Fa-f0-9]{3}){1,2}(?![A-Za-z0-9_]+)|#([A-Fa-f0-9]{3}){2}[A-Fa-f0-9]{2}(?![A-Za-z0-9_]+)");
        int indxTmp = text.indexOf (colorValue, start, &match);
        while (format (indxTmp) == neutralFormat // an error
               || format (indxTmp) == quoteFormat || format (indxTmp) == altQuoteFormat
               || isCSSCommented (text, valueRegions, indxTmp))
        {
            indxTmp = text.indexOf (colorValue, indxTmp + match.capturedLength(), &match);
        }
        while (indxTmp >= 0)
        {
            setFormat (indxTmp, match.capturedLength(), cssColorFormat);
            indxTmp = text.indexOf (colorValue, indxTmp + match.capturedLength(), &match);
            while (format (indxTmp) == neutralFormat
                   || format (indxTmp) == quoteFormat || format (indxTmp) == altQuoteFormat
                   || isCSSCommented (text, valueRegions, indxTmp))
            {
                indxTmp = text.indexOf (colorValue, indxTmp + match.capturedLength(), &match);
            }
        }

        /* definitions (starting with @) */
        QTextCharFormat cssDefinitionFormat;
        cssDefinitionFormat.setForeground (Brown);
        static const  QRegularExpression cssDef ("(@[\\w-]+\\b)([^;]*(;|$))");
        indxTmp = text.indexOf (cssDef, start, &match);
        while (format (indxTmp) == neutralFormat // an error
               || format (indxTmp) == cssValueFormat // inside a value
               || format (indxTmp) == quoteFormat || format (indxTmp) == altQuoteFormat
               || isCSSCommented (text, valueRegions, indxTmp))
        {
            indxTmp = text.indexOf (cssDef, indxTmp + match.capturedLength (1), &match);
        }
        while (indxTmp >= 0)
        {
            setFormat (indxTmp, match.capturedLength (1), cssDefinitionFormat);
            indxTmp = text.indexOf (cssDef, indxTmp + match.capturedLength(), &match);
            while (format (indxTmp) == neutralFormat
                   || format (indxTmp) == cssValueFormat
                   || format (indxTmp) == quoteFormat || format (indxTmp) == altQuoteFormat
                   || isCSSCommented (text, valueRegions, indxTmp))
            {
                indxTmp = text.indexOf (cssDef, indxTmp + match.capturedLength (1), &match);
            }
        }
    }
}

}
