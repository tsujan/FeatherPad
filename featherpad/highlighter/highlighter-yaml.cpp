/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2021-2024 <tsujan2000@gmail.com>
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

#include <algorithm>

namespace FeatherPad {

// Check whether the start bracket/brace is escaped. FIXME: This only covers keys and values.
static inline bool isYamlBraceEscaped (const QString &text, const QRegularExpression &start, int pos)
{
    if (pos < 0 || text.indexOf (start, pos) != pos)
        return false;
    int indx = text.lastIndexOf (QRegularExpression ("(^|{|,|\\[)?[^:#]*:\\s+\\K"), pos); // the last key
    if (indx > -1)
    {
        QString txt = text.right (text.size() - indx);
        if (txt.indexOf (QRegularExpression ("^[^{\\[#\\s]")) > -1) // inside value
            return true;
    }
    QRegularExpressionMatch match;
    indx = text.lastIndexOf (QRegularExpression ("[^:#\\s{\\[]+\\s*" + start.pattern() + "[^:#]*:\\s+"), pos, &match);
    if (indx > -1 && indx < pos && indx + match.capturedLength() > pos) // inside key
        return true;
    return false;
}

// Process open braces or brackets, apply the neutral format appropriately
// and return a boolean that shows whether the next line should be updated.
// Single-line comments should have already been highlighted.
bool Highlighter::yamlOpenBraces (const QString &text,
                                  const QRegularExpression &startExp, const QRegularExpression &endExp,
                                  int oldOpenNests, bool oldProperty, // old info on the current line
                                  bool setData) // whether data should be set
{
    TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData());
    if (!data) return false;

    int openNests = 0;
    bool property = startExp.pattern() == "{" ? false : true;

    if (setData)
    {
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid())
        {
            if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
            {
                if (prevData->getProperty() == property)
                    openNests = prevData->openNests();
            }
        }
    }

    QRegularExpression mixed (startExp.pattern() + "|" + endExp.pattern());
    int indx = -1, startIndx = 0;
    int txtL = text.length();
    QRegularExpressionMatch match;
    while (indx < txtL)
    {
        indx = text.indexOf (mixed, indx + 1, &match);
        while (isYamlBraceEscaped (text, startExp, indx) || isQuoted (text, indx))
            indx = text.indexOf (mixed, indx + match.capturedLength(), &match);
        if (format (indx) == commentFormat)
        {
            while (format (indx - 1) == commentFormat) --indx;
            if (indx > startIndx && openNests > 0)
                setFormat (startIndx, indx - startIndx, neutralFormat);
            break;
        }
        if (indx > -1)
        {
            if (text.indexOf (startExp, indx) == indx)
            {
                if (openNests == 0)
                    startIndx = indx;
                ++ openNests;
            }
            else
            {
                -- openNests;
                if (openNests == 0)
                    setFormat (startIndx, indx + match.capturedLength() - startIndx, neutralFormat);
                openNests = std::max (openNests, 0);
            }
        }
        else
        {
            if (openNests > 0)
            {
                indx = txtL;
                while (format (indx - 1) == commentFormat) --indx;
                if (indx > startIndx)
                    setFormat (startIndx, indx - startIndx, neutralFormat);
            }
            break;
        }
    }

    if (setData)
    {
        data->insertNestInfo (openNests);
        if (openNests == 0) // braces take priority over brackets
            property = false;
        data->setProperty (property);
        return (openNests != oldOpenNests || property != oldProperty);
    }
    return false; // don't update the next line if no data is set
}
/*************************/
void Highlighter::yamlLiteralBlock (const QString &text)
{
    /* each line of a literal block contains the info on the block's indentation
       as a whitespace string prefixed by "i" */
    TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData());
    if (data == nullptr) return; // impossible
    QString blockIndent;
    QTextBlock prevBlock = currentBlock().previous();
    if (prevBlock.isValid())
    {
        if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
        {
            blockIndent = prevData->labelInfo();
            if (!blockIndent.startsWith ("i"))
                blockIndent = QString();
        }
    }

    QRegularExpressionMatch match;
    if (previousBlockState() == codeBlockState) // the literal block may continue
    {
        (void)text.indexOf (QRegularExpression ("^\\s*"), 0, &match);
        QString startingSpaces = "i" + match.captured();
        if (text == match.captured() // only whitespaces...
            /* ... or the indentation is wider than that of the literal block */
            || (startingSpaces != blockIndent
                && (blockIndent.isEmpty() || startingSpaces.startsWith (blockIndent))))
        {
            setFormat (0, text.length(), codeBlockFormat);
            setCurrentBlockState (codeBlockState);
            data->insertInfo (blockIndent);
            return;
        }
    }

    /* the start of a literal block (which is formatted near the end of the main formatting
       because, if it was formatted here, its format might be overridden by that of a value) */
    static const QRegularExpression yamlBlockStartExp ("^(?!#)(?:(?!\\s#).)*\\s+\\K(\\||>)-?\\s*(?=\\s#|$)");
    static const QRegularExpression yamlKey ("\\s*[^\\s\"\'#][^:,#]*:\\s+");
    int index = text.indexOf (yamlBlockStartExp, 0);
    if (index >= 0)
    {
        if (text.contains (yamlKey))
        { // consider the list sign as a space if the block is a value
            (void)text.indexOf (QRegularExpression ("^\\s*(-\\s)?\\s*"), 0, &match);
        }
        else
        {
            if (text.indexOf (QRegularExpression ("^\\s*-\\s")) == -1)
                return; // if the block isn't a value, it should be a list
            (void)text.indexOf (QRegularExpression ("^\\s*"), 0, &match);
        }
        setCurrentBlockState (codeBlockState);
        data->insertInfo ("i" + match.captured().replace ("-", " "));
    }
}
/*************************/
void Highlighter::highlightYamlBlock (const QString &text)
{
    bool rehighlightNextBlock = false;
    int oldOpenNests = 0;
    bool oldProperty = false;
    QString oldLabel;
    if (TextBlockData *oldData = static_cast<TextBlockData *>(currentBlockUserData()))
    {
        oldOpenNests = oldData->openNests();
        oldProperty = oldData->getProperty();
        oldLabel = oldData->labelInfo();
    }

    int index;
    TextBlockData *data = new TextBlockData;
    data->setLastState (currentBlockState());
    setCurrentBlockUserData (data);
    setCurrentBlockState (0);

    singleLineComment (text, 0);

    bool isCodeBlock = previousBlockState() == codeBlockState;
    bool braces (true);
    int openNests = 0;
    QTextBlock prevBlock = currentBlock().previous();
    if (prevBlock.isValid())
    {
        if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
        {
            openNests = prevData->openNests();
            if (openNests != 0) // braces take priority over brackets
                braces = !prevData->getProperty();

            if (isCodeBlock)
            {
                QString spaces = prevData->labelInfo();
                if (!spaces.startsWith ("i"))
                    spaces = QString();
                else
                    spaces.remove (0, 1);
                isCodeBlock = text.startsWith (spaces + " ");
            }
        }
    }
    if (text.indexOf (QRegularExpression("^---")) == 0) // pass the data
    {
        data->insertNestInfo (openNests);
        data->setProperty (braces);
        rehighlightNextBlock |= oldOpenNests != openNests;
    }
    else // format braces and brackets before formatting multi-line quotes
    {
        if (!isCodeBlock)
        {
            if (braces)
            {
                rehighlightNextBlock |= yamlOpenBraces (text, QRegularExpression ("{"), QRegularExpression ("}"), oldOpenNests, oldProperty, true);
                rehighlightNextBlock |= yamlOpenBraces (text, QRegularExpression ("\\["), QRegularExpression ("\\]"), oldOpenNests, oldProperty,
                                                        data->openNests() == 0); // set data only if braces are completely closed
            }
            else
            {
                rehighlightNextBlock |= yamlOpenBraces (text, QRegularExpression ("\\["), QRegularExpression ("\\]"), oldOpenNests, oldProperty, true);
                rehighlightNextBlock |= yamlOpenBraces (text, QRegularExpression ("{"), QRegularExpression ("}"), oldOpenNests, oldProperty,
                                                        data->openNests() == 0); // set data only if brackets are completely closed
            }
        }
        else
        {
            data->insertNestInfo (0);
            data->setProperty (false);
        }
        if (data->openNests() == 0)
        {
            yamlLiteralBlock (text);
            QString newIndent = data->labelInfo();
            rehighlightNextBlock |= currentBlockState() == data->lastState() && oldLabel != newIndent;
        }

        /* since nothing can be before a Yaml comment except for spaces,
           it's safe to call multiLineQuote() here, after singleLineComment() */
        rehighlightNextBlock |= multiLineQuote (text);
    }

    QTextCharFormat fi;

    /* yaml main Formatting */
    int bn = currentBlock().blockNumber();
    if (bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber())
    {
        data->setHighlighted();
        QRegularExpressionMatch match;
#if (QT_VERSION >= QT_VERSION_CHECK(6,6,0))
        for (const HighlightingRule &rule : std::as_const (highlightingRules))
#else
        for (const HighlightingRule &rule : qAsConst (highlightingRules))
#endif
        {
            if (rule.format != whiteSpaceFormat
                && format (0) == codeBlockFormat) // a literal block
            {
                continue;
            }
            if (rule.format == commentFormat)
                continue;

            index = text.indexOf (rule.pattern, 0, &match);
            if (rule.format != whiteSpaceFormat)
            {
                fi = format (index);
                while (index >= 0
                        && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                            || fi == commentFormat || fi == urlFormat
                            || fi == noteFormat)) // because of Yaml keys (as in "# TODO:...")
                {
                    index = text.indexOf (rule.pattern, index + 1, &match);
                    fi = format (index);
                }
            }
            while (index >= 0)
            {
                int length = match.capturedLength();

                /* check if there is a valid brace inside the regex
                   and if there is, limit the found match to it */
                QString txt = text.mid (index, length);
                int braceIndx = 0;
                while ((braceIndx = txt.indexOf (QRegularExpression ("{"), braceIndx)) >= 0)
                {
                    if (format (index + braceIndx) == neutralFormat
                        && !isYamlBraceEscaped (text, QRegularExpression ("{"), index + braceIndx))
                    {
                        txt = text.mid (index, braceIndx);
                        break;
                    }
                    ++ braceIndx;
                }
                braceIndx = 0;
                while ((braceIndx = txt.indexOf (QRegularExpression ("}"), braceIndx)) >= 0)
                {
                    if (format (index + braceIndx) == neutralFormat)
                    {
                        txt = text.mid (index, braceIndx);
                        break;
                    }
                    ++ braceIndx;
                }
                braceIndx = 0;
                while ((braceIndx = txt.indexOf (QRegularExpression ("\\["), braceIndx)) >= 0)
                {
                    if (format (index + braceIndx) == neutralFormat
                        && !isYamlBraceEscaped (text, QRegularExpression ("\\["), index + braceIndx))
                    {
                        txt = text.mid (index, braceIndx);
                        break;
                    }
                    ++ braceIndx;
                }
                braceIndx = 0;
                while ((braceIndx = txt.indexOf (QRegularExpression ("\\]"), braceIndx)) >= 0)
                {
                    if (format (index + braceIndx) == neutralFormat)
                    {
                        txt = text.mid (index, braceIndx);
                        break;
                    }
                    ++ braceIndx;
                }
                braceIndx = 0;
                while ((braceIndx = txt.indexOf (QRegularExpression (","), braceIndx)) >= 0)
                {
                    if (format (index + braceIndx) == neutralFormat)
                    {
                        txt = text.mid (index, braceIndx);
                        break;
                    }
                    ++ braceIndx;
                }
                length = txt.length();

                if (length > 0)
                {
                    fi = rule.format;
                    if (fi.foreground() == Violet)
                    {
                        if (txt.indexOf (QRegularExpression ("([-+]?(\\d*\\.?\\d+|\\d+\\.)((e|E)(\\+|-)?\\d+)?|0[xX][0-9a-fA-F]+)\\s*(?=(#|$))"), 0, &match) == 0)
                        { // format numerical values differently
                            if (match.capturedLength() == length)
                                fi.setForeground (Brown);
                        }
                        else if (txt.indexOf (QRegularExpression ("(true|false|yes|no|TRUE|FALSE|YES|NO|True|False|Yes|No)\\s*(?=(#|$))"), 0, &match) == 0)
                        { // format booleans differently
                            if (match.capturedLength() == length)
                            {
                                fi.setForeground (DarkBlue);
                                fi.setFontWeight (QFont::Bold);
                            }
                        }
                    }
                    setFormat (index, length, fi);
                }
                index = text.indexOf (rule.pattern, index + std::max (length, 1), &match);

                if (rule.format != whiteSpaceFormat)
                {
                    fi = format (index);
                    while (index >= 0
                            && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                                || fi == commentFormat || fi == urlFormat || fi == noteFormat))
                    {
                        index = text.indexOf (rule.pattern, index + 1, &match);
                        fi = format (index);
                    }
                }
            }
        }
    }

    /* left parenthesis */
    index = text.indexOf ('(');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat))
    {
        index = text.indexOf ('(', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        ParenthesisInfo *info = new ParenthesisInfo;
        info->character = '(';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('(', index + 1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat))
        {
            index = text.indexOf ('(', index + 1);
            fi = format (index);
        }
    }

    /* right parenthesis */
    index = text.indexOf (')');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat))
    {
        index = text.indexOf (')', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        ParenthesisInfo *info = new ParenthesisInfo;
        info->character = ')';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf (')', index +1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat))
        {
            index = text.indexOf (')', index + 1);
            fi = format (index);
        }
    }

    /* left brace */
    index = text.indexOf ('{');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi != neutralFormat))
    {
        index = text.indexOf ('{', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BraceInfo *info = new BraceInfo;
        info->character = '{';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('{', index + 1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi != neutralFormat))
        {
            index = text.indexOf ('{', index + 1);
            fi = format (index);
        }
    }

    /* right brace */
    index = text.indexOf ('}');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi != neutralFormat))
    {
        index = text.indexOf ('}', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BraceInfo *info = new BraceInfo;
        info->character = '}';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('}', index +1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi != neutralFormat))
        {
            index = text.indexOf ('}', index + 1);
            fi = format (index);
        }
    }

    /* left bracket */
    index = text.indexOf ('[');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi != neutralFormat))
    {
        index = text.indexOf ('[', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BracketInfo *info = new BracketInfo;
        info->character = '[';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf ('[', index + 1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi != neutralFormat))
        {
            index = text.indexOf ('[', index + 1);
            fi = format (index);
        }
    }

    /* right bracket */
    index = text.indexOf (']');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi != neutralFormat))
    {
        index = text.indexOf (']', index + 1);
        fi = format (index);
    }
    while (index >= 0)
    {
        BracketInfo *info = new BracketInfo;
        info->character = ']';
        info->position = index;
        data->insertInfo (info);

        index = text.indexOf (']', index +1);
        fi = format (index);
        while (index >= 0
               && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                   || fi == commentFormat || fi == urlFormat
                   || fi != neutralFormat))
        {
            index = text.indexOf (']', index + 1);
            fi = format (index);
        }
    }

    setCurrentBlockUserData (data);

    if (rehighlightNextBlock)
    {
        QTextBlock nextBlock = currentBlock().next();
        if (nextBlock.isValid())
            QMetaObject::invokeMethod (this, "rehighlightBlock", Qt::QueuedConnection, Q_ARG (QTextBlock, nextBlock));
    }
}

}
