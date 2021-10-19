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

#include "highlighter.h"
#include <QTextDocument>

Q_DECLARE_METATYPE(QTextBlock)

namespace FeatherPad {

/* NOTE: It is supposed that a URL does not end with a punctuation mark, parenthesis, bracket or single-quotation mark. */
const QRegularExpression Highlighter::urlPattern ("[A-Za-z0-9_\\-]+://((?!&quot;|&gt;|&lt;)[A-Za-z0-9_.+/\\?\\=~&%#,;!@\\*\'\\-:\\(\\)\\[\\]])+(?<!\\.|\\?|!|:|;|,|\\(|\\)|\\[|\\]|\')|[A-Za-z0-9_.\\-]+@[A-Za-z0-9_\\-]+\\.[A-Za-z0-9.]+(?<!\\.)");
const QRegularExpression Highlighter::notePattern ("\\b(NOTE|TODO|FIXME|WARNING)\\b");

TextBlockData::~TextBlockData()
{
    while (!allParentheses.isEmpty())
    {
        ParenthesisInfo *info = allParentheses.at (0);
        allParentheses.remove (0);
        delete info;
    }
    while (!allBraces.isEmpty())
    {
        BraceInfo *info = allBraces.at (0);
        allBraces.remove (0);
        delete info;
    }
    while (!allBrackets.isEmpty())
    {
        BracketInfo *info = allBrackets.at (0);
        allBrackets.remove (0);
        delete info;
    }
}
/*************************/
QVector<ParenthesisInfo *> TextBlockData::parentheses() const
{
    return allParentheses;
}
/*************************/
QVector<BraceInfo *> TextBlockData::braces() const
{
    return allBraces;
}
/*************************/
QVector<BracketInfo *> TextBlockData::brackets() const
{
    return allBrackets;
}
/*************************/
QString TextBlockData::labelInfo() const
{
    return label;
}
/*************************/
bool TextBlockData::isHighlighted() const
{
    return Highlighted;
}
/*************************/
bool TextBlockData::getProperty() const
{
    return Property;
}
/*************************/
int TextBlockData::lastState() const
{
    return LastState;
}
/*************************/
int TextBlockData::openNests() const
{
    return OpenNests;
}
/*************************/
int TextBlockData::lastFormattedQuote() const
{
    return LastFormattedQuote;
}
/*************************/
int TextBlockData::lastFormattedRegex() const
{
    return LastFormattedRegex;
}
/*************************/
QSet<int> TextBlockData::openQuotes() const
{
    return OpenQuotes;
}
/*************************/
void TextBlockData::insertInfo (ParenthesisInfo *info)
{
    int i = 0;
    while (i < allParentheses.size()
           && info->position > allParentheses.at (i)->position)
    {
        ++i;
    }

    allParentheses.insert (i, info);
}
/*************************/
void TextBlockData::insertInfo (BraceInfo *info)
{
    int i = 0;
    while (i < allBraces.size()
           && info->position > allBraces.at (i)->position)
    {
        ++i;
    }

    allBraces.insert (i, info);
}
/*************************/
void TextBlockData::insertInfo (BracketInfo *info)
{
    int i = 0;
    while (i < allBrackets.size()
           && info->position > allBrackets.at (i)->position)
    {
        ++i;
    }

    allBrackets.insert (i, info);
}
/*************************/
void TextBlockData::insertInfo (const QString &str)
{
    label = str;
}
/*************************/
void TextBlockData::setHighlighted()
{
    Highlighted = true;
}
/*************************/
void TextBlockData::setProperty (bool p)
{
    Property = p;
}
/*************************/
void TextBlockData::setLastState (int state)
{
    LastState = state;
}
/*************************/
void TextBlockData::insertNestInfo (int nests)
{
    OpenNests = nests;
}
/*************************/
void TextBlockData::insertLastFormattedQuote (int last)
{
    LastFormattedQuote = last;
}
/*************************/
void TextBlockData::insertLastFormattedRegex (int last)
{
    LastFormattedRegex = last;
}
/*************************/
void TextBlockData::insertOpenQuotes (const QSet<int> &openQuotes)
{
    OpenQuotes.unite (openQuotes);
}
/*************************/
// Here, the order of formatting is important because of overrides.
Highlighter::Highlighter (QTextDocument *parent, const QString& lang,
                          const QTextCursor &start, const QTextCursor &end,
                          bool darkColorScheme,
                          bool showWhiteSpace,
                          bool showEndings,
                          int whitespaceValue,
                          const QHash<QString, QColor> &syntaxColors) : QSyntaxHighlighter (parent)
{
    if (lang.isEmpty()) return;

    if (showWhiteSpace || showEndings)
    {
        QTextOption opt =  document()->defaultTextOption();
        QTextOption::Flags flags = opt.flags();
        if (showWhiteSpace)
            flags |= QTextOption::ShowTabsAndSpaces;
        if (showEndings)
            flags |= QTextOption::ShowLineAndParagraphSeparators
                     | QTextOption::AddSpaceForLineAndParagraphSeparators // never show the horizontal scrollbar on wrapping
                     | QTextOption::ShowDocumentTerminator;
        opt.setFlags (flags);
        document()->setDefaultTextOption (opt);
    }

    /* for highlighting next block inside highlightBlock() when needed */
    qRegisterMetaType<QTextBlock>();

    startCursor = start;
    endCursor = end;
    progLan = lang;

    /* whether multiLineQuote() should be used in a normal way */
    multilineQuote_ =
        (progLan != "xml" // xmlQuotes() is used
         && progLan != "sh" // SH_MultiLineQuote() is used
         && progLan != "css" // cssHighlighter() is used
         && progLan != "pascal" && progLan != "LaTeX"
         && progLan != "diff" && progLan != "log"
         && progLan != "desktop" && progLan != "config" && progLan != "theme"
         && progLan != "changelog" && progLan != "url"
         && progLan != "srt" && progLan != "html"
         && progLan != "deb" && progLan != "m3u"
         && progLan != "reST" && progLan != "troff"
         && progLan != "yaml"); // yaml will be formated separately

    /* only for isQuoted() and multiLineQuote() (not used with JS, qml and perl) */
    mixedQuotes_ =
        (progLan == "c" || progLan == "cpp" // single quotes can also show syntax errors
         || progLan == "python"
         || progLan == "sh" // not used in multiLineQuote()
         || progLan == "makefile" || progLan == "cmake" || progLan == "lua"
         || progLan == "xml" // never used because we should consider "&quot;"
         || progLan == "ruby"
         || progLan == "html" // not used in multiLineQuote()
         || progLan == "scss" || progLan == "yaml" || progLan == "dart"
         || progLan == "go" || progLan == "php");

    quoteMark.setPattern ("\""); // the standard quote mark (always a single character)
    singleQuoteMark.setPattern ("\'"); // will be changed only for Go
    mixedQuoteMark.setPattern ("\"|\'"); // will be changed only for Go
    backQuote.setPattern ("`");
    /* includes Perl's backquote operator and JavaScript's template literal */
    mixedQuoteBackquote.setPattern ("\"|\'|`");

    HighlightingRule rule;

    QColor Faded (whitespaceValue, whitespaceValue, whitespaceValue);
    QColor TextColor, neutralColor, translucent;
    if (syntaxColors.size() == 11)
    {
        /* NOTE: All 11 + 1 colors should be valid, opaque and different from each other
                 but we don't check them here because "FeatherPad::Config" gets them so. */
        Blue = syntaxColors.value ("function");
        Magenta = syntaxColors.value ("BuiltinFunction");
        Red = syntaxColors.value ("comment");
        DarkGreen = syntaxColors.value ("quote");
        DarkMagenta = syntaxColors.value ("type");
        DarkBlue = syntaxColors.value ("keyWord");
        Brown = syntaxColors.value ("number");
        DarkRed = syntaxColors.value ("regex");
        Violet = syntaxColors.value ("xmlElement");
        Verda = syntaxColors.value ("cssValue");
        DarkYellow = syntaxColors.value ("other");

        QList<QColor> colors;
        colors << Blue << Magenta << Red << DarkGreen << DarkMagenta << DarkBlue << Brown << DarkRed << Violet << Verda << DarkYellow << Faded;

        /* extra colors */
        if (!darkColorScheme)
        {
            TextColor =  QColor (Qt::black);
            neutralColor = QColor (1, 1, 1);
            translucent = QColor (0, 0, 0, 190); // for huge lines
        }
        else
        {
            TextColor =  QColor (Qt::white);
            neutralColor = QColor (254, 254, 254);
            translucent = QColor (255, 255, 255, 190);
        }

        int i = 0;
        while (colors.contains (TextColor))
        {
            ++i;
            if (!darkColorScheme)
                TextColor = QColor (i, i, i);
            else
                TextColor = QColor (255 - i, 255 - i, 255 - i);
        }
        colors << TextColor;

        while (colors.contains (neutralColor))
        {
            ++i;
            if (!darkColorScheme)
                neutralColor = QColor (i, i, i);
            else
                neutralColor = QColor (255 - i, 255 - i, 255 - i);
        }
        colors << neutralColor;

        DarkGreenAlt = DarkGreen.lighter (101);
        if (DarkGreenAlt == DarkGreen)
        {
            DarkGreenAlt = QColor (DarkGreen.red() > 127 ? DarkGreen.red() - 1 : DarkGreen.red() + 1,
                                   DarkGreen.green() > 127 ? DarkGreen.green() - 1 : DarkGreen.green() + 1,
                                   DarkGreen.blue() > 127 ? DarkGreen.blue() - 1 : DarkGreen.blue() + 1);
        }
        while (colors.contains (DarkGreenAlt))
        {
            DarkGreenAlt = QColor (DarkGreen.red() > 127 ? DarkGreenAlt.red() - 1 : DarkGreenAlt.red() + 1,
                                   DarkGreen.green() > 127 ? DarkGreenAlt.green() - 1 : DarkGreenAlt.green() + 1,
                                   DarkGreen.blue() > 127 ? DarkGreenAlt.blue() - 1 : DarkGreenAlt.blue() + 1);
        }
    }
    else // built-in colors (this block is never reached but is kept for the sake of consistency)
    {
        if (!darkColorScheme)
        {
            TextColor =  QColor (Qt::black);
            neutralColor = QColor (1, 1, 1);
            Blue = QColor (Qt::blue);
            DarkBlue = QColor (Qt::darkBlue);
            Red = QColor (Qt::red);
            DarkRed = QColor (150, 0, 0);
            Verda = QColor (0, 110, 110);
            DarkGreen = QColor (Qt::darkGreen);
            DarkMagenta = QColor (Qt::darkMagenta);
            Violet = QColor (126, 0, 230); // #7e00e6
            Brown = QColor (150, 85, 0);
            DarkYellow = QColor (100, 100, 0); // Qt::darkYellow is (180, 180, 0)
            translucent = QColor (0, 0, 0, 190);
        }
        else
        {
            TextColor =  QColor (Qt::white);
            neutralColor = QColor (254, 254, 254);
            Blue = QColor (85, 227, 255);
            DarkBlue = QColor (65, 154, 255);
            Red = QColor (255, 120, 120);
            DarkRed = QColor (255, 160, 0);
            Verda = QColor (150, 255, 0);
            DarkGreen = QColor (Qt::green);
            DarkMagenta = QColor (255, 153, 255);
            Violet = QColor (255, 255, 1); // near Qt::yellow (= DarkYellow)
            Brown = QColor (255, 205, 0);
            DarkYellow = QColor (Qt::yellow);
            translucent = QColor (255, 255, 255, 190);
        }
        Magenta = QColor (Qt::magenta);
        DarkGreenAlt = DarkGreen.lighter (101); // almost identical
    }

    mainFormat.setForeground (TextColor);
    neutralFormat.setForeground (neutralColor);
    whiteSpaceFormat.setForeground (Faded);
    translucentFormat.setForeground (translucent);
    translucentFormat.setFontItalic (true);

    quoteFormat.setForeground (DarkGreen);
    altQuoteFormat.setForeground (DarkGreen);
    urlInsideQuoteFormat.setForeground (DarkGreen);
    altQuoteFormat.setFontItalic (true);
    urlInsideQuoteFormat.setFontItalic (true);
    urlInsideQuoteFormat.setFontUnderline (true);
    /*quoteStartExpression.setPattern ("\"([^\"'])");
    quoteEndExpression.setPattern ("([^\"'])\"");*/
    regexFormat.setForeground (DarkRed);

    /*************************
     * Functions and Numbers *
     *************************/

    /* there may be javascript inside html */
    QString Lang = progLan == "html" ? "javascript" : progLan;

    /* might be overridden by the keywords format */
    if (progLan == "c" || progLan == "cpp"
        || progLan == "lua" || progLan == "python"
        || progLan == "php" || progLan == "dart"
        || progLan == "go" || progLan == "java")
    {
        QTextCharFormat ft;

        /* numbers (including the exponential notation, binary, octal and hexadecimal literals) */
        ft.setForeground (Brown);
        if (progLan == "python")
            rule.pattern.setPattern ("(?<=^|[^\\w\\d\\.])("
                                     "\\d*\\.\\d+|\\d+\\.|(\\d*\\.?\\d+|\\d+\\.)(e|E)(\\+|-)?\\d+"
                                     "|"
                                     "0[bB][01]+|0[xX][0-9a-fA-F]+"
                                     "|"
                                     "(0|[1-9]\\d*)(L|l)?" // digits
                                     ")(?=[^\\w\\d\\.]|$)");
        else if (progLan == "java")
            rule.pattern.setPattern ("(?<=^|[^\\w\\d\\.])("
                                     "(\\d*\\.\\d+|\\d+\\.)(L|l|F|f)?"
                                     "|"
                                     "(\\d*\\.?\\d+|\\d+\\.)(e|E)(\\+|-)?\\d+(F|f)?"
                                     "|"
                                     "0[xX]([0-9a-fA-F]*\\.?[0-9a-fA-F]+|[0-9a-fA-F]+\\.)(p|P)(\\+|-)?\\d+(F|f)?"
                                     "|"
                                     "([1-9]\\d*|0[0-7]*)(L|l|F|f)?|(0[xX][0-9a-fA-F]+|0[bB][01]+)(L|l)?" // integer
                                     ")(?=[^\\w\\d\\.]|$)");
        else
            rule.pattern.setPattern ("(?<=^|[^\\w\\d\\.])("
                                     "(\\d*\\.\\d+|\\d+\\.|(\\d*\\.?\\d+|\\d+\\.)(e|E)(\\+|-)?\\d+)(L|l|F|f)?"
                                     /*"|"
                                     "0[xX]([0-9a-fA-F]*\\.[0-9a-fA-F]+|[0-9a-fA-F]+\\.)(L|l)?"*/ // hexadecimal floating literals need exponent
                                     "|"
                                     "0[xX]([0-9a-fA-F]*\\.?[0-9a-fA-F]+|[0-9a-fA-F]+\\.)(p|P)(\\+|-)?\\d+(L|l|F|f)?"
                                     "|"
                                     "([1-9]\\d*|0[0-7]*|0[xX][0-9a-fA-F]+|0[bB][01]+)(L|l|U|u|UL|ul|LL|ll|ULL|ull)?" // integer
                                     ")(?=[^\\w\\d\\.]|$)");
        rule.format = ft;
        highlightingRules.append (rule);

        /* POSIX signals */
        ft.setForeground (DarkYellow);
        rule.pattern.setPattern ("\\b(SIGABRT|SIGIOT|SIGALRM|SIGVTALRM|SIGPROF|SIGBUS|SIGCHLD|SIGCONT|SIGFPE|SIGHUP|SIGILL|SIGINT|SIGKILL|SIGPIPE|SIGPOLL|SIGRTMIN|SIGRTMAX|SIGQUIT|SIGSEGV|SIGSTOP|SIGSYS|SIGTERM|SIGTSTP|SIGTTIN|SIGTTOU|SIGTRAP|SIGURG|SIGUSR1|SIGUSR2|SIGXCPU|SIGXFSZ|SIGWINCH)(?!(\\.|-|@|#|\\$))\\b");
        rule.format = ft;
        highlightingRules.append (rule);

        ft.setFontItalic (true);
        ft.setForeground (Blue);
        /* before parentheses... */
        rule.pattern.setPattern ("\\b[A-Za-z0-9_]+(?=\\s*\\()");
        rule.format = ft;
        highlightingRules.append (rule);
        /* ... but make exception for what comes after "#define" */
        if (progLan == "c" || progLan == "cpp")
        {
            rule.pattern.setPattern ("^\\s*#\\s*define\\s+[^\"\']+" // may contain slash but no quote
                                     "(?=\\s*\\()");
            rule.format = neutralFormat;
            highlightingRules.append (rule);
        }
        else if (progLan == "python")
        { // built-in functions
            ft.setFontWeight (QFont::Bold);
            ft.setForeground (Magenta);
            rule.pattern.setPattern ("\\b(abs|add|all|append|any|as_integer_ratio|ascii|basestring|bin|bit_length|bool|bytearray|bytes|callable|c\\.conjugate|capitalize|center|chr|classmethod|clear|cmp|compile|complex|count|critical|debug|decode|delattr|dict|difference_update|dir|discard|divmod|encode|endswith|enumerate|error|eval|expandtabs|exception|exec|execfile|extend|file|filter|find|float|format|fromhex|fromkeys|frozenset|get|getattr|globals|hasattr|hash|has_key|help|hex|id|index|info|input|insert|int|intersection_update|isalnum|isalpha|isdecimal|isdigit|isinstance|islower|isnumeric|isspace|issubclass|istitle|items|iter|iteritems|iterkeys|itervalues|isupper|is_integer|join|keys|len|list|ljust|locals|log|long|lower|lstrip|map|max|memoryview|min|next|object|oct|open|ord|partition|pop|popitem|pow|print|property|range|raw_input|read|reduce|reload|remove|replace|repr|reverse|reversed|rfind|rindex|rjust|rpartition|round|rsplit|rstrip|run|seek|set|setattr|slice|sort|sorted|split|splitlines|staticmethod|startswith|str|strip|sum|super|symmetric_difference_update|swapcase|title|translate|tuple|type|unichr|unicode|update|upper|values|vars|viewitems|viewkeys|viewvalues|warning|write|xrange|zip|zfill|(__(abs|add|and|cmp|coerce|complex|contains|delattr|delete|delitem|delslice|div|divmod|enter|eq|exit|float|floordiv|ge|get|getattr|getattribute|getitem|getslice|gt|hex|iadd|iand|idiv|ifloordiv|ilshift|invert|imod|import|imul|init|instancecheck|index|int|ior|ipow|irshift|isub|iter|itruediv|ixor|le|len|long|lshift|lt|missing|mod|mul|neg|nonzero|oct|or|pos|pow|radd|rand|rdiv|rdivmod|reversed|rfloordiv|rlshift|rmod|rmul|ror|rpow|rshift|rsub|rrshift|rtruediv|rxor|set|setattr|setitem|setslice|sub|subclasses|subclasscheck|truediv|unicode|xor)__))(?=\\s*\\()");
            rule.format = ft;
            highlightingRules.append (rule);
        }
    }
    else if (Lang == "javascript" || progLan == "qml")
    {
        QTextCharFormat ft;

        /* before dot but not after it (might be overridden by keywords) */
        ft.setForeground (Blue);
        ft.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("(?<![A-Za-z0-9_\\$\\.])\\s*[A-Za-z0-9_\\$]*[A-Za-z][A-Za-z0-9_\\$]*\\s*(?=\\.\\s*[A-Za-z0-9_\\$]*[A-Za-z][A-Za-z0-9_\\$]*)"); // "(?<![A-Za-z0-9_\\$])[A-Za-z0-9_\\$]+\\s*(?=\\.)"
        rule.format = ft;
        highlightingRules.append (rule);

        /* before parentheses */
        ft.setFontWeight (QFont::Normal);
        ft.setFontItalic (true);
        rule.pattern.setPattern ("(?<![A-Za-z0-9_\\$])[A-Za-z0-9_\\$]*[A-Za-z][A-Za-z0-9_\\$]*(?=\\s*\\()");
        rule.format = ft;
        highlightingRules.append (rule);

        ft.setFontItalic (false);
        ft.setFontWeight (QFont::Bold);
        ft.setForeground (Magenta);
        rule.pattern.setPattern ("\\b(?<!(@|#|\\$))(export|from|import|as)(?!(@|#|\\$|(\\s*:)))\\b");
        rule.format = ft;
        highlightingRules.append (rule);
    }
    else if (progLan == "troff")
    {
        QTextCharFormat troffFormat;

        troffFormat.setForeground (Blue);
        rule.pattern.setPattern ("^\\.\\s*[a-zA-Z]+");
        rule.format = troffFormat;
        highlightingRules.append (rule);

        troffFormat.setForeground (DarkMagenta);
        rule.pattern.setPattern ("^\\.\\s*[a-zA-Z]+\\K.*");
        rule.format = troffFormat;
        highlightingRules.append (rule);

        /* numbers */
        troffFormat.setForeground (Brown);
        rule.pattern.setPattern ("(?<=^|[^\\w\\d@#\\$\\.])((\\d*\\.?\\d+|\\d+\\.)((e|E)(\\+|-)?\\d+)?|0[xX][0-9a-fA-F]+)(?=[^\\d\\.]|$)");
        rule.format = troffFormat;
        highlightingRules.append (rule);

        /* meaningful escapes */
        troffFormat.setForeground (DarkGreen);
        rule.pattern.setPattern ("\\\\(e|'|`|-|\\s|0|\\||\\^|&|\\!|\\\\\\$\\d+|%|\\(\\w{2}|\\\\\\*\\w|\\*\\(\\w{2}|a|b(?='\\w)|c|d|D|f(\\w|\\(\\w{2})|(h|H)(?='\\d)|(j|k)\\w|l|L|n(\\w|\\(\\w{2})|o|p|r|s|S(?='\\d)|t|u|v(?='\\d)|w|x|zc|\\{|\\})");
        rule.format = troffFormat;
        highlightingRules.append (rule);

        /* other escapes */
        troffFormat.setForeground (Violet);
        rule.pattern.setPattern ("\\\\([^e'`\\-\\s0\\|\\^&\\!\\\\%\\(\\*abcdDfhHjklLnoprsStuvwxz\\{\\}]|\\((?!\\w{2})|\\\\(?!\\$\\d)|\\*(?!\\(\\w{2})|b(?!'\\w)|(f|j|k|n)(?!\\w)|(f|n)(?!\\(\\w{2})|(h|H)(?!'\\d)|(S|v)(?!'\\d)|z(?!c))");
        rule.format = troffFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "LaTeX")
    {
        codeBlockFormat.setForeground (DarkMagenta);

        /* commands */
        QTextCharFormat laTexFormat;
        laTexFormat.setForeground (Blue);
        rule.pattern.setPattern ("\\\\([a-zA-Z]+|[^a-zA-Z\\(\\)\\[\\]])");
        rule.format = laTexFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "pascal")
    {
        /* before parentheses */
        QTextCharFormat pascalFormat;
        pascalFormat.setFontItalic (true);
        pascalFormat.setForeground (Blue);
        rule.pattern.setPattern ("\\b(?<!(@|#|\\$))[A-Za-z0-9_]+(?=\\s*\\()");
        rule.format = pascalFormat;
        highlightingRules.append (rule);
    }

    /**********************
     * Keywords and Types *
     **********************/

    /* keywords */
    QTextCharFormat keywordFormat;
    /* bash extra keywords */
    if (progLan == "sh" || progLan == "makefile" || progLan == "cmake")
    {
        if (progLan == "cmake")
        {
            keywordFormat.setForeground (Brown);
            rule.pattern.setPattern ("\\$\\{\\s*[A-Za-z0-9_.+/\\?#\\-:]*\\s*\\}");
            rule.format = keywordFormat;
            highlightingRules.append (rule);

            /* projects (may be overridden by CMAKE keywords below it) */
            keywordFormat.setForeground (Blue);
            rule.pattern.setPattern ("(?<=^|\\(|\\s)[A-Za-z0-9_]+(_BINARY_DIR|_SOURCE_DIR|_VERSION|_VERSION_MAJOR|_VERSION_MINOR|_VERSION_PATCH|_VERSION_TWEAK|_SOVERSION)(?!(\\.|-|@|#|\\$))\\b");
            rule.format = keywordFormat;
            highlightingRules.append (rule);

            /*
               previously, the first six patterns were started with:
               ((^\\s*|[\\(\\);&`\\|{}!=^]+\\s*|(?<=~|\\.)+\\s+)((if|then|elif|elseif|else|fi|while|do|done|esac)\\s+)*)
               instead of: (?<=^|\\(|\\s)
            */
            keywordFormat.setForeground (DarkBlue);
            rule.pattern.setPattern ("(?<=^|\\(|\\s)(CMAKE_ARGC|CMAKE_ARGV0|CMAKE_ARGS|CMAKE_AR|CMAKE_BINARY_DIR|CMAKE_BUILD_TOOL|CMAKE_CACHE_ARGS|CMAKE_CACHE_DEFAULT_ARGS|CMAKE_CACHEFILE_DIR|CMAKE_CACHE_MAJOR_VERSION|CMAKE_CACHE_MINOR_VERSION|CMAKE_CACHE_PATCH_VERSION|CMAKE_CFG_INTDIR|CMAKE_COMMAND|CMAKE_CROSSCOMPILING|CMAKE_CTEST_COMMAND|CMAKE_CURRENT_BINARY_DIR|CMAKE_CURRENT_LIST_DIR|CMAKE_CURRENT_LIST_FILE|CMAKE_CURRENT_LIST_LINE|CMAKE_CURRENT_SOURCE_DIR|CMAKE_DL_LIBS|CMAKE_EDIT_COMMAND|CMAKE_EXECUTABLE_SUFFIX|CMAKE_EXTRA_GENERATOR|CMAKE_EXTRA_SHARED_LIBRARY_SUFFIXES|CMAKE_GENERATOR|CMAKE_GENERATOR_INSTANCE|CMAKE_GENERATOR_PLATFORM|CMAKE_GENERATOR_TOOLSET|CMAKE_HOME_DIRECTORY|CMAKE_IMPORT_LIBRARY_PREFIX|CMAKE_IMPORT_LIBRARY_SUFFIX|CMAKE_JOB_POOL_COMPILE|CMAKE_JOB_POOL_LINK|CMAKE_LINK_LIBRARY_SUFFIX|CMAKE_MAJOR_VERSION|CMAKE_MAKE_PROGRAM|CMAKE_MINIMUM_REQUIRED_VERSION|CMAKE_MINOR_VERSION|CMAKE_PARENT_LIST_FILE|CMAKE_PATCH_VERSION|CMAKE_PROJECT_NAME|CMAKE_RANLIB|CMAKE_ROOT|CMAKE_SCRIPT_MODE_FILE|CMAKE_SHARED_LIBRARY_PREFIX|CMAKE_SHARED_LIBRARY_SUFFIX|CMAKE_SHARED_MODULE_PREFIX|CMAKE_SHARED_MODULE_SUFFIX|CMAKE_SIZEOF_VOID_P|CMAKE_SKIP_INSTALL_RULES|CMAKE_SKIP_RPATH|CMAKE_SOURCE_DIR|CMAKE_STANDARD_LIBRARIES|CMAKE_STATIC_LIBRARY_PREFIX|CMAKE_STATIC_LIBRARY_SUFFIX|CMAKE_TOOLCHAIN_FILE|CMAKE_TWEAK_VERSION|CMAKE_VERBOSE_MAKEFILE|CMAKE_VERSION|CMAKE_VS_DEVENV_COMMAND|CMAKE_VS_INTEL_Fortran_PROJECT_VERSION|CMAKE_VS_MSBUILD_COMMAND|CMAKE_VS_MSDEV_COMMAND|CMAKE_VS_PLATFORM_TOOLSETCMAKE_XCODE_PLATFORM_TOOLSET|PROJECT_BINARY_DIR|PROJECT_NAME|PROJECT_SOURCE_DIR|PROJECT_VERSION|PROJECT_VERSION_MAJOR|PROJECT_VERSION_MINOR|PROJECT_VERSION_PATCH|PROJECT_VERSION_TWEAK|BUILD_SHARED_LIBS|CMAKE_ABSOLUTE_DESTINATION_FILES|CMAKE_APPBUNDLE_PATH|CMAKE_AUTOMOC_RELAXED_MODE|CMAKE_BACKWARDS_COMPATIBILITY|CMAKE_BUILD_TYPE|CMAKE_COLOR_MAKEFILE|CMAKE_CONFIGURATION_TYPES|CMAKE_DEBUG_TARGET_PROPERTIES|CMAKE_ERROR_DEPRECATED|CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION|CMAKE_SYSROOT|CMAKE_FIND_LIBRARY_PREFIXES|CMAKE_FIND_LIBRARY_SUFFIXES|CMAKE_FIND_NO_INSTALL_PREFIX|CMAKE_FIND_PACKAGE_WARN_NO_MODULE|CMAKE_FIND_ROOT_PATH|CMAKE_FIND_ROOT_PATH_MODE_INCLUDE|CMAKE_FIND_ROOT_PATH_MODE_LIBRARY|CMAKE_FIND_ROOT_PATH_MODE_PACKAGE|CMAKE_FIND_ROOT_PATH_MODE_PROGRAM|CMAKE_FRAMEWORK_PATH|CMAKE_IGNORE_PATH|CMAKE_INCLUDE_PATH|CMAKE_INCLUDE_DIRECTORIES_BEFORE|CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE|CMAKE_INSTALL_DEFAULT_COMPONENT_NAME|CMAKE_INSTALL_PREFIX|CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT|CMAKE_LIBRARY_PATH|CMAKE_MFC_FLAG|CMAKE_MODULE_PATH|CMAKE_NOT_USING_CONFIG_FLAGS|CMAKE_PREFIX_PATH|CMAKE_PROGRAM_PATH|CMAKE_SKIP_INSTALL_ALL_DEPENDENCY|CMAKE_STAGING_PREFIX|CMAKE_SYSTEM_IGNORE_PATH|CMAKE_SYSTEM_INCLUDE_PATH|CMAKE_SYSTEM_LIBRARY_PATH|CMAKE_SYSTEM_PREFIX_PATH|CMAKE_SYSTEM_PROGRAM_PATH|CMAKE_USER_MAKE_RULES_OVERRIDE|CMAKE_WARN_DEPRECATED|CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION|BORLAND|CMAKE_CL_64|CMAKE_COMPILER_2005|CMAKE_HOST_APPLE|CMAKE_HOST_SYSTEM_NAME|CMAKE_HOST_SYSTEM_PROCESSOR|CMAKE_HOST_SYSTEM|CMAKE_HOST_SYSTEM_VERSION|CMAKE_HOST_UNIX|CMAKE_HOST_WIN32|CMAKE_LIBRARY_ARCHITECTURE_REGEX|CMAKE_LIBRARY_ARCHITECTURE|CMAKE_OBJECT_PATH_MAX|CMAKE_SYSTEM_NAME|CMAKE_SYSTEM_PROCESSOR|CMAKE_SYSTEM|CMAKE_SYSTEM_VERSION|ENV|MSVC10|MSVC11|MSVC12|MSVC60|MSVC70|MSVC71|MSVC80|MSVC90|MSVC_IDE|MSVC|MSVC_VERSION|XCODE_VERSION|CMAKE_ARCHIVE_OUTPUT_DIRECTORY|CMAKE_AUTOMOC_MOC_OPTIONS|CMAKE_AUTOMOC|CMAKE_AUTORCC|CMAKE_AUTORCC_OPTIONS|CMAKE_AUTOUIC|CMAKE_AUTOUIC_OPTIONS|CMAKE_BUILD_WITH_INSTALL_RPATH|CMAKE_DEBUG_POSTFIX|CMAKE_EXE_LINKER_FLAGS|CMAKE_Fortran_FORMAT|CMAKE_Fortran_MODULE_DIRECTORY|CMAKE_GNUtoMS|CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE|CMAKE_INCLUDE_CURRENT_DIR|CMAKE_INSTALL_NAME_DIR|CMAKE_INSTALL_RPATH|CMAKE_INSTALL_RPATH_USE_LINK_PATH|CMAKE_LIBRARY_OUTPUT_DIRECTORY|CMAKE_LIBRARY_PATH_FLAG|CMAKE_LINK_DEF_FILE_FLAG|CMAKE_LINK_DEPENDS_NO_SHARED|CMAKE_LINK_INTERFACE_LIBRARIES|CMAKE_LINK_LIBRARY_FILE_FLAG|CMAKE_LINK_LIBRARY_FLAG|CMAKE_MACOSX_BUNDLE|CMAKE_MACOSX_RPATH|CMAKE_MODULE_LINKER_FLAGS|CMAKE_NO_BUILTIN_CHRPATH|CMAKE_NO_SYSTEM_FROM_IMPORTED|CMAKE_OSX_ARCHITECTURES|CMAKE_OSX_DEPLOYMENT_TARGET|CMAKE_OSX_SYSROOT|CMAKE_PDB_OUTPUT_DIRECTORY|CMAKE_POSITION_INDEPENDENT_CODE|CMAKE_RUNTIME_OUTPUT_DIRECTORY|CMAKE_SHARED_LINKER_FLAGS|CMAKE_SKIP_BUILD_RPATH|CMAKE_SKIP_INSTALL_RPATH|CMAKE_STATIC_LINKER_FLAGS|CMAKE_TRY_COMPILE_CONFIGURATION|CMAKE_USE_RELATIVE_PATHS|CMAKE_VISIBILITY_INLINES_HIDDEN|CMAKE_WIN32_EXECUTABLE|EXECUTABLE_OUTPUT_PATH|LIBRARY_OUTPUT_PATH|CMAKE_Fortran_MODDIR_DEFAULT|CMAKE_Fortran_MODDIR_FLAG|CMAKE_Fortran_MODOUT_FLAG|CMAKE_INTERNAL_PLATFORM_ABI)(?!(\\.|-|@|#|\\$))\\b");
            rule.format = keywordFormat;
            highlightingRules.append (rule);

            rule.pattern.setPattern ("(?<=^|\\(|\\s)(CMAKE_DISABLE_FIND_PACKAGE_|CMAKE_EXE_LINKER_FLAGS_|CMAKE_MAP_IMPORTED_CONFIG_|CMAKE_MODULE_LINKER_FLAGS_|CMAKE_PDB_OUTPUT_DIRECTORY_|CMAKE_SHARED_LINKER_FLAGS_|CMAKE_STATIC_LINKER_FLAGS_|CMAKE_COMPILER_IS_GNU)[A-Za-z0-9_]+(?!(\\.|-|@|#|\\$))\\b");
            highlightingRules.append (rule);

            rule.pattern.setPattern ("(?<=^|\\(|\\s)(CMAKE_)[A-Za-z0-9_]+(_POSTFIX|_VISIBILITY_PRESET|_ARCHIVE_APPEND|_ARCHIVE_CREATE|_ARCHIVE_FINISH|_COMPILE_OBJECT|_COMPILER_ABI|_COMPILER_ID|_COMPILER_LOADED|_COMPILER|_COMPILER_EXTERNAL_TOOLCHAIN|_COMPILER_TARGET|_COMPILER_VERSION|_CREATE_SHARED_LIBRARY|_CREATE_SHARED_MODULE|_CREATE_STATIC_LIBRARY|_FLAGS_DEBUG|_FLAGS_MINSIZEREL|_FLAGS_RELEASE|_FLAGS_RELWITHDEBINFO|_FLAGS|_IGNORE_EXTENSIONS|_IMPLICIT_INCLUDE_DIRECTORIES|_IMPLICIT_LINK_DIRECTORIES|_IMPLICIT_LINK_FRAMEWOR)(?!(\\.|-|@|#|\\$))\\b");
            highlightingRules.append (rule);

            rule.pattern.setPattern ("(?<=^|\\(|\\s)(CMAKE_PROJECT_)[A-Za-z0-9_]+(_INCLUDE)(?!(\\.|-|@|#|\\$))\\b");
            highlightingRules.append (rule);

            /* platforms */
            keywordFormat.setFontItalic (true);
            rule.pattern.setPattern ("(?<=^|\\(|\\s)(APPLE|CRLF|CYGWIN|DOS|HAIKU|LF|MINGW|MSYS|UNIX|WIN32)(?=$|\\)|\\s)");
            rule.format = keywordFormat;
            highlightingRules.append (rule);
            keywordFormat.setFontItalic (false);

            keywordFormat.setForeground (DarkMagenta);
            rule.pattern.setPattern ("(?<=^|\\(|\\s)(AND|OR|NOT)(?=$|\\s)");
            rule.format = keywordFormat;
            highlightingRules.append (rule);

            rule.pattern.setPattern ("(?<=^|\\(|\\s)(ABSOLUTE|AFTER|ALIAS|ALIASED_TARGET|ALL|ALWAYS|APPEND|ARG_VAR|ARGS|ARCHIVE|ATTACHED_FILES|ATTACHED_FILES_ON_FAIL|AUTHOR_WARNING|BEFORE|BINARY_DIR|BOOL|BRIEF_DOCS|BUNDLE|BYPRODUCTS|CACHE|CACHED_VARIABLE|CLEAR|CMAKE_FIND_ROOT_PATH_BOTH|COMP|COMMAND|COMMENT|COMPONENT|COMPONENTS|CONFIG|CONFIG_MODE|CONFIGS|CONFIGURE_COMMAND|CONFIGURATIONS|COPY|COPYONLY|COMPATIBILITY|COST|CREATE_LINK|DEFAULT_MSG|DEPENDS|DEPENDEES|DEPENDERS|DEPRECATION|DESCRIPTION|DESTINATION|DIRECTORY|DIRECTORY_PERMISSIONS|DISABLED|DISABLED_FEATURES|DOC|DOWNLOAD|DOWNLOAD_COMMAND|DOWNLOAD_DIR|DOWNLOAD_NAME|DOWNLOAD_NO_EXTRACT|DOWNLOAD_NO_PROGRESS|ENABLED_FEATURES|ENCODING|ERROR_VARIABLE|ERROR_FILE|ERROR_QUIET|ERROR_STRIP_TRAILING_WHITESPACE|ESCAPE_QUOTES|EXACT|EXCLUDE|EXCLUDE_FROM_ALL|EXCLUDE_FROM_MAIN|EXPORT|EXPORT_LINK_INTERFACE_LIBRARIES|EXPORT_NAME|EXPORT_PROPERTIES|EXT|FAIL_MESSAGE|FALSE|FATAL_ERROR|FATAL_ON_MISSING_REQUIRED_PACKAGES|FILE_PERMISSIONS|FILENAME|FILEPATH|FILES|FILES_MATCHING|FIND|FILTER|FOLDER|FRAMEWORK|FORCE|FOUND_VAR|FULL_DOCS|GENERATE|GET|GLOB|GLOB_RECURSE|GLOBAL|GROUP_EXECUTE|GROUP_READ|GUARD|HANDLE_COMPONENTS|HINTS|IMMEDIATE|IMPLICIT_DEPENDS|IMPORTED|IN|INDEPENDENT_STEP_TARGETS|INCLUDE_QUIET_PACKAGES|INCLUDES|INPUT_FILE|INSERT|INSTALL|INSTALL_COMMAND|INSTALL_DIR|INSTALL_DESTINATION|INTERNAL|INTERFACE|JOIN|LABELS|LAST_EXT|LENGTH|LIBRARY|LIMIT|LINK_PRIVATE|LINK_PUBLIC|LOCK|LOG|LOG_DIR|MAIN_DEPENDENCY|MAKE_DIRECTORY|MEASUREMENT|MODULE|MODIFIED|NAMELINK_ONLY|NAMELINK_SKIP|NAMESPACE|NAME|NAME_WE|NAME_WLE|NAMES|NEWLINE_STYLE|NO_CHECK_REQUIRED_COMPONENTS_MACRO|NO_CMAKE_BUILDS_PATH|NO_CMAKE_ENVIRONMENT_PATH|NO_CMAKE_FIND_ROOT_PATH|NO_CMAKE_PACKAGE_REGISTRY|NO_CMAKE_PATH|NO_CMAKE_SYSTEM_PACKAGE_REGISTRY|NO_CMAKE_SYSTEM_PATH|NO_DEPENDS|NO_DEFAULT_PATH|NO_MODULE|NO_POLICY_SCOPE|NO_SET_AND_CHECK_MACRO|NO_SOURCE_PERMISSIONS|NO_SYSTEM_ENVIRONMENT_PATH|OBJECT|OFF|OFFSET|ON|ONLY_CMAKE_FIND_ROOT_PATH|OPTIONAL|OPTIONAL_PACKAGES_FOUND|OPTIONAL_PACKAGES_NOT_FOUND|OUTPUT_FILE|OUTPUT_QUIET|OUTPUT_STRIP_TRAILING_WHITESPACE|OUTPUT_VARIABLE|OWNER_EXECUTE|OWNER_READ|OWNER_WRITE|PATH|PACKAGES_FOUND|PACKAGES_NOT_FOUND|PACKAGE_VERSION_FILE|PARENT_SCOPE|PATHS|PATH_SUFFIXES|PATCH_COMMAND|PATH_VARS|PATTERN|PERMISSIONS|PKG_CONFIG_FOUND|PKG_CONFIG_EXECUTABLE|PKG_CONFIG_VERSION_STRING|PRE_BUILD|PRE_LINK|PREFIX|POST_BUILD|PRIVATE|PRIVATE_HEADER|PROGRAM|PROGRAM_ARGS|PROGRAMS|PROPERTY|PROPERTIES|PUBLIC|PUBLIC_HEADER|PURPOSE|QUIET|RANGE|READ|READ_SYMLINK|REALPATH|RECOMMENDED|RECOMMENDED_PACKAGES_FOUND|RECOMMENDED_PACKAGES_NOT_FOUND|REGEX|RELATIVE_PATH|RENAME|REMOVE|REMOVE_AT|REMOVE_DUPLICATES|REMOVE_RECURSE|REMOVE_ITEM|REQUIRED|REQUIRED_PACKAGES_FOUND|REQUIRED_PACKAGES_NOT_FOUND|REQUIRED_VARS|RESOURCE|REVERSE|RESULT_VARIABLE|RESULTS_VARIABLE|RUNTIME|RUNTIME_PACKAGES_FOUND|RUNTIME_PACKAGES_NOT_FOUND|RETURN_VALUE|SEND_ERROR|SHARED|SIZE|SORT|SOURCE|SOURCE_DIR|SOURCE_SUBDIR|SOURCES|SOVERSION|STAMP_DIR|STATIC|STATUS|STEP_TARGETS|STRING|STRINGS|SUBLIST|SUFFIX|SYSTEM|TARGETS|TEST|TIMEOUT|TIMESTAMP|TMP_DIR|TO_CMAKE_PATH|TO_NATIVE_PATH|TOUCH|TOUCH_NOCREATE|TRANSFORM|TRUE|TYPE|UNKNOWN|UPDATE_COMMAND|UPDATE_DISCONNECTED|UPLOAD|URL|URL_HASH|USES_TERMINAL|VAR|VARIABLE|VARIABLE_PREFIX|VERBATIM|VERSION|VERSION_HEADER|VERSION_VAR|WARNING|WHAT|WILL_FAIL|WORKING_DIRECTORY|WRITE)(?=$|\\)|\\s)");
            highlightingRules.append (rule);

            keywordFormat.setFontWeight (QFont::Bold);
            keywordFormat.setFontItalic (true);
            rule.pattern.setPattern ("(?<=^|\\(|\\s)(@ONLY|DEFINED|EQUAL|EXISTS|GREATER|IS_ABSOLUTE|IS_NEWER_THAN|IS_SYMLINK|LESS|MATCHES|STREQUAL|STRGREATER|STRLESS|VERSION_GREATER|VERSION_EQUAL|VERSION_LESS|AnyNewerVersion|ExactVersion|SameMajorVersion)(?=$|\\)|\\s)");
            rule.format = keywordFormat;
            highlightingRules.append (rule);
            keywordFormat.setFontItalic (false);
        }
        keywordFormat.setFontWeight (QFont::Bold);
        keywordFormat.setForeground (Magenta);
        rule.pattern.setPattern ("((^\\s*|[\\(\\);&`\\|{}!=^]+\\s*|(?<=~|\\.)+\\s+)((if|then|elif|elseif|else|fi|while|do|done|esac)\\s+)*)\\K(?<!\\${)(sudo\\s+)?((kill|killall|torify|proxychains)\\s+)?(adduser|addgroup|apropos|apt|apt-get|aspell|awk|basename|bash|bc|bsdtar|bunzip2|bzip2|cal|cat|cd|cfdisk|chgrp|chmod|chown|chroot|chkconfig|cksum|clear|cmake|cmp|comm|cp|cron|crontab|csplit|curl|cut|cvs|date|dc|dd|ddrescue|df|diff|diff3|dig|dir|dircolors|dirname|dirs|dmesg|dpkg|du|egrep|eject|emacs|env|ethtool|expect|expand|expr|fdformat|fdisk|featherpad|fgrep|file|find|finger|fmt|fold|format|fpad|free|fsck|ftp|function|fuser|gawk|gcc|gio|git|gpg|grep|groups|gunzip|gzip|head|hostname|id|ifconfig|ifdown|ifup|import|install|java|javac|jobs|join|kdialog|kill|killall|less|links|ln|locate|logname|look|lpc|lpr|lprint|lprintd|lprintq|lprm|ls|lsof|lynx|mail|make|makepkg|man|mkdir|mkfifo|mkisofs|mknod|mktemp|more|mount|mtools|mv|mmv|nano|netstat|nice|nl|nohup|nslookup|open|op|pacman|passwd|paste|pathchk|perl|pico|pine|ping|pkill|popd|pr|printcap|printenv|proxychains|ps|pwd|python|qarma|qmake((-qt)?[3-9])*|quota|quotacheck|quotactl|ram|rcp|readarray|reboot|rename|renice|remsync|rev|rm|rmdir|rsync|ruby|screen|scp|sdiff|sed|sftp|shutdown|sleep|slocate|sort|split|ssh|strace|strip|su|sudo|sum|svn|symlink|sync|tail|tar|tee|time|touch|top|torify|traceroute|tr|tsort|tty|type|ulimit|umask|umount|uname|unexpand|uniq|units|unshar|unzip|useradd|usermod|users|usleep|uuencode|uudecode|vdir|vi|vim|vmstat|wall|watch|wc|whereis|which|who|whoami|wget|write|xargs|xeyes|yad|yes|zenity|zip|zypper|7z)(?!(\\.|-|@|#|\\$))\\b");
        rule.format = keywordFormat;
        highlightingRules.append (rule);
    }
    else
        keywordFormat.setFontWeight (QFont::Bold);
    keywordFormat.setForeground (DarkBlue);

    /* types */
    QTextCharFormat typeFormat;
    typeFormat.setForeground (DarkMagenta); //(QColor (102, 0, 0));

    const QStringList keywordPatterns = keywords (Lang);
    for (const QString &pattern : keywordPatterns)
    {
        rule.pattern.setPattern (pattern);
        rule.format = keywordFormat;
        highlightingRules.append (rule);
    }

    if (progLan == "qmake")
    {
        QTextCharFormat qmakeFormat;
        /* qmake test functions */
        qmakeFormat.setForeground (DarkMagenta);
        rule.pattern.setPattern ("\\b(cache|CONFIG|contains|count|debug|defined|equals|error|eval|exists|export|files|for|greaterThan|if|include|infile|isActiveConfig|isEmpty|isEqual|lessThan|load|log|message|mkpath|packagesExist|prepareRecursiveTarget|qtCompileTest|qtHaveModule|requires|system|touch|unset|warning|write_file)(?=\\s*\\()");
        rule.format = qmakeFormat;
        highlightingRules.append (rule);
        /* qmake paths */
        qmakeFormat.setForeground (Blue);
        rule.pattern.setPattern ("\\${1,2}([A-Za-z0-9_]+|\\[[A-Za-z0-9_]+\\]|\\([A-Za-z0-9_]+\\))");
        rule.format = qmakeFormat;
        highlightingRules.append (rule);
    }

    const QStringList typePatterns = types();
    for (const QString &pattern : typePatterns)
    {
        rule.pattern.setPattern (pattern);
        rule.format = typeFormat;
        highlightingRules.append (rule);
    }

    /***********
     * Details *
     ***********/

    /* these are used for all comments */
    commentFormat.setForeground (Red);
    commentFormat.setFontItalic (true);
    /* WARNING: This is used by Fountain's synopses too. */
    noteFormat.setFontWeight (QFont::Bold);
    noteFormat.setFontItalic (true);
    noteFormat.setForeground (DarkRed);

    /* these can also be used inside multiline comments */
    urlFormat.setFontUnderline (true);
    urlFormat.setForeground (Blue);
    urlFormat.setFontItalic (true);

    if (progLan == "c" || progLan == "cpp")
    {
        QTextCharFormat cFormat;

        /* Qt and Gtk+ specific classes */
        cFormat.setFontWeight (QFont::Bold);
        cFormat.setForeground (DarkMagenta);
        if (progLan == "cpp")
            rule.pattern.setPattern ("\\bQ[A-Z][A-Za-z0-9]+(?!(\\.|-|@|#|\\$))\\b");
        else
            rule.pattern.setPattern ("\\bG[A-Za-z]+(?!(\\.|-|@|#|\\$))\\b");
        rule.format = cFormat;
        highlightingRules.append (rule);

        /* Qt's global functions, enums and global colors */
        if (progLan == "cpp")
        {
            cFormat.setFontItalic (true);
            rule.pattern.setPattern ("\\bq(App)(?!(\\@|#|\\$))\\b|\\bq(Abs|Bound|Critical|Debug|Fatal|FuzzyCompare|InstallMsgHandler|MacVersion|Max|Min|Round64|Round|Version|Warning|getenv|putenv|rand|srand|tTrId|unsetenv|_check_ptr|t_set_sequence_auto_mnemonic|t_symbian_exception2Error|t_symbian_exception2LeaveL|t_symbian_throwIfError)(?!(\\.|-|@|#|\\$))\\b");
            rule.format = cFormat;
            highlightingRules.append (rule);

            cFormat.setFontWeight (QFont::Normal);
            rule.pattern.setPattern ("\\bQt\\s*::\\s*[A-Z][A-Za-z0-9_]+(?!(\\.|-|@|#|\\$))\\b");
            rule.format = cFormat;
            highlightingRules.append (rule);
            cFormat.setFontWeight (QFont::Bold);
            cFormat.setFontItalic (false);

            cFormat.setForeground (Magenta);
            rule.pattern.setPattern ("\\bQt\\s*::\\s*(white|black|red|darkRed|green|darkGreen|blue|darkBlue|cyan|darkCyan|magenta|darkMagenta|yellow|darkYellow|gray|darkGray|lightGray|transparent|color0|color1)(?!(\\.|-|@|#|\\$))\\b");
            rule.format = cFormat;
            highlightingRules.append (rule);
        }

        /* preprocess */
        cFormat.setForeground (Blue);
        rule.pattern.setPattern ("^\\s*#\\s*include\\s|^\\s*#\\s*ifdef\\s|^\\s*#\\s*elif\\s|^\\s*#\\s*ifndef\\s|^\\s*#\\s*endif\\b|^\\s*#\\s*define\\s|^\\s*#\\s*undef\\s|^\\s*#\\s*error\\s|^\\s*#\\s*if\\s|^\\s*#\\s*else(?!(\\.|-|@|#|\\$))\\b");
        rule.format = cFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "python")
    {
        QTextCharFormat pFormat;
        pFormat.setFontWeight (QFont::Bold);
        pFormat.setForeground (DarkMagenta);
        rule.pattern.setPattern ("\\bself(?!(@|\\$))\\b");
        rule.format = pFormat;
        highlightingRules.append (rule);
    }
    else if (Lang == "javascript" || progLan == "qml")
    {
        QTextCharFormat ft;

        /* after dot (may override keywords) */
        ft.setForeground (Blue);
        ft.setFontItalic (true);
        rule.pattern.setPattern ("(?<=\\.)\\s*[A-Za-z0-9_\\$]*[A-Za-z][A-Za-z0-9_\\$]*(?=\\s*\\()"); // before parentheses
        rule.format = ft;
        highlightingRules.append (rule);
        ft.setFontItalic (false);
        rule.pattern.setPattern ("(?<=\\.)\\s*[A-Za-z0-9_\\$]*[A-Za-z][A-Za-z0-9_\\$]*(?!\\s*\\()(?![A-Za-z0-9_\\$])"); // not before parentheses
        rule.format = ft;
        highlightingRules.append (rule);

        /* numbers */
        ft.setForeground (Brown);
        rule.pattern.setPattern ("(?<=^|[^\\w\\d@#\\$\\.])("
                                 "\\d*\\.\\d+|\\d+\\.|(\\d*\\.?\\d+|\\d+\\.)(e|E)(\\+|-)?\\d+"
                                 "|"
                                 "([1-9]\\d*|0(o|O)[0-7]+|0[xX][0-9a-fA-F]+|0[bB][01]+)n?"
                                 "|"
                                 "0[0-7]*"
                                 ")(?=[^\\w\\d\\.]|$)");
        rule.format = ft;
        highlightingRules.append (rule);

        if (progLan == "qml")
        {
            ft.setFontWeight (QFont::Bold);
            ft.setForeground (DarkMagenta);
            rule.pattern.setPattern ("\\b(?<!(@|#|\\$))(Qt([A-Za-z]+)?|Accessible|AnchorAnimation|AnchorChanges|AnimatedImage|AnimatedSprite|Animation|AnimationController|Animator|Behavior|BorderImage|Canvas|CanvasGradient|CanvasImageData|CanvasPixelArray|ColorAnimation|Column|Context2D|DoubleValidator|Drag|DragEvent|DropArea|EnterKey|Flickable|Flipable|Flow|FocusScope|FontLoader|FontMetrics|Gradient|GradientStop|Grid|GridMesh|GridView|Image|IntValidator|Item|ItemGrabResult|KeyEvent|KeyNavigation|Keys|LayoutMirroring|ListView|Loader|Matrix4x4|MouseArea|MouseEvent|MultiPointTouchArea|NumberAnimation|OpacityAnimator|OpenGLInfo|ParallelAnimation|ParentAnimation|ParentChange|Path|PathAnimation|PathArc|PathAttribute|PathCubic|PathCurve|PathElement|PathInterpolator|PathLine|PathPercent|PathQuad|PathSvg|PathView|PauseAnimation|PinchArea|PinchEvent|Positioner|PropertyAction|PropertyAnimation|PropertyChanges|Rectangle|RegExpValidator|Repeater|Rotation|RotationAnimation|RotationAnimator|Row|Scale|ScaleAnimator|ScriptAction|SequentialAnimation|ShaderEffect|ShaderEffectSource|Shortcut|SmoothedAnimation|SpringAnimation|Sprite|SpriteSequence|State|StateChangeScript|StateGroup|SystemPalette|Text|TextEdit|TextInput|TextMetrics|TouchPoint|Transform|Transition|Translate|UniformAnimator|Vector3dAnimation|ViewTransition|WheelEvent|XAnimator|YAnimator|CloseEvent|ColorDialog|ColumnLayout|Dialog|FileDialog|FontDialog|GridLayout|Layout|MessageDialog|RowLayout|StackLayout|LocalStorage|Screen|SignalSpy|TestCase|Window|XmlListModel|XmlRole|Action|ApplicationWindow|BusyIndicator|Button|Calendar|CheckBox|ComboBox|ExclusiveGroup|GroupBox|Label|Menu|MenuBar|MenuItem|MenuSeparator|ProgressBar|RadioButton|ScrollView|Slider|SpinBox|SplitView|Stack|StackView|StackViewDelegate|StatusBar|Switch|Tab|TabView|TableView|TableViewColumn|TextArea|TextField|ToolBar|ToolButton|TreeView|Affector|Age|AngleDirection|Attractor|CumulativeDirection|CustomParticle|Direction|EllipseShape|Emitter|Friction|Gravity|GroupGoal|ImageParticle|ItemParticle|LineShape|MaskShape|Particle|ParticleGroup|ParticlePainter|ParticleSystem|PointDirection|RectangleShape|Shape|SpriteGoal|TargetDirection|TrailEmitter|Turbulence|Wander|Timer)(?!(\\-|@|#|\\$))\\b");
            rule.format = ft;
            highlightingRules.append (rule);
        }
    }
    else if (progLan == "xml")
    {
        xmlLt.setPattern ("(<|&lt;)");
        xmlGt.setPattern ("(>|&gt;)");

        QTextCharFormat xmlElementFormat;
        xmlElementFormat.setFontWeight (QFont::Bold);
        xmlElementFormat.setForeground (Violet);
        /* after </ or before /> */
        rule.pattern.setPattern ("\\s*(<|&lt;)/?[A-Za-z0-9_\\-:]+|\\s*(<|&lt;)!(DOCTYPE|ENTITY|ELEMENT|ATTLIST|NOTATION)\\s|\\s*/?(>|&gt;)");
        rule.format = xmlElementFormat;
        highlightingRules.append (rule);

        QTextCharFormat xmlAttributeFormat;
        xmlAttributeFormat.setFontItalic (true);
        xmlAttributeFormat.setForeground (Blue);
        /* before = */
        rule.pattern.setPattern ("\\s+[A-Za-z0-9_\\-:]+(?=\\s*\\=\\s*(\"|&quot;|\'))");
        rule.format = xmlAttributeFormat;
        highlightingRules.append (rule);

        /* <?xml ... ?> */
        rule.pattern.setPattern ("^\\s*<\\?xml\\s+(?=.*\\?>)|\\s*\\?>");
        rule.format = keywordFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "changelog")
    {
        /* before colon */
        rule.pattern.setPattern ("^\\s+\\*\\s+[^:]+:");
        rule.format = keywordFormat;
        highlightingRules.append (rule);

        QTextCharFormat asteriskFormat;
        asteriskFormat.setForeground (DarkMagenta);
        /* the first asterisk */
        rule.pattern.setPattern ("^\\s+\\*\\s+");
        rule.format = asteriskFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern (urlPattern.pattern());
        rule.format = urlFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "sh" || progLan == "makefile" || progLan == "cmake"
             || progLan == "perl" || progLan == "ruby")
    {
        /* # is the sh comment sign when it doesn't follow a character */
        if (progLan == "sh" || progLan == "makefile" || progLan == "cmake")
        {
            rule.pattern.setPattern ("(?<=^|\\s|;)#.*");

            if (progLan == "sh")
            {
                /* Kate uses something like: "<<(?:\\s*)([\\\\]{0,1}[^\\s]+)"
                "<<-" can be used instead of "<<" */
                hereDocDelimiter.setPattern ("<<-?(?:\\s*)(\\\\{0,1}[A-Za-z0-9_]+)|<<-?(?:\\s*)(\'[A-Za-z0-9_]+\')|<<-?(?:\\s*)(\"[A-Za-z0-9_]+\")");
            }
        }
        else if (progLan == "perl")
        {
            rule.pattern.setPattern ("(?<!\\$)#.*"); // $# isn't a comment

            /* without space after "<<" and with ";" at the end */
            //hereDocDelimiter.setPattern ("<<([A-Za-z0-9_]+)(?:;)|<<(\'[A-Za-z0-9_]+\')(?:;)|<<(\"[A-Za-z0-9_]+\")(?:;)");
            /* can contain spaces inside quote marks or backquotes and usually has ";" at the end */
            hereDocDelimiter.setPattern ("<<(?![0-9]+\\b)([A-Za-z0-9_]+)(?:;{0,1})|<<(?:\\s*)(\'[A-Za-z0-9_\\s]+\')(?:;{0,1})|<<(?:\\s*)(\"[A-Za-z0-9_\\s]+\")(?:;{0,1})|<<(?:\\s*)(`[A-Za-z0-9_\\s]+`)(?:;{0,1})");
        }
        else
        {
            rule.pattern.setPattern ("#.*");

            if (progLan == "ruby")
                hereDocDelimiter.setPattern ("<<(?:-|~){0,1}([A-Za-z0-9_]+)|<<(?:-|~){0,1}(\'[A-Za-z0-9_]+\')|<<(?:-|~){0,1}(\"[A-Za-z0-9_]+\")");
        }
        rule.format = commentFormat;
        highlightingRules.append (rule);

        QTextCharFormat shFormat;

        if (progLan == "sh" || progLan == "makefile" || progLan == "cmake")
        {
            /* make parentheses, braces and ; neutral as they were in keyword patterns */
            rule.pattern.setPattern ("[\\(\\){};]");
            rule.format = neutralFormat;
            highlightingRules.append (rule);

            shFormat.setForeground (Blue);
            /* words before = */
             if (progLan == "sh")
                 rule.pattern.setPattern ("\\b[A-Za-z0-9_]+(?=\\=)");
             else
                 rule.pattern.setPattern ("\\b[A-Za-z0-9_]+\\s*(?=(\\+|\\?){0,1}\\=)");
            rule.format = shFormat;
            highlightingRules.append (rule);

            /* but don't format a word before =
               if it follows a dash */
            rule.pattern.setPattern ("-+[^\\s\\\"\\\']+(?=\\=)");
            rule.format = neutralFormat;
            highlightingRules.append (rule);
        }

        if (progLan == "makefile" || progLan == "cmake")
        {
            shFormat.setForeground (DarkYellow);
            /* automake/autoconf variables */
            rule.pattern.setPattern ("@[A-Za-z0-9_-]+@|^[a-zA-Z0-9_-]+\\s*(?=:)");
            rule.format = shFormat;
            highlightingRules.append (rule);
        }

        if (progLan == "perl")
        {
            shFormat.setForeground (DarkYellow);
            rule.pattern.setPattern ("[%@\\$]");
            rule.format = shFormat;
            highlightingRules.append (rule);

            /* numbers (the underline separator is also included) */
            shFormat.setForeground (Brown);
            rule.pattern.setPattern ("(?<=^|[^\\w\\d\\.])("
                                     "(\\d|\\d_\\d)*\\.(\\d|\\d_\\d)+|(\\d|\\d_\\d)+\\." // floating point
                                     "|"
                                     "((\\d|\\d_\\d)*\\.?(\\d|\\d_\\d)+|(\\d|\\d_\\d)+\\.)(e|E)(\\+|-)?(\\d|\\d_\\d)+" // scientific
                                     "|"
                                     "[1-9]_?(\\d|\\d_\\d)*" // digits
                                     "|"
                                     "0[xX]([0-9a-fA-F]|[0-9a-fA-F]_[0-9a-fA-F])+" // hexadecimal
                                     "|"
                                     "0([0-7]|[0-7]_[0-7])*" // octal
                                     "|"
                                     "0[bB]([01]|[01]_[01])+" // binary
                                     ")(?=[^\\w\\d\\.]|$)");
            rule.format = shFormat;
            highlightingRules.append (rule);
        }
        else if (progLan == "sh" || progLan == "makefile" || progLan == "cmake")
        {
            shFormat.setForeground (DarkMagenta);
            /* operators */
            rule.pattern.setPattern ("[=\\+\\-*/%<>&`\\|~\\^\\!,]|\\s+-eq\\s+|\\s+-ne\\s+|\\s+-gt\\s+|\\s+-ge\\s+|\\s+-lt\\s+|\\s+-le\\s+|\\s+-z\\s+");
            rule.format = shFormat;
            highlightingRules.append (rule);

            shFormat.setFontWeight (QFont::Bold);
            /* brackets */
            rule.pattern.setPattern ("\\s+\\[{1,2}\\s+|^\\[{1,2}\\s+|\\s+\\]{1,2}\\s+|\\s+\\]{1,2}$|\\s+\\]{1,2}\\s*(?=;)");
            rule.format = shFormat;
            highlightingRules.append (rule);
        }
        else if (progLan == "ruby")
        {
            /* numbers */
            shFormat.setForeground (Brown);
            rule.pattern.setPattern ("(?<![a-zA-Z0-9_@$%])\\d+(\\.\\d+)?(?=[^\\d]|$)");
            rule.format = shFormat;
            highlightingRules.append (rule);

            /* built-in functions */
            shFormat.setFontWeight (QFont::Bold);
            shFormat.setForeground (Magenta);
            rule.pattern.setPattern ("\\b(Array|Float|Integer|String|atan2|autoload|binding|callcc|caller|catch|chomp\\!?|cos|dump|eval|exec|exit\\!?|exp|fail|format|frexp|garbage_collect|gets|gsub\\!?|ldexp|load|log|log10|open|p|print|printf|putc|puts|raise|rand|readline|readlines|require|restore|scan|select|set_trace_func|sin|singleton_method_added|sleep|split|sprintf|sqrt|srand|sub\\!?|syscall|system|tan|test|throw|trace_var|trap|untrace_var)(?!(\\w|\\!|\\?))"
                                     "|"
                                     "\\b(abort|at_exit|binding|chop\\!?|egid|euid|fork|getpgrp|getpriority|gid|global_variables|kill|lambda|local_variables|loop|pid|ppid|proc|setpgid|setpgrp|setpriority|setsid|uid|wait|wait2|waitpid|waitpid2)(?!(\\w|\\!|\\?))"
                                     "|"
                                     "(?<=\\.)(assoc|at|clear|collect\\!?|compact\\!?|concat|delete\\!?|delete_at|delete_if|each|each_index|fill|first|flatten\\!?|index|indexes|indices|join|last|length|nitems|pack|pop|push|rassoc|replace|reverse\\!?|reverse_each|rindex|shift|size\\??|slice\\!?|sort\\!?|to_a|to_ary|to_s|uniq\\!?|unshift)(?!(\\w|\\!|\\?))"
                                     "|"
                                     "(?<=\\.)(atime|blksize|backtrace|blocks|ceil|close|ctime|default|dev|each_key|each_pair|each_value|exception|fetch|floor|ftype|id2name|ino|invert|keys|message|mode|mtime|new|nlink|rdev|read|rehash|reject\\!?|rewind|round|seek|set_backtrace|store|superclass|tell|to_f|to_i|update|values)(?!(\\w|\\!|\\?))"
                                     "|"
                                     "(?<=\\.)(arity|binmode|call|clone|close_read|close_write|each_byte|each_line|eof\\??|fcntl|fileno|flush|getc|ioctl|isatty|lineno|offset|pos|post_match|pre_match|read|readchar|reopen|stat|string|sync|sysread|syswrite|to_io|to_proc|ungetc|write)(?!(\\w|\\!|\\?))"
                                     "|"
                                     "(?<=\\.)(ancestors|class_eval|class_variables|const_get|const_set|constants|included_modules|instance_methods|module_eval|name|private_class_method|private_instance_methods|protected_instance_methods|public_class_method|public_instance_methods)(?!(\\w|\\!|\\?))"
                                     "|"
                                     "(?<=\\.)(__id__|__send__|abs|coerce|display|divmod|dup|extend|freeze|hash|id|inspect|instance_eval|instance_variables|method|method_missing|methods|modulo|private_methods|protected_methods|public_methods|remainder|send|singleton_methods|taint|type|untaint)(?!(\\w|\\!|\\?))"
                                     "|"
                                     "(?<=\\.)(add|capitalize\\!?|center|count|crypt|downcase\\!?|hex|intern|kcode|list|ljust|match|members|next\\!?|oct|rjust|source|squeeze\\!?|strip\\!?|succ\\!?|sum|swapcase\\!?|to_str|tr\\!?|tr_s\\!?|unpack|upcase\\!?|upto)(?!(\\w|\\!|\\?))"
                                     "|"
                                     "(?<=\\.)(abort_on_exception|asctime|day|detect|each_with_index|entries|find|find_all|gmtime|grep|hour|isdst|localtime|map\\!?|max|mday|min|mon|month|priority|run|safe_level|sec|status|strftime|tv_sec|tv_usec|usec|utc|value\\??|wday|yday|year|wakeup|zone)(?!(\\w|\\!|\\?))"
                                     "|"
                                     "(?<=\\.)(begin|class|end)(?!(\\w|\\!|\\?))"
                                     "|"
                                     "(?<=\\.)(alive|block_given|blockdev|casefold|chardev|closed|const_defined|directory|empty|eql|equal|exclude_end|executable|executable_real|exist|exists|file|finite|frozen|gmt|grpowned|has_key|has_value|include|infinite|instance_of|integer|is_a|iterator|key|kind_of|member|method_defined|nan|nil|nonzero|owned|pipe|readable|readable_real|respond_to|setgid|setuid|socket|sticky|stop|symlink|tainted|tty|writable|writable_real|zero)\\?(?!(\\w|\\!|\\?))");
            rule.format = shFormat;
            highlightingRules.append (rule);
        }
    }
    else if (progLan == "diff")
    {
        QTextCharFormat diffMinusFormat;
        diffMinusFormat.setForeground (Red);
        rule.pattern.setPattern ("^\\-.*");
        rule.format = diffMinusFormat;
        highlightingRules.append (rule);

        QTextCharFormat diffPlusFormat;
        diffPlusFormat.setForeground (Blue);
        rule.pattern.setPattern ("^\\+.*");
        rule.format = diffPlusFormat;
        highlightingRules.append (rule);

        /*diffMinusFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("^\\-{3}.*");
        rule.format = diffMinusFormat;
        highlightingRules.append (rule);

        diffPlusFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("^\\+{3}.*");
        rule.format = diffPlusFormat;
        highlightingRules.append (rule);*/

        QTextCharFormat diffLinesFormat;
        diffLinesFormat.setForeground (DarkBlue);
        diffLinesFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("^diff.*");
        rule.format = diffLinesFormat;
        highlightingRules.append (rule);

        diffLinesFormat.setForeground (DarkGreenAlt);
        rule.pattern.setPattern ("^@{2}[\\d,\\-\\+\\s]+@{2}");
        rule.format = diffLinesFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "log")
    {
        /* example:
         * May 19 02:01:44 debian sudo:
         *   blue  green  magenta bold */
        QTextCharFormat logFormat = neutralFormat;
        logFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("\\b(\\d{4}-\\d{2}-\\d{2}|\\d{2}/(\\d{2}|[A-Za-z]{3})/\\d{4}|\\d{4}/(\\d{2}|[A-Za-z]{3})/\\d{2}|[A-Za-z]{3}\\s+\\d{1,2})(T|\\s)\\d{2}:\\d{2}(:\\d{2}((\\+|-)\\d+)?)?(AM|PM|am|pm)?\\s+[A-Za-z0-9_\\s]+(?=\\s*:)");
        rule.format = logFormat;
        highlightingRules.append (rule);

        QTextCharFormat logFormat1;
        logFormat1.setForeground (Magenta);
        rule.pattern.setPattern ("\\b(\\d{4}-\\d{2}-\\d{2}|\\d{2}/(\\d{2}|[A-Za-z]{3})/\\d{4}|\\d{4}/(\\d{2}|[A-Za-z]{3})/\\d{2}|[A-Za-z]{3}\\s+\\d{1,2})(T|\\s)\\d{2}:\\d{2}(:\\d{2}((\\+|-)\\d+)?)?(AM|PM|am|pm)?\\s+[A-Za-z0-9_]+(?=\\s|$|:)");
        rule.format = logFormat1;
        highlightingRules.append (rule);

        QTextCharFormat logDateFormat;
        logDateFormat.setFontWeight (QFont::Bold);
        logDateFormat.setForeground (Blue);
        rule.pattern.setPattern ("\\b(\\d{4}-\\d{2}-\\d{2}|\\d{2}/(\\d{2}|[A-Za-z]{3})/\\d{4}|\\d{4}/(\\d{2}|[A-Za-z]{3})/\\d{2}|[A-Za-z]{3}\\s+\\d{1,2})(?=(T|\\s)\\d{2}:\\d{2}(:\\d{2}((\\+|-)\\d+)?)?(AM|PM|am|pm)?\\b)");
        rule.format = logDateFormat;
        highlightingRules.append (rule);

        QTextCharFormat logTimeFormat;
        logTimeFormat.setFontWeight (QFont::Bold);
        logTimeFormat.setForeground (DarkGreenAlt);
        rule.pattern.setPattern ("(?<=T|\\s)\\d{2}:\\d{2}(:\\d{2}((\\+|-)\\d+)?)?(AM|PM|am|pm)?\\b");
        rule.format = logTimeFormat;
        highlightingRules.append (rule);

        QTextCharFormat logInOutFormat;
        logInOutFormat.setFontWeight (QFont::Bold);
        logInOutFormat.setForeground (Brown);
        rule.pattern.setPattern ("\\s+IN(?=\\s*\\=)|\\s+OUT(?=\\s*\\=)");
        rule.format = logInOutFormat;
        highlightingRules.append (rule);

        QTextCharFormat logRootFormat;
        logRootFormat.setFontWeight (QFont::Bold);
        logRootFormat.setForeground (Red);
        rule.pattern.setPattern ("\\broot\\b");
        rule.format = logRootFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "srt")
    {
        QTextCharFormat srtFormat;
        srtFormat.setFontWeight (QFont::Bold);

        /* <...> */
        srtFormat.setForeground (Violet);
        rule.pattern.setPattern ("</?[A-Za-z0-9_#\\s\"\\=]+>");
        rule.format = srtFormat;
        highlightingRules.append (rule);

        /* hh:mm:ss,ttt */
        srtFormat = neutralFormat;
        srtFormat.setFontItalic (true);
        srtFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("^\\s*\\d{2}:\\d{2}:\\d{2},\\d{3}\\s+-->\\s+\\d{2}:\\d{2}:\\d{2},\\d{3}\\s*$");
        rule.format = srtFormat;
        highlightingRules.append (rule);

        /* subtitle line */
        srtFormat.setForeground (Red);
        rule.pattern.setPattern ("^\\d+$");
        rule.format = srtFormat;
        highlightingRules.append (rule);

        /* hh */
        srtFormat.setForeground (Blue);
        rule.pattern.setPattern ("^\\s*\\d{2}(?=:\\d{2}:\\d{2},\\d{3}\\s+-->\\s+\\d{2}:\\d{2}:\\d{2},\\d{3}\\s*$)");
        rule.format = srtFormat;
        highlightingRules.append (rule);
        rule.pattern.setPattern ("^\\s*\\d{2}:\\d{2}:\\d{2},\\d{3}\\s+-->\\s+\\K\\d{2}(?=:\\d{2}:\\d{2},\\d{3}\\s*$)");
        highlightingRules.append (rule);

        /* mm */
        srtFormat.setForeground (DarkGreenAlt);
        rule.pattern.setPattern ("^\\s*\\d{2}:\\K\\d{2}(?=:\\d{2},\\d{3}\\s+-->\\s+\\d{2}:\\d{2}:\\d{2},\\d{3}\\s*$)");
        rule.format = srtFormat;
        highlightingRules.append (rule);
        rule.pattern.setPattern ("^\\s*\\d{2}:\\d{2}:\\d{2},\\d{3}\\s+-->\\s+\\d{2}:\\K\\d{2}(?=:\\d{2},\\d{3}\\s*$)");
        highlightingRules.append (rule);

        /* ss */
        srtFormat.setForeground (Brown);
        rule.pattern.setPattern ("^\\s*\\d{2}:\\d{2}:\\K\\d{2}(?=,\\d{3}\\s+-->\\s+\\d{2}:\\d{2}:\\d{2},\\d{3}\\s*$)");
        rule.format = srtFormat;
        highlightingRules.append (rule);
        rule.pattern.setPattern ("^\\s*\\d{2}:\\d{2}:\\d{2},\\d{3}\\s+-->\\s+\\d{2}:\\d{2}:\\K\\d{2}(?=,\\d{3}\\s*$)");
        highlightingRules.append (rule);
    }
    else if (progLan == "desktop" || progLan == "config" || progLan == "theme")
    {
        QTextCharFormat desktopFormat = neutralFormat;
        if (progLan == "config")
        {
            desktopFormat.setFontWeight (QFont::Bold);
            desktopFormat.setFontItalic (true);
            /* color values */
            rule.pattern.setPattern ("#([A-Fa-f0-9]{3}){1,2}(?![A-Za-z0-9_]+)|#([A-Fa-f0-9]{3}){2}[A-Fa-f0-9]{2}(?![A-Za-z0-9_]+)");
            rule.format = desktopFormat;
            highlightingRules.append (rule);
            desktopFormat.setFontItalic (false);
            desktopFormat.setFontWeight (QFont::Normal);

            /* URLs */
            rule.pattern = urlPattern;
            rule.format = urlFormat;
            highlightingRules.append (rule);
        }

        desktopFormat.setForeground (DarkMagenta);
        rule.pattern.setPattern ("^[^\\=\\[]+=|^[^\\=\\[]+\\[.*\\]=|;|/|%|\\+|-");
        rule.format = desktopFormat;
        highlightingRules.append (rule);

        desktopFormat = neutralFormat;
        desktopFormat.setFontWeight (QFont::Bold);
        /* [...] */
        rule.pattern.setPattern ("^\\[.*\\]$");
        rule.format = desktopFormat;
        highlightingRules.append (rule);

        desktopFormat.setForeground (Blue);
        /* [...] and before = (like ...[en]=)*/
        rule.pattern.setPattern ("^[^\\=\\[]+\\[.*\\](?=\\s*\\=)");
        rule.format = desktopFormat;
        highlightingRules.append (rule);

        desktopFormat.setForeground (DarkGreenAlt);
        /* before = and [] */
        rule.pattern.setPattern ("^[^\\=\\[]+(?=(\\[.*\\])*\\s*\\=)");
        rule.format = desktopFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "yaml")
    {
        rule.pattern.setPattern ("(?<=^|\\s)#.*");
        rule.format = commentFormat;
        highlightingRules.append (rule);

        QTextCharFormat yamlFormat;

        /* keys (a key shouldn't start with a quote but can contain quotes) */
        yamlFormat.setForeground (Blue);
        rule.pattern.setPattern ("\\s*[^\\s\"\'#][^:,#]*:(\\s+|$)");
        rule.format = yamlFormat;
        highlightingRules.append (rule);

        /* values */
        // NOTE: This is the first time I use \K with Qt and it seems to work well.
        yamlFormat.setForeground (Violet);
        rule.pattern.setPattern ("[^:#]*:\\s+\\K[^#]+");
        rule.format = yamlFormat;
        highlightingRules.append (rule);

        /* non-value numbers (including the scientific notation) */
        yamlFormat.setForeground (Brown);
        rule.pattern.setPattern ("^((\\s*-\\s)+)?\\s*\\K([-+]?(\\d*\\.?\\d+|\\d+\\.)((e|E)(\\+|-)?\\d+)?|0[xX][0-9a-fA-F]+)\\s*(?=(#|$))");
        rule.format = yamlFormat;
        highlightingRules.append (rule);

        /* lists */
        yamlFormat.setForeground (DarkBlue);
        yamlFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("^(\\s*-(\\s|$))+");
        rule.format = yamlFormat;
        highlightingRules.append (rule);

        /* booleans */
        yamlFormat.setForeground (DarkBlue);
        yamlFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("^((\\s*-\\s)+)?\\s*\\K(true|false|yes|no|TRUE|FALSE|YES|NO|True|False|Yes|No)\\s*(?=(#|$))");
        rule.format = yamlFormat;
        highlightingRules.append (rule);

        /* the start of a literal block (-> yamlLiteralBlock()) */
        codeBlockFormat.setForeground (DarkMagenta);
        codeBlockFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("^(?!#)(?:(?!\\s#).)*\\s+\\K(\\||>)-?\\s*(?=\\s#|$)");
        rule.format = codeBlockFormat;
        highlightingRules.append (rule);

        yamlFormat.setForeground (Verda);
        rule.pattern.setPattern ("^---.*");
        rule.format = yamlFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "fountain")
    {
        QTextCharFormat fFormat;

        /* sections */
        fFormat.setForeground (DarkRed);
        fFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("^\\s*#.*");
        rule.format = fFormat;
        highlightingRules.append (rule);

        /* centered */
        rule.pattern.setPattern (">|<");
        rule.format = fFormat;
        highlightingRules.append (rule);

        /* synopses */
        fFormat.setFontItalic (true);
        rule.pattern.setPattern ("^\\s*=.*");
        rule.format = fFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "url")
    {
        rule.pattern.setPattern (urlPattern.pattern());
        rule.format = urlFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "gtkrc")
    {
        QTextCharFormat gtkrcFormat;
        gtkrcFormat.setFontWeight (QFont::Bold);
        /* color value format (#xyz) */
        /*gtkrcFormat.setForeground (DarkGreenAlt);
        rule.pattern.setPattern ("#([A-Fa-f0-9]{3}){1,2}(?![A-Za-z0-9_]+)|#([A-Fa-f0-9]{3}){2}[A-Fa-f0-9]{2}(?![A-Za-z0-9_]+)");
        rule.format = gtkrcFormat;
        highlightingRules.append (rule);*/

        gtkrcFormat.setForeground (Blue);
        rule.pattern.setPattern ("(fg|bg|base|text)(\\[NORMAL\\]|\\[PRELIGHT\\]|\\[ACTIVE\\]|\\[SELECTED\\]|\\[INSENSITIVE\\])");
        rule.format = gtkrcFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "markdown")
    {
        /* Inline code is almost like a single-line quote.
           "`" will be distinguished from "```" at multiLineQuote(). */
        quoteMark.setPattern ("`");

        blockQuoteFormat.setForeground (DarkGreen);
        codeBlockFormat.setForeground (DarkRed);
        QTextCharFormat markdownFormat;

        /* footnotes */
        markdownFormat.setFontWeight (QFont::Bold);
        markdownFormat.setForeground (DarkBlue);
        markdownFormat.setFontItalic (true);
        rule.pattern.setPattern ("\\[\\^[^\\]]+\\]");
        rule.format = markdownFormat;
        highlightingRules.append (rule);
        markdownFormat.setFontItalic (false);

        /* horizontal rules */
        markdownFormat.setForeground (DarkMagenta);
        rule.pattern.setPattern ("^ {0,3}(\\* {0,2}){3,}\\s*$"
                                 "|"
                                 "^ {0,3}(- {0,2}){3,}\\s*$"
                                 "|"
                                 "^ {0,3}(\\= {0,2}){3,}\\s*$");
        rule.format = markdownFormat;
        highlightingRules.append (rule);

        /*
           links:
           [link text] [link]
           [link text] (http://example.com "Title")
           [link text]: http://example.com
           <http://example.com>
        */
        rule.pattern.setPattern ("\\[[^\\]\\^]*\\]\\s*\\[[^\\]\\s]*\\]"
                                 "|"
                                 "\\[[^\\]\\^]*\\]\\s*\\(\\s*[^\\)\\(\\s]+(\\s+\\\".*\\\")*\\s*\\)"
                                 "|"
                                 "\\[[^\\]\\^]*\\]:\\s+\\s*[^\\)\\(\\s]+(\\s+\\\".*\\\")*"
                                 "|"
                                 "<([A-Za-z0-9_]+://[A-Za-z0-9_.+/\\?\\=~&%#\\-:\\(\\)\\[\\]]+|[A-Za-z0-9_.\\-]+@[A-Za-z0-9_\\-]+\\.[A-Za-z0-9.]+)>");
        rule.format = urlFormat;
        highlightingRules.append (rule);

        /*
           images:
           ![image](image.jpg "An image")
           ![Image][1]
           [1]: /path/to/image "alt text"
        */
        markdownFormat.setFontWeight (QFont::Normal);
        markdownFormat.setForeground (Violet);
        markdownFormat.setFontUnderline (true);
        rule.pattern.setPattern ("\\!\\[[^\\]\\^]*\\]\\s*"
                                 "(\\(\\s*[^\\)\\(\\s]+(\\s+\\\".*\\\")*\\s*\\)|\\s*\\[[^\\]]*\\])");
        rule.format = markdownFormat;
        highlightingRules.append (rule);
        markdownFormat.setFontUnderline (false);

        /* headings */
        markdownFormat.setFontWeight (QFont::Bold);
        markdownFormat.setForeground (Verda);
        rule.pattern.setPattern ("^#+\\s+.*");
        rule.format = markdownFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "reST")
    {
        /* For bold, italic, verbatim and link

           possible characters before the start:  ([{<:'"/
           possible characters after the end:     )]}>.,;:-'"/\
        */

        codeBlockFormat.setForeground (DarkRed);
        QTextCharFormat reSTFormat;

        /* headings */
        reSTFormat.setFontWeight (QFont::Bold);
        reSTFormat.setForeground (Blue);
        rule.pattern.setPattern ("^([^\\s\\w:])\\1{2,}$");
        rule.format = reSTFormat;
        highlightingRules.append (rule);

        /* lists */
        reSTFormat.setFontWeight (QFont::Bold);
        reSTFormat.setForeground (DarkMagenta);
        rule.pattern.setPattern ("^\\s*(\\*|-|\\+|#\\.|[0-9]+\\.|[0-9]+\\)|\\([0-9]+\\)|[a-zA-Z]\\.|[a-zA-Z]\\)|\\([a-zA-Z]\\))\\s+");
        rule.format = reSTFormat;
        highlightingRules.append (rule);


        /* verbatim */
        quoteFormat.setForeground (Blue);
        rule.pattern.setPattern ("(?<=[\\s\\(\\[{<:'\\\"/]|^)``"
                                 "([^`\\s]((?!``).)*[^`\\s]"
                                 "|"
                                 "[^`\\s])"
                                 "``(?=[\\s\\)\\]}>.,;:\\-'\\\"/\\\\]|$)");
        rule.format = quoteFormat;
        highlightingRules.append (rule);

        /* links */
        reSTFormat = urlFormat;
        rule.pattern.setPattern ("(?<=[\\s\\(\\[{<:'\\\"/]|^)"
                                 "(`(\\S+|\\S((?!`_ ).)*\\S)`|\\w+|\\[\\w+\\])"
                                 "_(?=[\\s\\)\\]}>.,;:\\-'\\\"/\\\\]|$)");
        rule.format = reSTFormat;
        highlightingRules.append (rule);

        reSTFormat = neutralFormat;

        /* bold */
        reSTFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("(?<=[\\s\\(\\[{<:'\\\"/]|^)\\*\\*"
                                 "([^*\\s]|[^*\\s][^*]*[^*\\s])"
                                 "\\*\\*(?=[\\s\\)\\]}>.,;:\\-'\\\"/\\\\]|$)");
        rule.format = reSTFormat;
        highlightingRules.append (rule);
        reSTFormat.setFontWeight (QFont::Normal);

        /* italic */
        reSTFormat.setFontItalic (true);
        rule.pattern.setPattern ("(?<=[\\s\\(\\[{<:'\\\"/]|^)\\*"
                                 "([^*\\s]|[^*\\s]+[^*]*[^*\\s])"
                                 "\\*(?=[\\s\\)\\]}>.,;:\\-'\\\"/\\\\]|$)");
        rule.format = reSTFormat;
        highlightingRules.append (rule);
        reSTFormat.setFontItalic (false);

        /* labels and substitutions (".. _X:" and ".. |X| Y::") */
        reSTFormat.setForeground (Violet);
        rule.pattern.setPattern ("^\\s*\\.{2} _[\\w\\s\\-+]*:(?!\\S)");
        rule.format = reSTFormat;
        highlightingRules.append (rule);
        rule.pattern.setPattern ("^\\s*\\.{2} \\|[\\w\\s]+\\|\\s+\\w+::(?!\\S)");
        rule.format = reSTFormat;
        highlightingRules.append (rule);

        /* aliases ("|X|") */
        rule.pattern.setPattern ("(?<=[\\s\\(\\[{<:'\\\"/]|^)"
                                 "\\|(?!\\s)[^|]+\\|"
                                 "(?=[\\s\\)\\]}>.,;:\\-'\\\"/\\\\]|$)");
        rule.format = reSTFormat;
        highlightingRules.append (rule);

        /* footnotes (".. [#X]") */
        rule.pattern.setPattern ("^\\s*\\.{2} (\\[(\\w|\\s|-|\\+|\\*)+\\]|\\[#(\\w|\\s|-|\\+)*\\])\\s+");
        rule.format = reSTFormat;
        highlightingRules.append (rule);

        /* references */
        reSTFormat.setForeground (Brown);
        rule.pattern.setPattern (":[\\w\\-+]+:`[^`]*`");
        rule.format = reSTFormat;
        highlightingRules.append (rule);

        /* ".. X::" (like literalinclude) */
        reSTFormat.setFontWeight (QFont::Bold);
        reSTFormat.setForeground (DarkGreen);
        //rule.pattern.setPattern ("^\\s*\\.{2} literalinclude::(?!\\S)");
        rule.pattern.setPattern ("^\\s*\\.{2} ((\\w|-)+::(?!\\S)|code-block::)");
        rule.format = reSTFormat;
        highlightingRules.append (rule);

        /* ":X:" */
        reSTFormat.setFontWeight (QFont::Normal);
        reSTFormat.setForeground (DarkMagenta);
        rule.pattern.setPattern ("^\\s*:[^:]+:(?!\\S)");
        rule.format = reSTFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "lua")
    {
        QTextCharFormat luaFormat;
        luaFormat.setFontWeight (QFont::Bold);
        luaFormat.setFontItalic (true);
        luaFormat.setForeground (DarkMagenta);
        rule.pattern.setPattern ("\\bos(?=\\.)");
        rule.format = luaFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "m3u")
    {
        QTextCharFormat plFormat = neutralFormat;
        plFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("^#EXTM3U\\b");
        rule.format = plFormat;
        highlightingRules.append (rule);

        /* after "," */
        plFormat.setFontWeight (QFont::Normal);
        plFormat.setForeground (DarkRed);
        rule.pattern.setPattern ("^#EXTINF\\s*:[^,]*,\\K.*"); // "^#EXTINF\\s*:\\s*-*\\d+\\s*,.*|^#EXTINF\\s*:\\s*,.*"
        rule.format = plFormat;
        highlightingRules.append (rule);

        /* before "," and after "EXTINF:" */
        plFormat.setForeground (DarkYellow);
        rule.pattern.setPattern ("^#EXTINF\\s*:\\s*\\K-*\\d+\\b"); // "^#EXTINF\\s*:\\s*-*\\d+\\b"
        rule.format = plFormat;
        highlightingRules.append (rule);

        /*plFormat = neutralFormat;
        rule.pattern.setPattern ("^#EXTINF\\s*:");
        rule.format = plFormat;
        highlightingRules.append (rule);*/

        plFormat.setForeground (DarkGreen);
        plFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("^#EXTINF\\b");
        rule.format = plFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "scss")
    {
        /* scss supports nested css blocks but, instead of making its highlighting complex,
           we format it without considering that and so, without syntax error, but with keywords() */

        QTextCharFormat scssFormat;

        /* definitions (starting with @) */
        scssFormat.setForeground (Brown);
        rule.pattern.setPattern ("@[A-Za-z_-]+\\b");
        rule.format = scssFormat;
        highlightingRules.append (rule);

        /* numbers */
        scssFormat.setFontItalic (true);
        rule.pattern.setPattern ("(-|\\+){0,1}\\b\\d*\\.{0,1}\\d+%*");
        rule.format = scssFormat;
        highlightingRules.append (rule);

        /* colors */
        scssFormat.setForeground (Verda);
        scssFormat.setFontWeight (QFont::Bold);
        rule.pattern.setPattern ("#([A-Fa-f0-9]{3}){1,2}(?![A-Za-z0-9_]+)|#([A-Fa-f0-9]{3}){2}[A-Fa-f0-9]{2}(?![A-Za-z0-9_]+)");
        rule.format = scssFormat;
        highlightingRules.append (rule);


        /* before :...; */
        scssFormat.setForeground (Blue);
        scssFormat.setFontItalic (false);
        /* exclude note patterns artificially */
        rule.pattern.setPattern ("[A-Za-z0-9_\\-]+(?<!\\bNOTE|\\bTODO|\\bFIXME|\\bWARNING)(?=\\s*:.*;*)");
        rule.format = scssFormat;
        highlightingRules.append (rule);
        scssFormat.setFontWeight (QFont::Normal);

        /* variables ($...) */
        scssFormat.setFontItalic (true);
        rule.pattern.setPattern ("\\$[A-Za-z0-9_\\-]+");
        rule.format = scssFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "dart")
    {
        QTextCharFormat dartFormat;

        /* dart:core classes */
        dartFormat.setFontWeight (QFont::Bold);
        dartFormat.setForeground (DarkMagenta);
        rule.pattern.setPattern ("\\b(BidirectionalIterator|BigInt|Comparable|DateTime|Deprecated|Duration|Expando|Function|Future|Invocation|Iterable|Iterator|List|Map|MapEntry|Match|Object|Pattern|RegExp|RegExpMatch|RuneIterator|Runes|Set|Sink|StackTrace|Stopwatch|Stream|String|StringBuffer|StringSink|Symbol|Type|Uri|UriData)\\b");
        rule.format = dartFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "go")
    {
        singleQuoteMark.setPattern ("`");
        mixedQuoteMark.setPattern ("\"|`");

        QTextCharFormat goFormat;
        goFormat.setFontWeight (QFont::Bold);
        goFormat.setForeground (Magenta);
        rule.pattern.setPattern ("\\b(append|cap|close|complex|copy|delete|imag|len|make|new|panic|print|println|real|recover)\\b");
        rule.format = goFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "pascal")
    {
        quoteMark.setPattern ("'");

        QTextCharFormat pascalFormat;

        /* symbols */
        pascalFormat.setForeground (DarkYellow);
        rule.pattern.setPattern ("[=\\+\\-*/<>\\^@#,;:\\.]");
        rule.format = pascalFormat;
        highlightingRules.append (rule);

        /* numbers (including the scientific notation, hexadecimal, octal and binary numbers) */
        pascalFormat.setForeground (Brown);
        rule.pattern.setPattern ("#?(?<=^|[^\\w\\d])((\\d*\\.?\\d+|\\d+\\.)((e|E)(\\+|-)?\\d+)?|\\$[0-9a-fA-F]+|&[0-7]+|%[0-1]+)(?=[^\\d]|$)");
        rule.format = pascalFormat;
        highlightingRules.append (rule);

        /* built-in functions */
        pascalFormat.setFontWeight (QFont::Bold);
        pascalFormat.setForeground (Magenta);
        rule.pattern.setPattern ("(?i)\\b(?<!(@|#|\\$))(abs|addr|arctan|card|chr|concat|copy|copyword|cos|cosh|countwords|createobject|dirsep|eof|eoln|exp|expo|fexpand|filematch|filepos|filesize|firstof|frac|getenv|getlasterror|hex|int|ioresult|isalpha|isalphanum|isdigit|islower|isnull|isprint|isspace|isupper|isxdigit|keypressed|lastof|length|ln|log|locase|lowercase|ltrim|max|min|odd|ord|paramcount|paramstr|pi|platform|pos|pred|ptr|random|readkey|reverse|round|seed|sin|sinh|sizeof|sqr|sqrt|stopserverviceevent|succ|supported|swap|system|tan|tanh|trim|trunc|unixplatform|upcase|uppercase|urldecode|version|wait|wherex|wherey)(?!(@|#|\\$))\\b");
        rule.format = pascalFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "java")
    {
        commentBoldFormat.setForeground (Red);
        commentBoldFormat.setFontItalic (true);
        commentBoldFormat.setFontWeight (QFont::Bold);

        codeBlockFormat.setForeground (Violet);

        /* characters */
        rule.pattern.setPattern ("\'([^\'\\\\]|\\\\[0-9abefnrtv\"\'\\\\]|\\\\u[0-9a-fA-F]{4})\'");
        rule.format = altQuoteFormat;
        highlightingRules.append (rule);

        /* all classes */
        QTextCharFormat javaFormat;
        javaFormat.setForeground (DarkMagenta);
        javaFormat.setFontWeight (QFont::Bold);

        rule.pattern.setPattern ("\\b(AboutEvent|AboutHandler|AbsentInformationException|AbstractAction|AbstractAnnotationValueVisitor6|AbstractAnnotationValueVisitor7|AbstractAnnotationValueVisitor8|AbstractBorder|AbstractButton|AbstractCellEditor|AbstractChronology|AbstractCollection|AbstractColorChooserPanel|AbstractDocument|AbstractDocument.AttributeContext|AbstractDocument.Content|AbstractDocument.ElementEdit|AbstractElementVisitor6|AbstractElementVisitor7|AbstractElementVisitor8|AbstractExecutorService|AbstractInterruptibleChannel|AbstractLayoutCache|AbstractLayoutCache.NodeDimensions|AbstractList|AbstractListModel|AbstractMap|AbstractMap.SimpleEntry|AbstractMap.SimpleImmutableEntry|AbstractMarshallerImpl|AbstractMethodError|AbstractOwnableSynchronizer|AbstractPreferences|AbstractProcessor|AbstractQueue|AbstractQueuedLongSynchronizer|AbstractQueuedSynchronizer|AbstractRegionPainter|AbstractRegionPainter.PaintContext|AbstractRegionPainter.PaintContext.CacheMode|AbstractScriptEngine|AbstractSelectableChannel|AbstractSelectionKey|AbstractSelector|AbstractSequentialList|AbstractSet|AbstractSpinnerModel|AbstractTableModel|AbstractTypeVisitor6|AbstractTypeVisitor7|AbstractTypeVisitor8|AbstractUndoableEdit|AbstractUnmarshallerImpl|AbstractView|AbstractWriter|AcceptPendingException|AccessControlContext|AccessControlException|AccessController|AccessDeniedException|AccessException|Accessible|AccessibleAction|AccessibleAttributeSequence|AccessibleBundle|AccessibleComponent|AccessibleContext|AccessibleEditableText|AccessibleExtendedComponent|AccessibleExtendedTable|AccessibleExtendedText|AccessibleHyperlink|AccessibleHypertext|AccessibleIcon|AccessibleKeyBinding|AccessibleObject|AccessibleRelation|AccessibleRelationSet|AccessibleResourceBundle|AccessibleRole|AccessibleSelection|AccessibleState|AccessibleStateSet|AccessibleStreamable|AccessibleTable|AccessibleTableModelChange|AccessibleText|AccessibleTextSequence|AccessibleValue|AccessMode|AccountException|AccountExpiredException|AccountLockedException|AccountNotFoundException|Acl|AclEntry|AclEntry|AclEntry.Builder|AclEntryFlag|AclEntryPermission|AclEntryType|AclFileAttributeView|AclNotFoundException|Action|Action|ActionEvent|ActionListener|ActionMap|ActionMapUIResource|Activatable|ActivateFailedException|ActivationDataFlavor|ActivationDesc|ActivationException|ActivationGroup|ActivationGroup_Stub|ActivationGroupDesc|ActivationGroupDesc.CommandEnvironment|ActivationGroupID|ActivationID|ActivationInstantiator|ActivationMonitor|ActivationSystem|Activator|ACTIVE|ActiveEvent|ACTIVITY_COMPLETED|ACTIVITY_REQUIRED|ActivityCompletedException|ActivityRequiredException|AdapterActivator|AdapterActivatorOperations|AdapterAlreadyExists|AdapterAlreadyExistsHelper|AdapterInactive|AdapterInactiveHelper|AdapterManagerIdHelper|AdapterNameHelper|AdapterNonExistent|AdapterNonExistentHelper|AdapterStateHelper|AddressHelper|Addressing|AddressingFeature|AddressingFeature.Responses|Adjustable|AdjustmentEvent|AdjustmentListener|Adler32|AEADBadTagException|AffineTransform|AffineTransformOp|AlgorithmConstraints|AlgorithmMethod|AlgorithmParameterGenerator|AlgorithmParameterGeneratorSpi|AlgorithmParameters|AlgorithmParameterSpec|AlgorithmParametersSpi|AllPermission|AlphaComposite|AlreadyBound|AlreadyBoundException|AlreadyBoundException|AlreadyBoundHelper|AlreadyBoundHolder|AlreadyConnectedException|AncestorEvent|AncestorListener|AnnotatedArrayType|AnnotatedConstruct|AnnotatedElement|AnnotatedParameterizedType|AnnotatedType|AnnotatedTypeVariable|AnnotatedWildcardType|Annotation|Annotation|AnnotationFormatError|AnnotationMirror|AnnotationTypeMismatchException|AnnotationValue|AnnotationValueVisitor|Any|AnyHolder|AnySeqHelper|AnySeqHelper|AnySeqHolder|AppConfigurationEntry|AppConfigurationEntry.LoginModuleControlFlag|Appendable|Applet|AppletContext|AppletInitializer|AppletStub|ApplicationException|Arc2D|Arc2D.Double|Arc2D.Float|Area|AreaAveragingScaleFilter|ARG_IN|ARG_INOUT|ARG_OUT|ArithmeticException|Array|Array|ArrayBlockingQueue|ArrayDeque|ArrayIndexOutOfBoundsException|ArrayList|Arrays|ArrayStoreException|ArrayType|ArrayType|AssertionError|AsyncBoxView|AsyncHandler|AsynchronousByteChannel|AsynchronousChannel|AsynchronousChannelGroup|AsynchronousChannelProvider|AsynchronousCloseException|AsynchronousFileChannel|AsynchronousServerSocketChannel|AsynchronousSocketChannel|AtomicBoolean|AtomicInteger|AtomicIntegerArray|AtomicIntegerFieldUpdater|AtomicLong|AtomicLongArray|AtomicLongFieldUpdater|AtomicMarkableReference|AtomicMoveNotSupportedException|AtomicReference|AtomicReferenceArray|AtomicReferenceFieldUpdater|AtomicStampedReference|AttachmentMarshaller|AttachmentPart|AttachmentUnmarshaller|Attr|Attribute|Attribute|Attribute|Attribute|AttributeChangeNotification|AttributeChangeNotificationFilter|AttributedCharacterIterator|AttributedCharacterIterator.Attribute|AttributedString|AttributeException|AttributeInUseException|AttributeList|AttributeList|AttributeList|AttributeListImpl|AttributeModificationException|AttributeNotFoundException|Attributes|Attributes|Attributes|Attributes.Name|Attributes2|Attributes2Impl|AttributeSet|AttributeSet|AttributeSet.CharacterAttribute|AttributeSet.ColorAttribute|AttributeSet.FontAttribute|AttributeSet.ParagraphAttribute|AttributeSetUtilities|AttributesImpl|AttributeValueExp|AttributeView|AudioClip|AudioFileFormat|AudioFileFormat.Type|AudioFileReader|AudioFileWriter|AudioFormat|AudioFormat.Encoding|AudioInputStream|AudioPermission|AudioSystem|AuthenticationException|AuthenticationException|AuthenticationNotSupportedException|Authenticator|Authenticator.RequestorType|AuthorizeCallback|AuthPermission|AuthProvider|AutoCloseable|Autoscroll|AWTError|AWTEvent|AWTEventListener|AWTEventListenerProxy|AWTEventMulticaster|AWTException|AWTKeyStroke|AWTPermission)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(BackingStoreException|BAD_CONTEXT|BAD_INV_ORDER|BAD_OPERATION|BAD_PARAM|BAD_POLICY|BAD_POLICY_TYPE|BAD_POLICY_VALUE|BAD_QOS|BAD_TYPECODE|BadAttributeValueExpException|BadBinaryOpValueExpException|BadKind|BadLocationException|BadPaddingException|BadStringOperationException|BandCombineOp|BandedSampleModel|Base64|Base64.Decoder|Base64.Encoder|BaseRowSet|BaseStream|BasicArrowButton|BasicAttribute|BasicAttributes|BasicBorders|BasicBorders.ButtonBorder|BasicBorders.FieldBorder|BasicBorders.MarginBorder|BasicBorders.MenuBarBorder|BasicBorders.RadioButtonBorder|BasicBorders.RolloverButtonBorder|BasicBorders.SplitPaneBorder|BasicBorders.ToggleButtonBorder|BasicButtonListener|BasicButtonUI|BasicCheckBoxMenuItemUI|BasicCheckBoxUI|BasicColorChooserUI|BasicComboBoxEditor|BasicComboBoxEditor.UIResource|BasicComboBoxRenderer|BasicComboBoxRenderer.UIResource|BasicComboBoxUI|BasicComboPopup|BasicControl|BasicDesktopIconUI|BasicDesktopPaneUI|BasicDirectoryModel|BasicEditorPaneUI|BasicFileAttributes|BasicFileAttributeView|BasicFileChooserUI|BasicFormattedTextFieldUI|BasicGraphicsUtils|BasicHTML|BasicIconFactory|BasicInternalFrameTitlePane|BasicInternalFrameUI|BasicLabelUI|BasicListUI|BasicLookAndFeel|BasicMenuBarUI|BasicMenuItemUI|BasicMenuUI|BasicOptionPaneUI|BasicOptionPaneUI.ButtonAreaLayout|BasicPanelUI|BasicPasswordFieldUI|BasicPermission|BasicPopupMenuSeparatorUI|BasicPopupMenuUI|BasicProgressBarUI|BasicRadioButtonMenuItemUI|BasicRadioButtonUI|BasicRootPaneUI|BasicScrollBarUI|BasicScrollPaneUI|BasicSeparatorUI|BasicSliderUI|BasicSpinnerUI|BasicSplitPaneDivider|BasicSplitPaneUI|BasicStroke|BasicTabbedPaneUI|BasicTableHeaderUI|BasicTableUI|BasicTextAreaUI|BasicTextFieldUI|BasicTextPaneUI|BasicTextUI|BasicTextUI.BasicCaret|BasicTextUI.BasicHighlighter|BasicToggleButtonUI|BasicToolBarSeparatorUI|BasicToolBarUI|BasicToolTipUI|BasicTreeUI|BasicViewportUI|BatchUpdateException|BeanContext|BeanContextChild|BeanContextChildComponentProxy|BeanContextChildSupport|BeanContextContainerProxy|BeanContextEvent|BeanContextMembershipEvent|BeanContextMembershipListener|BeanContextProxy|BeanContextServiceAvailableEvent|BeanContextServiceProvider|BeanContextServiceProviderBeanInfo|BeanContextServiceRevokedEvent|BeanContextServiceRevokedListener|BeanContextServices|BeanContextServicesListener|BeanContextServicesSupport|BeanContextServicesSupport.BCSSServiceProvider|BeanContextSupport|BeanContextSupport.BCSIterator|BeanDescriptor|BeanInfo|Beans|BevelBorder|BiConsumer|Bidi|BiFunction|BigDecimal|BigInteger|BinaryOperator|BinaryRefAddr|Binder|BindException|Binding|Binding|Binding|BindingHelper|BindingHolder|BindingIterator|BindingIteratorHelper|BindingIteratorHolder|BindingIteratorOperations|BindingIteratorPOA|BindingListHelper|BindingListHolder|BindingProvider|Bindings|BindingType|BindingType|BindingTypeHelper|BindingTypeHolder|BiPredicate|BitSet|Blob|BlockingDeque|BlockingQueue|BlockView|BMPImageWriteParam|Book|Boolean|BooleanControl|BooleanControl.Type|BooleanHolder|BooleanSeqHelper|BooleanSeqHolder|BooleanSupplier|BootstrapMethodError|Border|BorderFactory|BorderLayout|BorderUIResource|BorderUIResource.BevelBorderUIResource|BorderUIResource.CompoundBorderUIResource|BorderUIResource.EmptyBorderUIResource|BorderUIResource.EtchedBorderUIResource|BorderUIResource.LineBorderUIResource|BorderUIResource.MatteBorderUIResource|BorderUIResource.TitledBorderUIResource|BoundedRangeModel|Bounds|Bounds|Box|Box.Filler|BoxedValueHelper|BoxLayout|BoxView|BreakIterator|BreakIteratorProvider|BrokenBarrierException|Buffer|BufferCapabilities|BufferCapabilities.FlipContents|BufferedImage|BufferedImageFilter|BufferedImageOp|BufferedInputStream|BufferedOutputStream|BufferedReader|BufferedWriter|BufferOverflowException|BufferPoolMXBean|BufferStrategy|BufferUnderflowException|Button|ButtonGroup|ButtonModel|ButtonUI|Byte|ByteArrayInputStream|ByteArrayOutputStream|ByteBuffer|ByteChannel|ByteHolder|ByteLookupTable|ByteOrder)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(C14NMethodParameterSpec|CachedRowSet|CacheRequest|CacheResponse|Calendar|Calendar.Builder|CalendarDataProvider|CalendarNameProvider|Callable|CallableStatement|Callback|CallbackHandler|CallSite|CancelablePrintJob|CancellationException|CancelledKeyException|CannotProceed|CannotProceedException|CannotProceedHelper|CannotProceedHolder|CannotRedoException|CannotUndoException|CanonicalizationMethod|Canvas|CardLayout|Caret|CaretEvent|CaretListener|CDATASection|CellEditor|CellEditorListener|CellRendererPane|Certificate|Certificate|Certificate|Certificate.CertificateRep|CertificateEncodingException|CertificateEncodingException|CertificateException|CertificateException|CertificateExpiredException|CertificateExpiredException|CertificateFactory|CertificateFactorySpi|CertificateNotYetValidException|CertificateNotYetValidException|CertificateParsingException|CertificateParsingException|CertificateRevokedException|CertPath|CertPath.CertPathRep|CertPathBuilder|CertPathBuilderException|CertPathBuilderResult|CertPathBuilderSpi|CertPathChecker|CertPathParameters|CertPathTrustManagerParameters|CertPathValidator|CertPathValidatorException|CertPathValidatorException.BasicReason|CertPathValidatorException.Reason|CertPathValidatorResult|CertPathValidatorSpi|CertSelector|CertStore|CertStoreException|CertStoreParameters|CertStoreSpi|ChangedCharSetException|ChangeEvent|ChangeListener|Channel|ChannelBinding|Channels|Character|Character.Subset|Character.UnicodeBlock|Character.UnicodeScript|CharacterCodingException|CharacterData|CharacterIterator|Characters|CharArrayReader|CharArrayWriter|CharBuffer|CharConversionException|CharHolder|CharSeqHelper|CharSeqHolder|CharSequence|Charset|CharsetDecoder|CharsetEncoder|CharsetProvider|Checkbox|CheckboxGroup|CheckboxMenuItem|CheckedInputStream|CheckedOutputStream|Checksum|Choice|ChoiceCallback|ChoiceFormat|Chromaticity|ChronoField|ChronoLocalDate|ChronoLocalDateTime|Chronology|ChronoPeriod|ChronoUnit|ChronoZonedDateTime|Cipher|CipherInputStream|CipherOutputStream|CipherSpi|Class|ClassCastException|ClassCircularityError|ClassDefinition|ClassDesc|ClassFileTransformer|ClassFormatError|ClassLoader|ClassLoaderRepository|ClassLoadingMXBean|ClassNotFoundException|ClassValue|ClientInfoStatus|ClientRequestInfo|ClientRequestInfoOperations|ClientRequestInterceptor|ClientRequestInterceptorOperations|Clip|Clipboard|ClipboardOwner|Clob|Clock|Cloneable|CloneNotSupportedException|Closeable|ClosedByInterruptException|ClosedChannelException|ClosedDirectoryStreamException|ClosedFileSystemException|ClosedSelectorException|ClosedWatchServiceException|CMMException|Codec|CodecFactory|CodecFactoryHelper|CodecFactoryOperations|CodecOperations|CoderMalfunctionError|CoderResult|CODESET_INCOMPATIBLE|CodeSets|CodeSigner|CodeSource|CodingErrorAction|CollapsedStringAdapter|CollationElementIterator|CollationKey|Collator|CollatorProvider|Collection|CollectionCertStoreParameters|Collections|Collector|Collector.Characteristics|Collectors|Color|ColorChooserComponentFactory|ColorChooserUI|ColorConvertOp|ColorModel|ColorSelectionModel|ColorSpace|ColorSupported|ColorType|ColorUIResource|ComboBoxEditor|ComboBoxModel|ComboBoxUI|ComboPopup|COMM_FAILURE|CommandInfo|CommandMap|CommandObject|Comment|Comment|CommonDataSource|CommunicationException|Comparable|Comparator|Compilable|CompilationMXBean|CompiledScript|Compiler|CompletableFuture|CompletableFuture.AsynchronousCompletionTask|Completion|CompletionException|CompletionHandler|Completions|CompletionService|CompletionStage|CompletionStatus|CompletionStatusHelper|Component|Component.BaselineResizeBehavior|ComponentAdapter|ComponentColorModel|ComponentEvent|ComponentIdHelper|ComponentInputMap|ComponentInputMapUIResource|ComponentListener|ComponentOrientation|ComponentSampleModel|ComponentUI|ComponentView|Composite|CompositeContext|CompositeData|CompositeDataInvocationHandler|CompositeDataSupport|CompositeDataView|CompositeName|CompositeType|CompositeView|CompoundBorder|CompoundControl|CompoundControl.Type|CompoundEdit|CompoundName|Compression|ConcurrentHashMap|ConcurrentHashMap.KeySetView|ConcurrentLinkedDeque|ConcurrentLinkedQueue|ConcurrentMap|ConcurrentModificationException|ConcurrentNavigableMap|ConcurrentSkipListMap|ConcurrentSkipListSet|Condition|Configuration|Configuration.Parameters|ConfigurationException|ConfigurationSpi|ConfirmationCallback|ConnectException|ConnectException|ConnectIOException|Connection|ConnectionEvent|ConnectionEventListener|ConnectionPendingException|ConnectionPoolDataSource|Console|ConsoleHandler|ConstantCallSite|Constructor|ConstructorProperties|Consumer|Container|ContainerAdapter|ContainerEvent|ContainerListener|ContainerOrderFocusTraversalPolicy|ContentHandler|ContentHandler|ContentHandlerFactory|ContentModel|Context|Context|ContextList|ContextNotEmptyException|ContextualRenderedImageFactory|Control|Control|Control.Type|ControlFactory|ControllerEventListener|ConvolveOp|CookieHandler|CookieHolder|CookieManager|CookiePolicy|CookieStore|Copies|CopiesSupported|CopyOnWriteArrayList|CopyOnWriteArraySet|CopyOption|CountDownLatch|CountedCompleter|CounterMonitor|CounterMonitorMBean|CRC32|CredentialException|CredentialExpiredException|CredentialNotFoundException|CRL|CRLException|CRLReason|CRLSelector|CropImageFilter|CryptoPrimitive|CSS|CSS.Attribute|CTX_RESTRICT_SCOPE|CubicCurve2D|CubicCurve2D.Double|CubicCurve2D.Float|Currency|CurrencyNameProvider|Current|Current|Current|CurrentHelper|CurrentHelper|CurrentHelper|CurrentHolder|CurrentOperations|CurrentOperations|CurrentOperations|Cursor|Customizer|CustomMarshal|CustomValue|CyclicBarrier)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(Data|DATA_CONVERSION|DatabaseMetaData|DataBindingException|DataBuffer|DataBufferByte|DataBufferDouble|DataBufferFloat|DataBufferInt|DataBufferShort|DataBufferUShort|DataContentHandler|DataContentHandlerFactory|DataFlavor|DataFormatException|DatagramChannel|DatagramPacket|DatagramSocket|DatagramSocketImpl|DatagramSocketImplFactory|DataHandler|DataInput|DataInputStream|DataInputStream|DataLine|DataLine.Info|DataOutput|DataOutputStream|DataOutputStream|DataSource|DataSource|DataTruncation|DatatypeConfigurationException|DatatypeConstants|DatatypeConstants.Field|DatatypeConverter|DatatypeConverterInterface|DatatypeFactory|Date|Date|DateFormat|DateFormat.Field|DateFormatProvider|DateFormatSymbols|DateFormatSymbolsProvider|DateFormatter|DateTimeAtCompleted|DateTimeAtCreation|DateTimeAtProcessing|DateTimeException|DateTimeFormatter|DateTimeFormatterBuilder|DateTimeParseException|DateTimeSyntax|DayOfWeek|DebugGraphics|DecimalFormat|DecimalFormatSymbols|DecimalFormatSymbolsProvider|DecimalStyle|DeclaredType|DeclHandler|DefaultBoundedRangeModel|DefaultButtonModel|DefaultCaret|DefaultCellEditor|DefaultColorSelectionModel|DefaultComboBoxModel|DefaultDesktopManager|DefaultEditorKit|DefaultEditorKit.BeepAction|DefaultEditorKit.CopyAction|DefaultEditorKit.CutAction|DefaultEditorKit.DefaultKeyTypedAction|DefaultEditorKit.InsertBreakAction|DefaultEditorKit.InsertContentAction|DefaultEditorKit.InsertTabAction|DefaultEditorKit.PasteAction|DefaultFocusManager|DefaultFocusTraversalPolicy|DefaultFormatter|DefaultFormatterFactory|DefaultHandler|DefaultHandler2|DefaultHighlighter|DefaultHighlighter.DefaultHighlightPainter|DefaultKeyboardFocusManager|DefaultListCellRenderer|DefaultListCellRenderer.UIResource|DefaultListModel|DefaultListSelectionModel|DefaultLoaderRepository|DefaultLoaderRepository|DefaultMenuLayout|DefaultMetalTheme|DefaultMutableTreeNode|DefaultPersistenceDelegate|DefaultRowSorter|DefaultRowSorter.ModelWrapper|DefaultSingleSelectionModel|DefaultStyledDocument|DefaultStyledDocument.AttributeUndoableEdit|DefaultStyledDocument.ElementSpec|DefaultTableCellRenderer|DefaultTableCellRenderer.UIResource|DefaultTableColumnModel|DefaultTableModel|DefaultTextUI|DefaultTreeCellEditor|DefaultTreeCellRenderer|DefaultTreeModel|DefaultTreeSelectionModel|DefaultValidationEventHandler|DefinitionKind|DefinitionKindHelper|Deflater|DeflaterInputStream|DeflaterOutputStream|Delayed|DelayQueue|Delegate|Delegate|Delegate|DelegationPermission|Deprecated|Deque|Descriptor|DescriptorAccess|DescriptorKey|DescriptorRead|DescriptorSupport|DESedeKeySpec|DesignMode|DESKeySpec|Desktop|Desktop.Action|DesktopIconUI|DesktopManager|DesktopPaneUI|Destination|Destroyable|DestroyFailedException|Detail|DetailEntry|DGC|DHGenParameterSpec|DHKey|DHParameterSpec|DHPrivateKey|DHPrivateKeySpec|DHPublicKey|DHPublicKeySpec|Diagnostic|Diagnostic.Kind|DiagnosticCollector|DiagnosticListener|Dialog|Dialog.ModalExclusionType|Dialog.ModalityType|DialogTypeSelection|Dictionary|DigestException|DigestInputStream|DigestMethod|DigestMethodParameterSpec|DigestOutputStream|Dimension|Dimension2D|DimensionUIResource|DirContext|DirectColorModel|DirectoryIteratorException|DirectoryManager|DirectoryNotEmptyException|DirectoryStream|DirectoryStream.Filter|DirObjectFactory|DirStateFactory|DirStateFactory.Result|DISCARDING|Dispatch|DisplayMode|DnDConstants|Doc|DocAttribute|DocAttributeSet|DocFlavor|DocFlavor.BYTE_ARRAY|DocFlavor.CHAR_ARRAY|DocFlavor.INPUT_STREAM|DocFlavor.READER|DocFlavor.SERVICE_FORMATTED|DocFlavor.STRING|DocFlavor.URL|DocPrintJob|Document|Document|DocumentationTool|DocumentationTool.DocumentationTask|DocumentationTool.Location|DocumentBuilder|DocumentBuilderFactory|Documented|DocumentEvent|DocumentEvent|DocumentEvent.ElementChange|DocumentEvent.EventType|DocumentFilter|DocumentFilter.FilterBypass|DocumentFragment|DocumentHandler|DocumentListener|DocumentName|DocumentParser|DocumentType|DocumentView|DomainCombiner|DomainLoadStoreParameter|DomainManager|DomainManagerOperations|DOMConfiguration|DOMCryptoContext|DOMError|DOMErrorHandler|DOMException|DomHandler|DOMImplementation|DOMImplementationList|DOMImplementationLS|DOMImplementationRegistry|DOMImplementationSource|DOMLocator|DOMLocator|DOMResult|DOMSignContext|DOMSource|DOMStringList|DOMStructure|DOMURIReference|DOMValidateContext|DosFileAttributes|DosFileAttributeView|Double|DoubleAccumulator|DoubleAdder|DoubleBinaryOperator|DoubleBuffer|DoubleConsumer|DoubleFunction|DoubleHolder|DoublePredicate|DoubleSeqHelper|DoubleSeqHolder|DoubleStream|DoubleStream.Builder|DoubleSummaryStatistics|DoubleSupplier|DoubleToIntFunction|DoubleToLongFunction|DoubleUnaryOperator|DragGestureEvent|DragGestureListener|DragGestureRecognizer|DragSource|DragSourceAdapter|DragSourceContext|DragSourceDragEvent|DragSourceDropEvent|DragSourceEvent|DragSourceListener|DragSourceMotionListener|Driver|DriverAction|DriverManager|DriverPropertyInfo|DropMode|DropTarget|DropTarget.DropTargetAutoScroller|DropTargetAdapter|DropTargetContext|DropTargetDragEvent|DropTargetDropEvent|DropTargetEvent|DropTargetListener|DSAGenParameterSpec|DSAKey|DSAKeyPairGenerator|DSAParameterSpec|DSAParams|DSAPrivateKey|DSAPrivateKeySpec|DSAPublicKey|DSAPublicKeySpec|DTD|DTD|DTDConstants|DTDHandler|DuplicateFormatFlagsException|DuplicateName|DuplicateNameHelper|Duration|Duration|DynamicImplementation|DynamicImplementation|DynamicMBean|DynAny|DynAny|DynAnyFactory|DynAnyFactoryHelper|DynAnyFactoryOperations|DynAnyHelper|DynAnyOperations|DynAnySeqHelper|DynArray|DynArray|DynArrayHelper|DynArrayOperations|DynEnum|DynEnum|DynEnumHelper|DynEnumOperations|DynFixed|DynFixed|DynFixedHelper|DynFixedOperations|DynSequence|DynSequence|DynSequenceHelper|DynSequenceOperations|DynStruct|DynStruct|DynStructHelper|DynStructOperations|DynUnion|DynUnion|DynUnionHelper|DynUnionOperations|DynValue|DynValue|DynValueBox|DynValueBoxOperations|DynValueCommon|DynValueCommonOperations|DynValueHelper|DynValueOperations)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(ECField|ECFieldF2m|ECFieldFp|ECGenParameterSpec|ECKey|ECParameterSpec|ECPoint|ECPrivateKey|ECPrivateKeySpec|ECPublicKey|ECPublicKeySpec|EditorKit|Element|Element|Element|Element|Element|ElementFilter|ElementIterator|ElementKind|ElementKindVisitor6|ElementKindVisitor7|ElementKindVisitor8|Elements|ElementScanner6|ElementScanner7|ElementScanner8|ElementType|ElementVisitor|Ellipse2D|Ellipse2D.Double|Ellipse2D.Float|EllipticCurve|EmptyBorder|EmptyStackException|EncodedKeySpec|Encoder|Encoding|ENCODING_CDR_ENCAPS|EncryptedPrivateKeyInfo|EndDocument|EndElement|Endpoint|EndpointContext|EndpointReference|Entity|Entity|EntityDeclaration|EntityReference|EntityReference|EntityResolver|EntityResolver2|Enum|EnumConstantNotPresentException|EnumControl|EnumControl.Type|Enumeration|EnumMap|EnumSet|EnumSyntax|Environment|EOFException|Era|Error|ErrorHandler|ErrorListener|ErrorManager|ErrorType|EtchedBorder|Event|Event|EventContext|EventDirContext|EventException|EventFilter|EventHandler|EventListener|EventListener|EventListenerList|EventListenerProxy|EventObject|EventQueue|EventReaderDelegate|EventSetDescriptor|EventTarget|ExcC14NParameterSpec|Exception|ExceptionDetailMessage|ExceptionInInitializerError|ExceptionList|ExceptionListener|Exchanger|Executable|ExecutableElement|ExecutableType|ExecutionException|Executor|ExecutorCompletionService|Executors|ExecutorService|ExemptionMechanism|ExemptionMechanismException|ExemptionMechanismSpi|ExpandVetoException|ExportException|Expression|ExtendedRequest|ExtendedResponse|ExtendedSSLSession|Extension|Externalizable|FactoryConfigurationError|FactoryConfigurationError|FailedLoginException|FaultAction|FeatureDescriptor|Fidelity|Field|FieldNameHelper|FieldNameHelper|FieldPosition|FieldView|File|FileAlreadyExistsException|FileAttribute|FileAttributeView|FileCacheImageInputStream|FileCacheImageOutputStream|FileChannel|FileChannel.MapMode|FileChooserUI|FileDataSource|FileDescriptor|FileDialog|FileFilter|FileFilter|FileHandler|FileImageInputStream|FileImageOutputStream|FileInputStream|FileLock|FileLockInterruptionException|FileNameExtensionFilter|FilenameFilter|FileNameMap|FileNotFoundException|FileObject|FileOutputStream|FileOwnerAttributeView|FilePermission|Filer|FileReader|FilerException|Files|FileStore|FileStoreAttributeView|FileSystem|FileSystemAlreadyExistsException|FileSystemException|FileSystemLoopException|FileSystemNotFoundException|FileSystemProvider|FileSystems|FileSystemView|FileTime|FileTypeDetector|FileTypeMap|FileView|FileVisitOption|FileVisitor|FileVisitResult|FileWriter|Filter|FilteredImageSource|FilteredRowSet|FilterInputStream|FilterOutputStream|FilterReader|FilterWriter|Finishings|FixedHeightLayoutCache|FixedHolder|FlatteningPathIterator|FlavorEvent|FlavorException|FlavorListener|FlavorMap|FlavorTable|Float|FloatBuffer|FloatControl|FloatControl.Type|FloatHolder|FloatSeqHelper|FloatSeqHolder|FlowLayout|FlowView|FlowView.FlowStrategy|Flushable|FocusAdapter|FocusEvent|FocusListener|FocusManager|FocusTraversalPolicy|Font|FontFormatException|FontMetrics|FontRenderContext|FontUIResource|ForkJoinPool|ForkJoinPool.ForkJoinWorkerThreadFactory|ForkJoinPool.ManagedBlocker|ForkJoinTask|ForkJoinWorkerThread|Format|Format.Field|FormatConversionProvider|FormatFlagsConversionMismatchException|FormatMismatch|FormatMismatchHelper|FormatStyle|Formattable|FormattableFlags|Formatter|Formatter|Formatter.BigDecimalLayoutForm|FormatterClosedException|FormSubmitEvent|FormSubmitEvent.MethodType|FormView|ForwardingFileObject|ForwardingJavaFileManager|ForwardingJavaFileObject|ForwardRequest|ForwardRequest|ForwardRequestHelper|ForwardRequestHelper|Frame|FREE_MEM|Function|FunctionalInterface|Future|FutureTask|GapContent|GarbageCollectorMXBean|GatheringByteChannel|GaugeMonitor|GaugeMonitorMBean|GCMParameterSpec|GeneralPath|GeneralSecurityException|Generated|GenericArrayType|GenericDeclaration|GenericSignatureFormatError|GlyphJustificationInfo|GlyphMetrics|GlyphVector|GlyphView|GlyphView.GlyphPainter|GradientPaint|GraphicAttribute|Graphics|Graphics2D|GraphicsConfigTemplate|GraphicsConfiguration|GraphicsDevice|GraphicsDevice.WindowTranslucency|GraphicsEnvironment|GrayFilter|GregorianCalendar|GridBagConstraints|GridBagLayout|GridBagLayoutInfo|GridLayout|Group|GroupLayout|GroupLayout.Alignment|GroupPrincipal|GSSContext|GSSCredential|GSSException|GSSManager|GSSName|Guard|GuardedObject|GZIPInputStream|GZIPOutputStream)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(Handler|Handler|HandlerBase|HandlerChain|HandlerResolver|HandshakeCompletedEvent|HandshakeCompletedListener|HasControls|HashAttributeSet|HashDocAttributeSet|HashMap|HashPrintJobAttributeSet|HashPrintRequestAttributeSet|HashPrintServiceAttributeSet|HashSet|Hashtable|HeadlessException|HexBinaryAdapter|HierarchyBoundsAdapter|HierarchyBoundsListener|HierarchyEvent|HierarchyListener|Highlighter|Highlighter.Highlight|Highlighter.HighlightPainter|HijrahChronology|HijrahDate|HijrahEra|HMACParameterSpec|Holder|HOLDING|HostnameVerifier|HTML|HTML.Attribute|HTML.Tag|HTML.UnknownTag|HTMLDocument|HTMLDocument.Iterator|HTMLEditorKit|HTMLEditorKit.HTMLFactory|HTMLEditorKit.HTMLTextAction|HTMLEditorKit.InsertHTMLTextAction|HTMLEditorKit.LinkController|HTMLEditorKit.Parser|HTMLEditorKit.ParserCallback|HTMLFrameHyperlinkEvent|HTMLWriter|HTTPBinding|HttpContext|HttpCookie|HTTPException|HttpExchange|HttpHandler|HttpRetryException|HttpsURLConnection|HttpURLConnection|HyperlinkEvent|HyperlinkEvent.EventType|HyperlinkListener|ICC_ColorSpace|ICC_Profile|ICC_ProfileGray|ICC_ProfileRGB|Icon|IconUIResource|IconView|ID_ASSIGNMENT_POLICY_ID|ID_UNIQUENESS_POLICY_ID|IdAssignmentPolicy|IdAssignmentPolicyOperations|IdAssignmentPolicyValue|IdentifierHelper|Identity|IdentityHashMap|IdentityScope|IDLEntity|IDLType|IDLTypeHelper|IDLTypeOperations|IDN|IdUniquenessPolicy|IdUniquenessPolicyOperations|IdUniquenessPolicyValue|IIOByteBuffer|IIOException|IIOImage|IIOInvalidTreeException|IIOMetadata|IIOMetadataController|IIOMetadataFormat|IIOMetadataFormatImpl|IIOMetadataNode|IIOParam|IIOParamController|IIOReadProgressListener|IIOReadUpdateListener|IIOReadWarningListener|IIORegistry|IIOServiceProvider|IIOWriteProgressListener|IIOWriteWarningListener|IllegalAccessError|IllegalAccessException|IllegalArgumentException|IllegalBlockingModeException|IllegalBlockSizeException|IllegalChannelGroupException|IllegalCharsetNameException|IllegalClassFormatException|IllegalComponentStateException|IllegalFormatCodePointException|IllegalFormatConversionException|IllegalFormatException|IllegalFormatFlagsException|IllegalFormatPrecisionException|IllegalFormatWidthException|IllegalMonitorStateException|IllegalPathStateException|IllegalSelectorException|IllegalStateException|IllegalThreadStateException|IllformedLocaleException|Image|ImageCapabilities|ImageConsumer|ImageFilter|ImageGraphicAttribute|ImageIcon|ImageInputStream|ImageInputStreamImpl|ImageInputStreamSpi|ImageIO|ImageObserver|ImageOutputStream|ImageOutputStreamImpl|ImageOutputStreamSpi|ImageProducer|ImageReader|ImageReaderSpi|ImageReaderWriterSpi|ImageReadParam|ImageTranscoder|ImageTranscoderSpi|ImageTypeSpecifier|ImageView|ImageWriteParam|ImageWriter|ImageWriterSpi|ImagingOpException|ImmutableDescriptor|IMP_LIMIT|IMPLICIT_ACTIVATION_POLICY_ID|ImplicitActivationPolicy|ImplicitActivationPolicyOperations|ImplicitActivationPolicyValue|INACTIVE|IncompatibleClassChangeError|IncompleteAnnotationException|InconsistentTypeCode|InconsistentTypeCode|InconsistentTypeCodeHelper|IndexColorModel|IndexedPropertyChangeEvent|IndexedPropertyDescriptor|IndexOutOfBoundsException|IndirectionException|Inet4Address|Inet6Address|InetAddress|InetSocketAddress|Inflater|InflaterInputStream|InflaterOutputStream|InheritableThreadLocal|Inherited|InitialContext|InitialContextFactory|InitialContextFactoryBuilder|InitialDirContext|INITIALIZE|InitialLdapContext|InitParam|InlineView|InputContext|InputEvent|InputMap|InputMapUIResource|InputMethod|InputMethodContext|InputMethodDescriptor|InputMethodEvent|InputMethodHighlight|InputMethodListener|InputMethodRequests|InputMismatchException|InputSource|InputStream|InputStream|InputStream|InputStreamReader|InputSubset|InputVerifier|Insets|InsetsUIResource|InstanceAlreadyExistsException|InstanceNotFoundException|Instant|InstantiationError|InstantiationException|Instrument|Instrumentation|InsufficientResourcesException|IntBinaryOperator|IntBuffer|IntConsumer|Integer|IntegerSyntax|Interceptor|InterceptorOperations|InterfaceAddress|INTERNAL|InternalError|InternalFrameAdapter|InternalFrameEvent|InternalFrameFocusTraversalPolicy|InternalFrameListener|InternalFrameUI|InternationalFormatter|InterruptedByTimeoutException|InterruptedException|InterruptedIOException|InterruptedNamingException|InterruptibleChannel|IntersectionType|INTF_REPOS|IntFunction|IntHolder|IntPredicate|IntrospectionException|IntrospectionException|Introspector|IntStream|IntStream.Builder|IntSummaryStatistics|IntSupplier|IntToDoubleFunction|IntToLongFunction|IntUnaryOperator|INV_FLAG|INV_IDENT|INV_OBJREF|INV_POLICY|Invalid|INVALID_ACTIVITY|INVALID_TRANSACTION|InvalidActivityException|InvalidAddress|InvalidAddressHelper|InvalidAddressHolder|InvalidAlgorithmParameterException|InvalidApplicationException|InvalidAttributeIdentifierException|InvalidAttributesException|InvalidAttributeValueException|InvalidAttributeValueException|InvalidClassException|InvalidDnDOperationException|InvalidKeyException|InvalidKeyException|InvalidKeySpecException|InvalidMarkException|InvalidMidiDataException|InvalidName|InvalidName|InvalidName|InvalidNameException|InvalidNameHelper|InvalidNameHelper|InvalidNameHolder|InvalidObjectException|InvalidOpenTypeException|InvalidParameterException|InvalidParameterSpecException|InvalidPathException|InvalidPolicy|InvalidPolicyHelper|InvalidPreferencesFormatException|InvalidPropertiesFormatException|InvalidRelationIdException|InvalidRelationServiceException|InvalidRelationTypeException|InvalidRoleInfoException|InvalidRoleValueException|InvalidSearchControlsException|InvalidSearchFilterException|InvalidSeq|InvalidSlot|InvalidSlotHelper|InvalidTargetObjectTypeException|InvalidTransactionException|InvalidTypeForEncoding|InvalidTypeForEncodingHelper|InvalidValue|InvalidValue|InvalidValueHelper|Invocable|InvocationEvent|InvocationHandler|InvocationTargetException|InvokeHandler|Invoker|IOError|IOException|IOR|IORHelper|IORHolder|IORInfo|IORInfoOperations|IORInterceptor|IORInterceptor_3_0|IORInterceptor_3_0Helper|IORInterceptor_3_0Holder|IORInterceptor_3_0Operations|IORInterceptorOperations|IRObject|IRObjectOperations|IsoChronology|IsoEra|IsoFields|IstringHelper|ItemEvent|ItemListener|ItemSelectable|Iterable|Iterator|IvParameterSpec|JapaneseChronology|JapaneseDate|JapaneseEra|JApplet|JarEntry|JarException|JarFile|JarInputStream|JarOutputStream|JarURLConnection|JavaCompiler|JavaCompiler.CompilationTask|JavaFileManager|JavaFileManager.Location|JavaFileObject|JavaFileObject.Kind|JAXB|JAXBContext|JAXBElement|JAXBElement.GlobalScope|JAXBException|JAXBIntrospector|JAXBPermission|JAXBResult|JAXBSource|JButton|JCheckBox|JCheckBoxMenuItem|JColorChooser|JComboBox|JComboBox.KeySelectionManager|JComponent|JdbcRowSet|JDBCType|JDesktopPane|JDialog|JEditorPane|JFileChooser|JFormattedTextField|JFormattedTextField.AbstractFormatter|JFormattedTextField.AbstractFormatterFactory|JFrame|JInternalFrame|JInternalFrame.JDesktopIcon|JLabel|JLayer|JLayeredPane|JList|JList.DropLocation|JMenu|JMenuBar|JMenuItem|JMException|JMRuntimeException|JMX|JMXAddressable|JMXAuthenticator|JMXConnectionNotification|JMXConnector|JMXConnectorFactory|JMXConnectorProvider|JMXConnectorServer|JMXConnectorServerFactory|JMXConnectorServerMBean|JMXConnectorServerProvider|JMXPrincipal|JMXProviderException|JMXServerErrorException|JMXServiceURL|JobAttributes|JobAttributes.DefaultSelectionType|JobAttributes.DestinationType|JobAttributes.DialogType|JobAttributes.MultipleDocumentHandlingType|JobAttributes.SidesType|JobHoldUntil|JobImpressions|JobImpressionsCompleted|JobImpressionsSupported|JobKOctets|JobKOctetsProcessed|JobKOctetsSupported|JobMediaSheets|JobMediaSheetsCompleted|JobMediaSheetsSupported|JobMessageFromOperator|JobName|JobOriginatingUserName|JobPriority|JobPrioritySupported|JobSheets|JobState|JobStateReason|JobStateReasons|Joinable|JoinRowSet|JOptionPane|JPanel|JPasswordField|JPEGHuffmanTable|JPEGImageReadParam|JPEGImageWriteParam|JPEGQTable|JPopupMenu|JPopupMenu.Separator|JProgressBar|JRadioButton|JRadioButtonMenuItem|JRootPane|JScrollBar|JScrollPane|JSeparator|JSlider|JSpinner|JSpinner.DateEditor|JSpinner.DefaultEditor|JSpinner.ListEditor|JSpinner.NumberEditor|JSplitPane|JTabbedPane|JTable|JTable.DropLocation|JTable.PrintMode|JTableHeader|JTextArea|JTextComponent|JTextComponent.DropLocation|JTextComponent.KeyBinding|JTextField|JTextPane|JToggleButton|JToggleButton.ToggleButtonModel|JToolBar|JToolBar.Separator|JToolTip|JTree|JTree.DropLocation|JTree.DynamicUtilTreeNode|JTree.EmptySelectionModel|JulianFields|JViewport|JWindow)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(KerberosKey|KerberosPrincipal|KerberosTicket|Kernel|Key|KeyAdapter|KeyAgreement|KeyAgreementSpi|KeyAlreadyExistsException|KeyboardFocusManager|KeyEvent|KeyEventDispatcher|KeyEventPostProcessor|KeyException|KeyFactory|KeyFactorySpi|KeyGenerator|KeyGeneratorSpi|KeyInfo|KeyInfoFactory|KeyListener|KeyManagementException|KeyManager|KeyManagerFactory|KeyManagerFactorySpi|Keymap|KeyName|KeyPair|KeyPairGenerator|KeyPairGeneratorSpi|KeyRep|KeyRep.Type|KeySelector|KeySelector.Purpose|KeySelectorException|KeySelectorResult|KeySpec|KeyStore|KeyStore.Builder|KeyStore.CallbackHandlerProtection|KeyStore.Entry|KeyStore.Entry.Attribute|KeyStore.LoadStoreParameter|KeyStore.PasswordProtection|KeyStore.PrivateKeyEntry|KeyStore.ProtectionParameter|KeyStore.SecretKeyEntry|KeyStore.TrustedCertificateEntry|KeyStoreBuilderParameters|KeyStoreException|KeyStoreSpi|KeyStroke|KeyTab|KeyValue|Label|LabelUI|LabelView|LambdaConversionException|LambdaMetafactory|LanguageCallback|LastOwnerException|LayeredHighlighter|LayeredHighlighter.LayerPainter|LayerUI|LayoutFocusTraversalPolicy|LayoutManager|LayoutManager2|LayoutPath|LayoutQueue|LayoutStyle|LayoutStyle.ComponentPlacement|LDAPCertStoreParameters|LdapContext|LdapName|LdapReferralException|Lease|Level|LexicalHandler|LIFESPAN_POLICY_ID|LifespanPolicy|LifespanPolicyOperations|LifespanPolicyValue|LimitExceededException|Line|Line.Info|Line2D|Line2D.Double|Line2D.Float|LinearGradientPaint|LineBorder|LineBreakMeasurer|LineEvent|LineEvent.Type|LineListener|LineMetrics|LineNumberInputStream|LineNumberReader|LineUnavailableException|LinkageError|LinkedBlockingDeque|LinkedBlockingQueue|LinkedHashMap|LinkedHashSet|LinkedList|LinkedTransferQueue|LinkException|LinkLoopException|LinkOption|LinkPermission|LinkRef|List|List|ListCellRenderer|ListDataEvent|ListDataListener|ListenerNotFoundException|ListIterator|ListModel|ListResourceBundle|ListSelectionEvent|ListSelectionListener|ListSelectionModel|ListUI|ListView|LoaderHandler|LocalDate|LocalDateTime|Locale|Locale.Builder|Locale.Category|Locale.FilteringMode|Locale.LanguageRange|LocaleNameProvider|LocaleServiceProvider|LocalObject|LocalTime|LocateRegistry|Location|LOCATION_FORWARD|Locator|Locator2|Locator2Impl|LocatorImpl|Lock|LockInfo|LockSupport|Logger|LoggingMXBean|LoggingPermission|LogicalHandler|LogicalMessage|LogicalMessageContext|LoginContext|LoginException|LoginModule|LogManager|LogRecord|LogStream|Long|LongAccumulator|LongAdder|LongBinaryOperator|LongBuffer|LongConsumer|LongFunction|LongHolder|LongLongSeqHelper|LongLongSeqHolder|LongPredicate|LongSeqHelper|LongSeqHolder|LongStream|LongStream.Builder|LongSummaryStatistics|LongSupplier|LongToDoubleFunction|LongToIntFunction|LongUnaryOperator|LookAndFeel|LookupOp|LookupTable|LSException|LSInput|LSLoadEvent|LSOutput|LSParser|LSParserFilter|LSProgressEvent|LSResourceResolver|LSSerializer|LSSerializerFilter)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(Mac|MacSpi|MailcapCommandMap|MalformedInputException|MalformedLinkException|MalformedObjectNameException|MalformedParameterizedTypeException|MalformedParametersException|MalformedURLException|ManagementFactory|ManagementPermission|ManageReferralControl|ManagerFactoryParameters|Manifest|Manifest|Map|Map.Entry|MappedByteBuffer|MARSHAL|MarshalException|MarshalException|MarshalException|MarshalledObject|Marshaller|Marshaller.Listener|MaskFormatter|Matcher|MatchResult|Math|MathContext|MatteBorder|MBeanAttributeInfo|MBeanConstructorInfo|MBeanException|MBeanFeatureInfo|MBeanInfo|MBeanNotificationInfo|MBeanOperationInfo|MBeanParameterInfo|MBeanPermission|MBeanRegistration|MBeanRegistrationException|MBeanServer|MBeanServerBuilder|MBeanServerConnection|MBeanServerDelegate|MBeanServerDelegateMBean|MBeanServerFactory|MBeanServerForwarder|MBeanServerInvocationHandler|MBeanServerNotification|MBeanServerNotificationFilter|MBeanServerPermission|MBeanTrustPermission|Media|MediaName|MediaPrintableArea|MediaSize|MediaSize.Engineering|MediaSize.ISO|MediaSize.JIS|MediaSize.NA|MediaSize.Other|MediaSizeName|MediaTracker|MediaTray|Member|MembershipKey|MemoryCacheImageInputStream|MemoryCacheImageOutputStream|MemoryHandler|MemoryImageSource|MemoryManagerMXBean|MemoryMXBean|MemoryNotificationInfo|MemoryPoolMXBean|MemoryType|MemoryUsage|Menu|MenuBar|MenuBarUI|MenuComponent|MenuContainer|MenuDragMouseEvent|MenuDragMouseListener|MenuElement|MenuEvent|MenuItem|MenuItemUI|MenuKeyEvent|MenuKeyListener|MenuListener|MenuSelectionManager|MenuShortcut|MessageContext|MessageContext.Scope|MessageDigest|MessageDigestSpi|MessageFactory|MessageFormat|MessageFormat.Field|MessageProp|Messager|MetaEventListener|MetalBorders|MetalBorders.ButtonBorder|MetalBorders.Flush3DBorder|MetalBorders.InternalFrameBorder|MetalBorders.MenuBarBorder|MetalBorders.MenuItemBorder|MetalBorders.OptionDialogBorder|MetalBorders.PaletteBorder|MetalBorders.PopupMenuBorder|MetalBorders.RolloverButtonBorder|MetalBorders.ScrollPaneBorder|MetalBorders.TableHeaderBorder|MetalBorders.TextFieldBorder|MetalBorders.ToggleButtonBorder|MetalBorders.ToolBarBorder|MetalButtonUI|MetalCheckBoxIcon|MetalCheckBoxUI|MetalComboBoxButton|MetalComboBoxEditor|MetalComboBoxEditor.UIResource|MetalComboBoxIcon|MetalComboBoxUI|MetalDesktopIconUI|MetalFileChooserUI|MetalIconFactory|MetalIconFactory.FileIcon16|MetalIconFactory.FolderIcon16|MetalIconFactory.PaletteCloseIcon|MetalIconFactory.TreeControlIcon|MetalIconFactory.TreeFolderIcon|MetalIconFactory.TreeLeafIcon|MetalInternalFrameTitlePane|MetalInternalFrameUI|MetalLabelUI|MetalLookAndFeel|MetalMenuBarUI|MetalPopupMenuSeparatorUI|MetalProgressBarUI|MetalRadioButtonUI|MetalRootPaneUI|MetalScrollBarUI|MetalScrollButton|MetalScrollPaneUI|MetalSeparatorUI|MetalSliderUI|MetalSplitPaneUI|MetalTabbedPaneUI|MetalTextFieldUI|MetalTheme|MetalToggleButtonUI|MetalToolBarUI|MetalToolTipUI|MetalTreeUI|MetaMessage|Method|MethodDescriptor|MethodHandle|MethodHandleInfo|MethodHandleProxies|MethodHandles|MethodHandles.Lookup|MethodType|MGF1ParameterSpec|MidiChannel|MidiDevice|MidiDevice.Info|MidiDeviceProvider|MidiDeviceReceiver|MidiDeviceTransmitter|MidiEvent|MidiFileFormat|MidiFileReader|MidiFileWriter|MidiMessage|MidiSystem|MidiUnavailableException|MimeHeader|MimeHeaders|MimeType|MimeTypeParameterList|MimeTypeParseException|MimeTypeParseException|MimetypesFileTypeMap|MinguoChronology|MinguoDate|MinguoEra|MinimalHTMLWriter|MirroredTypeException|MirroredTypesException|MissingFormatArgumentException|MissingFormatWidthException|MissingResourceException|Mixer|Mixer.Info|MixerProvider|MLet|MLetContent|MLetMBean|ModelMBean|ModelMBeanAttributeInfo|ModelMBeanConstructorInfo|ModelMBeanInfo|ModelMBeanInfoSupport|ModelMBeanNotificationBroadcaster|ModelMBeanNotificationInfo|ModelMBeanOperationInfo|ModificationItem|Modifier|Modifier|Monitor|MonitorInfo|MonitorMBean|MonitorNotification|MonitorSettingException|Month|MonthDay|MouseAdapter|MouseDragGestureRecognizer|MouseEvent|MouseEvent|MouseInfo|MouseInputAdapter|MouseInputListener|MouseListener|MouseMotionAdapter|MouseMotionListener|MouseWheelEvent|MouseWheelListener|MTOM|MTOMFeature|MultiButtonUI|MulticastChannel|MulticastSocket|MultiColorChooserUI|MultiComboBoxUI|MultiDesktopIconUI|MultiDesktopPaneUI|MultiDoc|MultiDocPrintJob|MultiDocPrintService|MultiFileChooserUI|MultiInternalFrameUI|MultiLabelUI|MultiListUI|MultiLookAndFeel|MultiMenuBarUI|MultiMenuItemUI|MultiOptionPaneUI|MultiPanelUI|MultiPixelPackedSampleModel|MultipleComponentProfileHelper|MultipleComponentProfileHolder|MultipleDocumentHandling|MultipleGradientPaint|MultipleGradientPaint.ColorSpaceType|MultipleGradientPaint.CycleMethod|MultipleMaster|MultiPopupMenuUI|MultiProgressBarUI|MultiRootPaneUI|MultiScrollBarUI|MultiScrollPaneUI|MultiSeparatorUI|MultiSliderUI|MultiSpinnerUI|MultiSplitPaneUI|MultiTabbedPaneUI|MultiTableHeaderUI|MultiTableUI|MultiTextUI|MultiToolBarUI|MultiToolTipUI|MultiTreeUI|MultiViewportUI|MutableAttributeSet|MutableCallSite|MutableComboBoxModel|MutableTreeNode|MutationEvent|MXBean|Name|Name|Name|NameAlreadyBoundException|NameCallback|NameClassPair|NameComponent|NameComponentHelper|NameComponentHolder|NamedNodeMap|NamedValue|NameDynAnyPair|NameDynAnyPairHelper|NameDynAnyPairSeqHelper|NameHelper|NameHolder|NameList|NameNotFoundException|NameParser|Namespace|NamespaceChangeListener|NamespaceContext|NamespaceSupport|NameValuePair|NameValuePair|NameValuePairHelper|NameValuePairHelper|NameValuePairSeqHelper|Naming|NamingContext|NamingContextExt|NamingContextExtHelper|NamingContextExtHolder|NamingContextExtOperations|NamingContextExtPOA|NamingContextHelper|NamingContextHolder|NamingContextOperations|NamingContextPOA|NamingEnumeration|NamingEvent|NamingException|NamingExceptionEvent|NamingListener|NamingManager|NamingSecurityException|Native|NavigableMap|NavigableSet|NavigationFilter|NavigationFilter.FilterBypass|NClob|NegativeArraySizeException|NestingKind|NetPermission|NetworkChannel|NetworkInterface|NimbusLookAndFeel|NimbusStyle|NO_IMPLEMENT|NO_MEMORY|NO_PERMISSION|NO_RESOURCES|NO_RESPONSE|NoClassDefFoundError|NoConnectionPendingException|NoContext|NoContextHelper|Node|Node|NodeChangeEvent|NodeChangeListener|NodeList|NodeSetData|NoInitialContextException|NON_EXISTENT|NoninvertibleTransformException|NonReadableChannelException|NonWritableChannelException|NoPermissionException|NormalizedStringAdapter|Normalizer|Normalizer.Form|NoRouteToHostException|NoServant|NoServantHelper|NoSuchAlgorithmException|NoSuchAttributeException|NoSuchElementException|NoSuchFieldError|NoSuchFieldException|NoSuchFileException|NoSuchMechanismException|NoSuchMethodError|NoSuchMethodException|NoSuchObjectException|NoSuchPaddingException|NoSuchProviderException|NotActiveException|Notation|NotationDeclaration|NotBoundException|NotCompliantMBeanException|NotContextException|NotDirectoryException|NotEmpty|NotEmptyHelper|NotEmptyHolder|NotFound|NotFoundHelper|NotFoundHolder|NotFoundReason|NotFoundReasonHelper|NotFoundReasonHolder|NotIdentifiableEvent|NotIdentifiableEventImpl|Notification|NotificationBroadcaster|NotificationBroadcasterSupport|NotificationEmitter|NotificationFilter|NotificationFilterSupport|NotificationListener|NotificationResult|NotLinkException|NotOwnerException|NotSerializableException|NotYetBoundException|NotYetConnectedException|NoType|NullCipher|NullPointerException|NullType|Number|NumberFormat|NumberFormat.Field|NumberFormatException|NumberFormatProvider|NumberFormatter|NumberOfDocuments|NumberOfInterveningJobs|NumberUp|NumberUpSupported|NumericShaper|NumericShaper.Range|NVList)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(OAEPParameterSpec|OBJ_ADAPTER|ObjDoubleConsumer|Object|Object|OBJECT_NOT_EXIST|ObjectAlreadyActive|ObjectAlreadyActiveHelper|ObjectChangeListener|ObjectFactory|ObjectFactoryBuilder|ObjectHelper|ObjectHolder|ObjectIdHelper|ObjectIdHelper|ObjectImpl|ObjectImpl|ObjectInput|ObjectInputStream|ObjectInputStream.GetField|ObjectInputValidation|ObjectInstance|ObjectName|ObjectNotActive|ObjectNotActiveHelper|ObjectOutput|ObjectOutputStream|ObjectOutputStream.PutField|ObjectReferenceFactory|ObjectReferenceFactoryHelper|ObjectReferenceFactoryHolder|ObjectReferenceTemplate|ObjectReferenceTemplateHelper|ObjectReferenceTemplateHolder|ObjectReferenceTemplateSeqHelper|ObjectReferenceTemplateSeqHolder|Objects|ObjectStreamClass|ObjectStreamConstants|ObjectStreamException|ObjectStreamField|ObjectView|ObjID|ObjIntConsumer|ObjLongConsumer|Observable|Observer|OceanTheme|OctetSeqHelper|OctetSeqHolder|OctetStreamData|OffsetDateTime|OffsetTime|Oid|OMGVMCID|Oneway|OpenDataException|OpenMBeanAttributeInfo|OpenMBeanAttributeInfoSupport|OpenMBeanConstructorInfo|OpenMBeanConstructorInfoSupport|OpenMBeanInfo|OpenMBeanInfoSupport|OpenMBeanOperationInfo|OpenMBeanOperationInfoSupport|OpenMBeanParameterInfo|OpenMBeanParameterInfoSupport|OpenOption|OpenType|OpenType|OperatingSystemMXBean|Operation|OperationNotSupportedException|OperationsException|Option|Optional|OptionalDataException|OptionalDouble|OptionalInt|OptionalLong|OptionChecker|OptionPaneUI|ORB|ORB|ORBIdHelper|ORBInitializer|ORBInitializerOperations|ORBInitInfo|ORBInitInfoOperations|OrientationRequested|OutOfMemoryError|OutputDeviceAssigned|OutputKeys|OutputStream|OutputStream|OutputStream|OutputStreamWriter|OverlappingFileLockException|OverlayLayout|Override|Owner|Pack200|Pack200.Packer|Pack200.Unpacker|Package|PackageElement|PackedColorModel|Pageable|PageAttributes|PageAttributes.ColorType|PageAttributes.MediaType|PageAttributes.OrientationRequestedType|PageAttributes.OriginType|PageAttributes.PrintQualityType|PagedResultsControl|PagedResultsResponseControl|PageFormat|PageRanges|PagesPerMinute|PagesPerMinuteColor|Paint|PaintContext|Painter|PaintEvent|Panel|PanelUI|Paper|ParagraphView|ParagraphView|Parameter|Parameter|ParameterBlock|ParameterDescriptor|Parameterizable|ParameterizedType|ParameterMetaData|ParameterMode|ParameterModeHelper|ParameterModeHolder|ParseConversionEvent|ParseConversionEventImpl|ParseException|ParsePosition|Parser|Parser|ParserAdapter|ParserConfigurationException|ParserDelegator|ParserFactory|PartialResultException|PasswordAuthentication|PasswordCallback|PasswordView|Patch|Path|Path2D|Path2D.Double|Path2D.Float|PathIterator|PathMatcher|Paths|Pattern|PatternSyntaxException|PBEKey|PBEKeySpec|PBEParameterSpec|PDLOverrideSupported|Period|Permission|Permission|PermissionCollection|Permissions|PERSIST_STORE|PersistenceDelegate|PersistentMBean|PGPData|PhantomReference|Phaser|Pipe|Pipe.SinkChannel|Pipe.SourceChannel|PipedInputStream|PipedOutputStream|PipedReader|PipedWriter|PixelGrabber|PixelInterleavedSampleModel|PKCS12Attribute|PKCS8EncodedKeySpec|PKIXBuilderParameters|PKIXCertPathBuilderResult|PKIXCertPathChecker|PKIXCertPathValidatorResult|PKIXParameters|PKIXReason|PKIXRevocationChecker|PKIXRevocationChecker.Option|PlainDocument|PlainView|PlatformLoggingMXBean|PlatformManagedObject|POA|POAHelper|POAManager|POAManagerOperations|POAOperations|Point|Point2D|Point2D.Double|Point2D.Float|PointerInfo|Policy|Policy|Policy|Policy.Parameters|PolicyError|PolicyErrorCodeHelper|PolicyErrorHelper|PolicyErrorHolder|PolicyFactory|PolicyFactoryOperations|PolicyHelper|PolicyHolder|PolicyListHelper|PolicyListHolder|PolicyNode|PolicyOperations|PolicyQualifierInfo|PolicySpi|PolicyTypeHelper|Polygon|PooledConnection|Popup|PopupFactory|PopupMenu|PopupMenuEvent|PopupMenuListener|PopupMenuUI|Port|Port.Info|PortableRemoteObject|PortableRemoteObjectDelegate|PortInfo|PortUnreachableException|Position|Position.Bias|PosixFileAttributes|PosixFileAttributeView|PosixFilePermission|PosixFilePermissions|PostConstruct|PreDestroy|Predicate|Predicate|PreferenceChangeEvent|PreferenceChangeListener|Preferences|PreferencesFactory|PreparedStatement|PresentationDirection|PrimitiveIterator|PrimitiveIterator.OfDouble|PrimitiveIterator.OfInt|PrimitiveIterator.OfLong|PrimitiveType|Principal|Principal|PrincipalHolder|Printable|PrintConversionEvent|PrintConversionEventImpl|PrinterAbortException|PrinterException|PrinterGraphics|PrinterInfo|PrinterIOException|PrinterIsAcceptingJobs|PrinterJob|PrinterLocation|PrinterMakeAndModel|PrinterMessageFromOperator|PrinterMoreInfo|PrinterMoreInfoManufacturer|PrinterName|PrinterResolution|PrinterState|PrinterStateReason|PrinterStateReasons|PrinterURI|PrintEvent|PrintException|PrintGraphics|PrintJob|PrintJobAdapter|PrintJobAttribute|PrintJobAttributeEvent|PrintJobAttributeListener|PrintJobAttributeSet|PrintJobEvent|PrintJobListener|PrintQuality|PrintRequestAttribute|PrintRequestAttributeSet|PrintService|PrintServiceAttribute|PrintServiceAttributeEvent|PrintServiceAttributeListener|PrintServiceAttributeSet|PrintServiceLookup|PrintStream|PrintWriter|PriorityBlockingQueue|PriorityQueue|PRIVATE_MEMBER|PrivateClassLoader|PrivateCredentialPermission|PrivateKey|PrivateMLet|PrivilegedAction|PrivilegedActionException|PrivilegedExceptionAction|Process|ProcessBuilder|ProcessBuilder.Redirect|ProcessBuilder.Redirect.Type|ProcessingEnvironment|ProcessingInstruction|ProcessingInstruction|Processor|ProfileDataException|ProfileIdHelper|ProgressBarUI|ProgressMonitor|ProgressMonitorInputStream|Properties|PropertyChangeEvent|PropertyChangeListener|PropertyChangeListenerProxy|PropertyChangeSupport|PropertyDescriptor|PropertyEditor|PropertyEditorManager|PropertyEditorSupport|PropertyException|PropertyPermission|PropertyResourceBundle|PropertyVetoException|ProtectionDomain|ProtocolException|ProtocolException|ProtocolFamily|Provider|Provider|Provider|Provider.Service|ProviderException|ProviderMismatchException|ProviderNotFoundException|Proxy|Proxy|Proxy.Type|ProxySelector|PseudoColumnUsage|PSource|PSource.PSpecified|PSSParameterSpec|PUBLIC_MEMBER|PublicKey|PushbackInputStream|PushbackReader)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(QName|QuadCurve2D|QuadCurve2D.Double|QuadCurve2D.Float|QualifiedNameable|Query|QueryEval|QueryExp|Queue|QueuedJobCount|RadialGradientPaint|Random|RandomAccess|RandomAccessFile|Raster|RasterFormatException|RasterOp|RC2ParameterSpec|RC5ParameterSpec|Rdn|Readable|ReadableByteChannel|Reader|ReadOnlyBufferException|ReadOnlyFileSystemException|ReadPendingException|ReadWriteLock|RealmCallback|RealmChoiceCallback|REBIND|Receiver|Rectangle|Rectangle2D|Rectangle2D.Double|Rectangle2D.Float|RectangularShape|RecursiveAction|RecursiveTask|ReentrantLock|ReentrantReadWriteLock|ReentrantReadWriteLock.ReadLock|ReentrantReadWriteLock.WriteLock|Ref|RefAddr|Reference|Reference|Reference|Referenceable|ReferenceQueue|ReferenceType|ReferenceUriSchemesSupported|ReferralException|ReflectionException|ReflectiveOperationException|ReflectPermission|Refreshable|RefreshFailedException|Region|RegisterableService|Registry|RegistryHandler|RejectedExecutionException|RejectedExecutionHandler|Relation|RelationException|RelationNotFoundException|RelationNotification|RelationService|RelationServiceMBean|RelationServiceNotRegisteredException|RelationSupport|RelationSupportMBean|RelationType|RelationTypeNotFoundException|RelationTypeSupport|RemarshalException|Remote|RemoteCall|RemoteException|RemoteObject|RemoteObjectInvocationHandler|RemoteRef|RemoteServer|RemoteStub|RenderableImage|RenderableImageOp|RenderableImageProducer|RenderContext|RenderedImage|RenderedImageFactory|Renderer|RenderingHints|RenderingHints.Key|RepaintManager|Repeatable|ReplicateScaleFilter|RepositoryIdHelper|Request|REQUEST_PROCESSING_POLICY_ID|RequestInfo|RequestInfoOperations|RequestingUserName|RequestProcessingPolicy|RequestProcessingPolicyOperations|RequestProcessingPolicyValue|RequestWrapper|RequiredModelMBean|RescaleOp|ResolutionSyntax|Resolver|ResolveResult|ResolverStyle|Resource|Resource.AuthenticationType|ResourceBundle|ResourceBundle.Control|ResourceBundleControlProvider|Resources|RespectBinding|RespectBindingFeature|Response|ResponseCache|ResponseHandler|ResponseWrapper|Result|ResultSet|ResultSetMetaData|Retention|RetentionPolicy|RetrievalMethod|ReverbType|RGBImageFilter|RMIClassLoader|RMIClassLoaderSpi|RMIClientSocketFactory|RMIConnection|RMIConnectionImpl|RMIConnectionImpl_Stub|RMIConnector|RMIConnectorServer|RMICustomMaxStreamFormat|RMIFailureHandler|RMIIIOPServerImpl|RMIJRMPServerImpl|RMISecurityException|RMISecurityManager|RMIServer|RMIServerImpl|RMIServerImpl_Stub|RMIServerSocketFactory|RMISocketFactory|Robot|Role|RoleInfo|RoleInfoNotFoundException|RoleList|RoleNotFoundException|RoleResult|RoleStatus|RoleUnresolved|RoleUnresolvedList|RootPaneContainer|RootPaneUI|RoundEnvironment|RoundingMode|RoundRectangle2D|RoundRectangle2D.Double|RoundRectangle2D.Float|RowFilter|RowFilter.ComparisonType|RowFilter.Entry|RowId|RowIdLifetime|RowMapper|RowSet|RowSetEvent|RowSetFactory|RowSetInternal|RowSetListener|RowSetMetaData|RowSetMetaDataImpl|RowSetProvider|RowSetReader|RowSetWarning|RowSetWriter|RowSorter|RowSorter.SortKey|RowSorterEvent|RowSorterEvent.Type|RowSorterListener|RSAKey|RSAKeyGenParameterSpec|RSAMultiPrimePrivateCrtKey|RSAMultiPrimePrivateCrtKeySpec|RSAOtherPrimeInfo|RSAPrivateCrtKey|RSAPrivateCrtKeySpec|RSAPrivateKey|RSAPrivateKeySpec|RSAPublicKey|RSAPublicKeySpec|RTFEditorKit|RuleBasedCollator|Runnable|RunnableFuture|RunnableScheduledFuture|Runtime|RunTime|RuntimeErrorException|RuntimeException|RuntimeMBeanException|RuntimeMXBean|RunTimeOperations|RuntimeOperationsException|RuntimePermission)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(SAAJMetaFactory|SAAJResult|SafeVarargs|SampleModel|Sasl|SaslClient|SaslClientFactory|SaslException|SaslServer|SaslServerFactory|Savepoint|SAXException|SAXNotRecognizedException|SAXNotSupportedException|SAXParseException|SAXParser|SAXParserFactory|SAXResult|SAXSource|SAXTransformerFactory|Scanner|ScatteringByteChannel|ScheduledExecutorService|ScheduledFuture|ScheduledThreadPoolExecutor|Schema|SchemaFactory|SchemaFactoryConfigurationError|SchemaFactoryLoader|SchemaOutputResolver|SchemaViolationException|ScriptContext|ScriptEngine|ScriptEngineFactory|ScriptEngineManager|ScriptException|Scrollable|Scrollbar|ScrollBarUI|ScrollPane|ScrollPaneAdjustable|ScrollPaneConstants|ScrollPaneLayout|ScrollPaneLayout.UIResource|ScrollPaneUI|SealedObject|SearchControls|SearchResult|SecondaryLoop|SecretKey|SecretKeyFactory|SecretKeyFactorySpi|SecretKeySpec|SecureCacheResponse|SecureClassLoader|SecureDirectoryStream|SecureRandom|SecureRandomSpi|Security|SecurityException|SecurityManager|SecurityPermission|SeekableByteChannel|Segment|SelectableChannel|SelectionKey|Selector|SelectorProvider|Semaphore|SeparatorUI|Sequence|SequenceInputStream|Sequencer|Sequencer.SyncMode|SerialArray|SerialBlob|SerialClob|SerialDatalink|SerialException|Serializable|SerializablePermission|SerializedLambda|SerialJavaObject|SerialRef|SerialStruct|Servant|SERVANT_RETENTION_POLICY_ID|ServantActivator|ServantActivatorHelper|ServantActivatorOperations|ServantActivatorPOA|ServantAlreadyActive|ServantAlreadyActiveHelper|ServantLocator|ServantLocatorHelper|ServantLocatorOperations|ServantLocatorPOA|ServantManager|ServantManagerOperations|ServantNotActive|ServantNotActiveHelper|ServantObject|ServantRetentionPolicy|ServantRetentionPolicyOperations|ServantRetentionPolicyValue|ServerCloneException|ServerError|ServerException|ServerIdHelper|ServerNotActiveException|ServerRef|ServerRequest|ServerRequestInfo|ServerRequestInfoOperations|ServerRequestInterceptor|ServerRequestInterceptorOperations|ServerRuntimeException|ServerSocket|ServerSocketChannel|ServerSocketFactory|Service|Service.Mode|ServiceConfigurationError|ServiceContext|ServiceContextHelper|ServiceContextHolder|ServiceContextListHelper|ServiceContextListHolder|ServiceDelegate|ServiceDetail|ServiceDetailHelper|ServiceIdHelper|ServiceInformation|ServiceInformationHelper|ServiceInformationHolder|ServiceLoader|ServiceMode|ServiceNotFoundException|ServicePermission|ServiceRegistry|ServiceRegistry.Filter|ServiceUI|ServiceUIFactory|ServiceUnavailableException|Set|SetOfIntegerSyntax|SetOverrideType|SetOverrideTypeHelper|Severity|Shape|ShapeGraphicAttribute|SheetCollate|Short|ShortBuffer|ShortBufferException|ShortHolder|ShortLookupTable|ShortMessage|ShortSeqHelper|ShortSeqHolder|ShutdownChannelGroupException|Sides|Signature|SignatureException|SignatureMethod|SignatureMethodParameterSpec|SignatureProperties|SignatureProperty|SignatureSpi|SignedInfo|SignedObject|Signer|SignStyle|SimpleAnnotationValueVisitor6|SimpleAnnotationValueVisitor7|SimpleAnnotationValueVisitor8|SimpleAttributeSet|SimpleBeanInfo|SimpleBindings|SimpleDateFormat|SimpleDoc|SimpleElementVisitor6|SimpleElementVisitor7|SimpleElementVisitor8|SimpleFileVisitor|SimpleFormatter|SimpleJavaFileObject|SimpleScriptContext|SimpleTimeZone|SimpleType|SimpleTypeVisitor6|SimpleTypeVisitor7|SimpleTypeVisitor8|SinglePixelPackedSampleModel|SingleSelectionModel|Size2DSyntax|SizeLimitExceededException|SizeRequirements|SizeSequence|Skeleton|SkeletonMismatchException|SkeletonNotFoundException|SliderUI|SNIHostName|SNIMatcher|SNIServerName|SOAPBinding|SOAPBinding|SOAPBinding.ParameterStyle|SOAPBinding.Style|SOAPBinding.Use|SOAPBody|SOAPBodyElement|SOAPConnection|SOAPConnectionFactory|SOAPConstants|SOAPElement|SOAPElementFactory|SOAPEnvelope|SOAPException|SOAPFactory|SOAPFault|SOAPFaultElement|SOAPFaultException|SOAPHandler|SOAPHeader|SOAPHeaderElement|SOAPMessage|SOAPMessageContext|SOAPMessageHandler|SOAPMessageHandlers|SOAPPart|Socket|SocketAddress|SocketChannel|SocketException|SocketFactory|SocketHandler|SocketImpl|SocketImplFactory|SocketOption|SocketOptions|SocketPermission|SocketSecurityException|SocketTimeoutException|SoftBevelBorder|SoftReference|SortControl|SortedMap|SortedSet|SortingFocusTraversalPolicy|SortKey|SortOrder|SortResponseControl|Soundbank|SoundbankReader|SoundbankResource|Source|SourceDataLine|SourceLocator|SourceVersion|SpinnerDateModel|SpinnerListModel|SpinnerModel|SpinnerNumberModel|SpinnerUI|SplashScreen|Spliterator|Spliterator.OfDouble|Spliterator.OfInt|Spliterator.OfLong|Spliterator.OfPrimitive|Spliterators|Spliterators.AbstractDoubleSpliterator|Spliterators.AbstractIntSpliterator|Spliterators.AbstractLongSpliterator|Spliterators.AbstractSpliterator|SplitPaneUI|SplittableRandom|Spring|SpringLayout|SpringLayout.Constraints|SQLClientInfoException|SQLData|SQLDataException|SQLException|SQLFeatureNotSupportedException|SQLInput|SQLInputImpl|SQLIntegrityConstraintViolationException|SQLInvalidAuthorizationSpecException|SQLNonTransientConnectionException|SQLNonTransientException|SQLOutput|SQLOutputImpl|SQLPermission|SQLRecoverableException|SQLSyntaxErrorException|SQLTimeoutException|SQLTransactionRollbackException|SQLTransientConnectionException|SQLTransientException|SQLType|SQLWarning|SQLXML|SSLContext|SSLContextSpi|SSLEngine|SSLEngineResult|SSLEngineResult.HandshakeStatus|SSLEngineResult.Status|SSLException|SSLHandshakeException|SSLKeyException|SSLParameters|SSLPeerUnverifiedException|SSLPermission|SSLProtocolException|SslRMIClientSocketFactory|SslRMIServerSocketFactory|SSLServerSocket|SSLServerSocketFactory|SSLSession|SSLSessionBindingEvent|SSLSessionBindingListener|SSLSessionContext|SSLSocket|SSLSocketFactory|Stack|StackOverflowError|StackTraceElement|StampedLock|StandardCharsets|StandardConstants|StandardCopyOption|StandardEmitterMBean|StandardJavaFileManager|StandardLocation|StandardMBean|StandardOpenOption|StandardProtocolFamily|StandardSocketOptions|StandardWatchEventKinds|StartDocument|StartElement|StartTlsRequest|StartTlsResponse|State|State|StateEdit|StateEditable|StateFactory|Statement|Statement|StatementEvent|StatementEventListener|StAXResult|StAXSource|Stream|Stream.Builder|Streamable|StreamableValue|StreamCorruptedException|StreamFilter|StreamHandler|StreamPrintService|StreamPrintServiceFactory|StreamReaderDelegate|StreamResult|StreamSource|StreamSupport|StreamTokenizer|StrictMath|String|StringBuffer|StringBufferInputStream|StringBuilder|StringCharacterIterator|StringContent|StringHolder|StringIndexOutOfBoundsException|StringJoiner|StringMonitor|StringMonitorMBean|StringNameHelper|StringReader|StringRefAddr|StringSelection|StringSeqHelper|StringSeqHolder|StringTokenizer|StringValueExp|StringValueHelper|StringWriter|Stroke|StrokeBorder|Struct|StructMember|StructMemberHelper|Stub|StubDelegate|StubNotFoundException|Style|StyleConstants|StyleConstants.CharacterConstants|StyleConstants.ColorConstants|StyleConstants.FontConstants|StyleConstants.ParagraphConstants|StyleContext|StyledDocument|StyledEditorKit|StyledEditorKit.AlignmentAction|StyledEditorKit.BoldAction|StyledEditorKit.FontFamilyAction|StyledEditorKit.FontSizeAction|StyledEditorKit.ForegroundAction|StyledEditorKit.ItalicAction|StyledEditorKit.StyledTextAction|StyledEditorKit.UnderlineAction|StyleSheet|StyleSheet.BoxPainter|StyleSheet.ListPainter|Subject|SubjectDelegationPermission|SubjectDomainCombiner|SUCCESSFUL|Supplier|SupportedAnnotationTypes|SupportedOptions|SupportedSourceVersion|SupportedValuesAttribute|SuppressWarnings|SwingConstants|SwingPropertyChangeSupport|SwingUtilities|SwingWorker|SwingWorker.StateValue|SwitchPoint|SYNC_WITH_TRANSPORT|SyncFactory|SyncFactoryException|SyncFailedException|SynchronousQueue|SyncProvider|SyncProviderException|SyncResolver|SyncScopeHelper|SynthButtonUI|SynthCheckBoxMenuItemUI|SynthCheckBoxUI|SynthColorChooserUI|SynthComboBoxUI|SynthConstants|SynthContext|SynthDesktopIconUI|SynthDesktopPaneUI|SynthEditorPaneUI|Synthesizer|SynthFormattedTextFieldUI|SynthGraphicsUtils|SynthInternalFrameUI|SynthLabelUI|SynthListUI|SynthLookAndFeel|SynthMenuBarUI|SynthMenuItemUI|SynthMenuUI|SynthOptionPaneUI|SynthPainter|SynthPanelUI|SynthPasswordFieldUI|SynthPopupMenuUI|SynthProgressBarUI|SynthRadioButtonMenuItemUI|SynthRadioButtonUI|SynthRootPaneUI|SynthScrollBarUI|SynthScrollPaneUI|SynthSeparatorUI|SynthSliderUI|SynthSpinnerUI|SynthSplitPaneUI|SynthStyle|SynthStyleFactory|SynthTabbedPaneUI|SynthTableHeaderUI|SynthTableUI|SynthTextAreaUI|SynthTextFieldUI|SynthTextPaneUI|SynthToggleButtonUI|SynthToolBarUI|SynthToolTipUI|SynthTreeUI|SynthUI|SynthViewportUI|SysexMessage|System|SYSTEM_EXCEPTION|SystemColor|SystemException|SystemFlavorMap|SystemTray)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(TabableView|TabbedPaneUI|TabExpander|TableCellEditor|TableCellRenderer|TableColumn|TableColumnModel|TableColumnModelEvent|TableColumnModelListener|TableHeaderUI|TableModel|TableModelEvent|TableModelListener|TableRowSorter|TableStringConverter|TableUI|TableView|TabSet|TabStop|TabularData|TabularDataSupport|TabularType|TAG_ALTERNATE_IIOP_ADDRESS|TAG_CODE_SETS|TAG_INTERNET_IOP|TAG_JAVA_CODEBASE|TAG_MULTIPLE_COMPONENTS|TAG_ORB_TYPE|TAG_POLICIES|TAG_RMI_CUSTOM_MAX_STREAM_FORMAT|TagElement|TaggedComponent|TaggedComponentHelper|TaggedComponentHolder|TaggedProfile|TaggedProfileHelper|TaggedProfileHolder|Target|TargetDataLine|TargetedNotification|TCKind|Templates|TemplatesHandler|Temporal|TemporalAccessor|TemporalAdjuster|TemporalAdjusters|TemporalAmount|TemporalField|TemporalQueries|TemporalQuery|TemporalUnit|Text|Text|TextAction|TextArea|TextAttribute|TextComponent|TextEvent|TextField|TextHitInfo|TextInputCallback|TextLayout|TextLayout.CaretPolicy|TextListener|TextMeasurer|TextOutputCallback|TextStyle|TextSyntax|TextUI|TexturePaint|ThaiBuddhistChronology|ThaiBuddhistDate|ThaiBuddhistEra|Thread|Thread.State|Thread.UncaughtExceptionHandler|THREAD_POLICY_ID|ThreadDeath|ThreadFactory|ThreadGroup|ThreadInfo|ThreadLocal|ThreadLocalRandom|ThreadMXBean|ThreadPolicy|ThreadPolicyOperations|ThreadPolicyValue|ThreadPoolExecutor|ThreadPoolExecutor.AbortPolicy|ThreadPoolExecutor.CallerRunsPolicy|ThreadPoolExecutor.DiscardOldestPolicy|ThreadPoolExecutor.DiscardPolicy|Throwable|Tie|TileObserver|Time|TimeLimitExceededException|TIMEOUT|TimeoutException|Timer|Timer|Timer|TimerMBean|TimerNotification|TimerTask|Timestamp|Timestamp|TimeUnit|TimeZone|TimeZoneNameProvider|TitledBorder|ToDoubleBiFunction|ToDoubleFunction|ToIntBiFunction|ToIntFunction|ToLongBiFunction|ToLongFunction|Tool|ToolBarUI|Toolkit|ToolProvider|ToolTipManager|ToolTipUI|TooManyListenersException|Track|TRANSACTION_MODE|TRANSACTION_REQUIRED|TRANSACTION_ROLLEDBACK|TRANSACTION_UNAVAILABLE|TransactionalWriter|TransactionRequiredException|TransactionRolledbackException|TransactionService|Transferable|TransferHandler|TransferHandler.DropLocation|TransferHandler.TransferSupport|TransferQueue|Transform|TransformAttribute|Transformer|TransformerConfigurationException|TransformerException|TransformerFactory|TransformerFactoryConfigurationError|TransformerHandler|TransformException|TransformParameterSpec|TransformService|Transient|TRANSIENT|Transmitter|Transparency|TRANSPORT_RETRY|TrayIcon|TrayIcon.MessageType|TreeCellEditor|TreeCellRenderer|TreeExpansionEvent|TreeExpansionListener|TreeMap|TreeModel|TreeModelEvent|TreeModelListener|TreeNode|TreePath|TreeSelectionEvent|TreeSelectionListener|TreeSelectionModel|TreeSet|TreeUI|TreeWillExpandListener|TrustAnchor|TrustManager|TrustManagerFactory|TrustManagerFactorySpi|Type|TypeCode|TypeCodeHolder|TypeConstraintException|TypeElement|TypeInfo|TypeInfoProvider|TypeKind|TypeKindVisitor6|TypeKindVisitor7|TypeKindVisitor8|TypeMirror|TypeMismatch|TypeMismatch|TypeMismatch|TypeMismatchHelper|TypeMismatchHelper|TypeNotPresentException|TypeParameterElement|Types|Types|TypeVariable|TypeVariable|TypeVisitor|UID|UIDefaults|UIDefaults.ActiveValue|UIDefaults.LazyInputMap|UIDefaults.LazyValue|UIDefaults.ProxyLazyValue|UIEvent|UIManager|UIManager.LookAndFeelInfo|UIResource|ULongLongSeqHelper|ULongLongSeqHolder|ULongSeqHelper|ULongSeqHolder|UnaryOperator|UncheckedIOException|UndeclaredThrowableException|UndoableEdit|UndoableEditEvent|UndoableEditListener|UndoableEditSupport|UndoManager|UnexpectedException|UnicastRemoteObject|UnionMember|UnionMemberHelper|UnionType|UNKNOWN|UNKNOWN|UnknownAnnotationValueException|UnknownElementException|UnknownEncoding|UnknownEncodingHelper|UnknownEntityException|UnknownError|UnknownException|UnknownFormatConversionException|UnknownFormatFlagsException|UnknownGroupException|UnknownHostException|UnknownHostException|UnknownObjectException|UnknownServiceException|UnknownTypeException|UnknownUserException|UnknownUserExceptionHelper|UnknownUserExceptionHolder|UnmappableCharacterException|UnmarshalException|UnmarshalException|Unmarshaller|Unmarshaller.Listener|UnmarshallerHandler|UnmodifiableClassException|UnmodifiableSetException|UnrecoverableEntryException|UnrecoverableKeyException|Unreferenced|UnresolvedAddressException|UnresolvedPermission|UnsatisfiedLinkError|UnsolicitedNotification|UnsolicitedNotificationEvent|UnsolicitedNotificationListener|UNSUPPORTED_POLICY|UNSUPPORTED_POLICY_VALUE|UnsupportedAddressTypeException|UnsupportedAudioFileException|UnsupportedCallbackException|UnsupportedCharsetException|UnsupportedClassVersionError|UnsupportedDataTypeException|UnsupportedEncodingException|UnsupportedFlavorException|UnsupportedLookAndFeelException|UnsupportedOperationException|UnsupportedTemporalTypeException|URI|URIDereferencer|URIException|URIParameter|URIReference|URIReferenceException|URIResolver|URISyntax|URISyntaxException|URL|URLClassLoader|URLConnection|URLDataSource|URLDecoder|URLEncoder|URLPermission|URLStreamHandler|URLStreamHandlerFactory|URLStringHelper|USER_EXCEPTION|UserDataHandler|UserDefinedFileAttributeView|UserException|UserPrincipal|UserPrincipalLookupService|UserPrincipalNotFoundException|UShortSeqHelper|UShortSeqHolder|UTFDataFormatException|Util|UtilDelegate|Utilities|UUID)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);

        rule.pattern.setPattern ("\\b(ValidationEvent|ValidationEventCollector|ValidationEventHandler|ValidationEventImpl|ValidationEventLocator|ValidationEventLocatorImpl|ValidationException|Validator|Validator|ValidatorHandler|ValueBase|ValueBaseHelper|ValueBaseHolder|ValueExp|ValueFactory|ValueHandler|ValueHandlerMultiFormat|ValueInputStream|ValueMember|ValueMemberHelper|ValueOutputStream|ValueRange|VariableElement|VariableHeightLayoutCache|Vector|VerifyError|VersionSpecHelper|VetoableChangeListener|VetoableChangeListenerProxy|VetoableChangeSupport|View|ViewFactory|ViewportLayout|ViewportUI|VirtualMachineError|Visibility|VisibilityHelper|VM_ABSTRACT|VM_CUSTOM|VM_NONE|VM_TRUNCATABLE|VMID|VoiceStatus|Void|VolatileCallSite|VolatileImage|W3CDomHandler|W3CEndpointReference|W3CEndpointReferenceBuilder|Watchable|WatchEvent|WatchEvent.Kind|WatchEvent.Modifier|WatchKey|WatchService|WCharSeqHelper|WCharSeqHolder|WeakHashMap|WeakReference|WebEndpoint|WebFault|WebMethod|WebParam|WebParam.Mode|WebResult|WebRowSet|WebService|WebServiceClient|WebServiceContext|WebServiceException|WebServiceFeature|WebServiceFeatureAnnotation|WebServicePermission|WebServiceProvider|WebServiceRef|WebServiceRefs|WeekFields|WildcardType|WildcardType|Window|Window.Type|WindowAdapter|WindowConstants|WindowEvent|WindowFocusListener|WindowListener|WindowStateListener|WrappedPlainView|Wrapper|WritableByteChannel|WritableRaster|WritableRenderedImage|WriteAbortedException|WritePendingException|Writer|WrongAdapter|WrongAdapterHelper|WrongMethodTypeException|WrongPolicy|WrongPolicyHelper|WrongTransaction|WrongTransactionHelper|WrongTransactionHolder|WStringSeqHelper|WStringSeqHolder|WStringValueHelper|X500Principal|X500PrivateCredential|X509Certificate|X509Certificate|X509CertSelector|X509CRL|X509CRLEntry|X509CRLSelector|X509Data|X509EncodedKeySpec|X509ExtendedKeyManager|X509ExtendedTrustManager|X509Extension|X509IssuerSerial|X509KeyManager|X509TrustManager|XAConnection|XADataSource|XAException|XAResource|Xid|XmlAccessOrder|XmlAccessorOrder|XmlAccessorType|XmlAccessType|XmlAdapter|XmlAnyAttribute|XmlAnyElement|XmlAttachmentRef|XmlAttribute|XMLConstants|XMLCryptoContext|XMLDecoder|XmlElement|XmlElement.DEFAULT|XmlElementDecl|XmlElementDecl.GLOBAL|XmlElementRef|XmlElementRef.DEFAULT|XmlElementRefs|XmlElements|XmlElementWrapper|XMLEncoder|XmlEnum|XmlEnumValue|XMLEvent|XMLEventAllocator|XMLEventConsumer|XMLEventFactory|XMLEventReader|XMLEventWriter|XMLFilter|XMLFilterImpl|XMLFormatter|XMLGregorianCalendar|XmlID|XmlIDREF|XmlInlineBinaryData|XMLInputFactory|XmlJavaTypeAdapter|XmlJavaTypeAdapter.DEFAULT|XmlJavaTypeAdapters|XmlList|XmlMimeType|XmlMixed|XmlNs|XmlNsForm|XMLObject|XMLOutputFactory|XMLParseException|XmlReader|XMLReader|XMLReaderAdapter|XMLReaderFactory|XmlRegistry|XMLReporter|XMLResolver|XmlRootElement|XmlSchema|XmlSchemaType|XmlSchemaType.DEFAULT|XmlSchemaTypes|XmlSeeAlso|XMLSignature|XMLSignature.SignatureValue|XMLSignatureException|XMLSignatureFactory|XMLSignContext|XMLStreamConstants|XMLStreamException|XMLStreamReader|XMLStreamWriter|XMLStructure|XmlTransient|XmlType|XmlType.DEFAULT|XMLValidateContext|XmlValue|XmlWriter|XPath|XPathConstants|XPathException|XPathExpression|XPathExpressionException|XPathFactory|XPathFactoryConfigurationException|XPathFilter2ParameterSpec|XPathFilterParameterSpec|XPathFunction|XPathFunctionException|XPathFunctionResolver|XPathType|XPathType.Filter|XPathVariableResolver|XSLTTransformParameterSpec|Year|YearMonth|ZipEntry|ZipError|ZipException|ZipFile|ZipInputStream|ZipOutputStream|ZonedDateTime|ZoneId|ZoneOffset|ZoneOffsetTransition|ZoneOffsetTransitionRule|ZoneOffsetTransitionRule.TimeDefinition|ZoneRules|ZoneRulesException|ZoneRulesProvider|ZoneView|_BindingIteratorImplBase|_BindingIteratorStub|_DynAnyFactoryStub|_DynAnyStub|_DynArrayStub|_DynEnumStub|_DynFixedStub|_DynSequenceStub|_DynStructStub|_DynUnionStub|_DynValueStub|_IDLTypeStub|_NamingContextExtStub|_NamingContextImplBase|_NamingContextStub|_PolicyStub|_Remote_Stub|_ServantActivatorStub|_ServantLocatorStub)(?!(@|#|\\$))\\b");
        rule.format = javaFormat;
        highlightingRules.append (rule);
    }
    else if (progLan == "json")
    {
        quoteFormat.setFontWeight (QFont::Bold);
        errorFormat.setForeground (Red);
        errorFormat.setFontUnderline (true);
    }

    if (showWhiteSpace)
    {
        rule.pattern.setPattern ("\\s+");
        rule.format = whiteSpaceFormat;
        highlightingRules.append (rule);
    }

    /************
     * Comments *
     ************/

    /* single line comments */
    rule.pattern.setPattern (QString());
    if (progLan == "c" || progLan == "cpp"
        || Lang == "javascript" || progLan == "qml"
        || progLan == "scss" || progLan == "dart"
        || progLan == "go" || progLan == "java")
    {
        rule.pattern.setPattern ("//.*"); // why had I set it to ("//(?!\\*).*")?
    }
    else if (progLan == "php")
    {
        rule.pattern.setPattern ("(//|#).*");
    }
    else if (progLan == "python"
             || progLan == "qmake"
             || progLan == "gtkrc")
    {
        rule.pattern.setPattern ("#.*"); // or "#[^\n]*"
    }
    else if (progLan == "desktop" || progLan == "config" || progLan == "theme")
    {
        rule.pattern.setPattern ("^\\s*#.*"); // only at start
    }
    /*else if (progLan == "deb")
    {
        rule.pattern.setPattern ("^#[^\\s:]+:(?=\\s*)");
    }*/
    else if (progLan == "m3u")
    {
        rule.pattern.setPattern ("^\\s+#|^#(?!(EXTM3U|EXTINF))");
    }
    else if (progLan == "lua")
        rule.pattern.setPattern ("--(?!\\[).*");
    else if (progLan == "troff")
        rule.pattern.setPattern ("\\\\\"|\\\\#|\\.\\s*\\\\\"");
    else if (progLan == "LaTeX")
        rule.pattern.setPattern ("%.*");
    if (!rule.pattern.pattern().isEmpty())
    {
        rule.format = commentFormat;
        highlightingRules.append (rule);
    }

    /* multiline comments */
    if (progLan == "c" || progLan == "cpp"
        || progLan == "javascript" || progLan == "qml"
        || progLan == "php" || progLan == "css" || progLan == "scss"
        || progLan == "fountain" || progLan == "dart"
        || progLan == "go" || progLan == "java")
    {
        commentStartExpression.setPattern ("/\\*");
        commentEndExpression.setPattern ("\\*/");
    }
    else if (progLan == "lua")
    {
        commentStartExpression.setPattern ("\\[\\[|--\\[\\[");
        commentEndExpression.setPattern ("\\]\\]");
    }
    else if (progLan == "python")
    {
        commentStartExpression.setPattern ("\"\"\"|\'\'\'");
        commentEndExpression = commentStartExpression;
    }
    else if (progLan == "xml")
    {
        commentStartExpression.setPattern ("<!--");
        commentEndExpression.setPattern ("-->");
    }
    else if (progLan == "html")
    {
        errorFormat.setForeground (Red);
        errorFormat.setFontUnderline (true);

        htmlCommetStart.setPattern ("<!--");
        htmlCommetEnd.setPattern ("-->");

        /* CSS and JS inside HTML (see htmlCSSHighlighter and htmlJavascript) */
        htmlSubcommetStart.setPattern ("/\\*");
        htmlSubcommetEnd.setPattern ("\\*/");

        commentStartExpression = htmlCommetStart;
        commentEndExpression = htmlCommetEnd;
    }
    else if (progLan == "perl")
    {
        commentStartExpression.setPattern ("^=[A-Za-z0-9_]+($|\\s+)");
        commentEndExpression.setPattern ("^=cut.*");
    }
    else if (progLan == "ruby")
    {
        commentStartExpression.setPattern ("=begin\\s*$");
        commentEndExpression.setPattern ("^=end\\s*$");
    }
    else if (progLan == "markdown")
    {
        quoteFormat.setForeground (DarkRed); // not a quote but a code block
        urlInsideQuoteFormat.setForeground (DarkRed);
        commentStartExpression.setPattern ("<!--");
        commentEndExpression.setPattern ("-->");
    }
}
/*************************/
Highlighter::~Highlighter()
{
    if (QTextDocument *doc = document())
    {
        QTextOption opt =  doc->defaultTextOption();
        opt.setFlags (opt.flags() & ~QTextOption::ShowTabsAndSpaces
                                  & ~QTextOption::ShowLineAndParagraphSeparators
                                  & ~QTextOption::AddSpaceForLineAndParagraphSeparators
                                  & ~QTextOption::ShowDocumentTerminator);
        doc->setDefaultTextOption (opt);
    }
}
/*************************/
// Should be used only with characters that can be escaped in a language.
bool Highlighter::isEscapedChar (const QString &text, const int pos) const
{
    if (pos < 1) return false;
    int i = 0;
    while (pos - i - 1 >= 0 && text.at (pos - i - 1) == '\\')
        ++i;
    if (i % 2 != 0)
        return true;
    return false;
}
/*************************/
// Checks if a start quote is inside a Yaml key (as in ab""c).
bool Highlighter::isYamlKeyQuote (const QString &key, const int pos)
{
    static int lastKeyQuote = -1;
    int indx = pos;
    if (indx > 0 && indx < key.length())
    {
        while (indx > 0 && key.at (indx - 1).isSpace())
            --indx;
        if (indx > 0)
        {
            QChar c = key.at (indx - 1);
            if (c != '\"' && c != '\'')
            {
                lastKeyQuote = pos;
                return true;
            }
            if (lastKeyQuote == indx - 1)
            {
                lastKeyQuote = pos;
                return true;
            }
        }
    }
    lastKeyQuote = -1;
    return false;
}
/*************************/
// Check if a start or end quotation mark (positioned at "pos") is escaped.
// If "skipCommandSign" is true (only for SH), start double quotes are escaped before "$(".
bool Highlighter::isEscapedQuote (const QString &text, const int pos, bool isStartQuote,
                                  bool skipCommandSign)
{
    if (pos < 0) return false;

    if (progLan == "html" || progLan == "xml")
        return false;

    /* with markdown, something should be inside quotes */
    if (progLan == "markdown" && isStartQuote
        && (pos >= text.length() - 1 || text.at (pos + 1) == '`'))
    {
        return true;
    }

    if (progLan == "yaml")
    {
        if (isStartQuote)
        {
            if (format (pos) == codeBlockFormat) // inside a literal block
                return true;
            QRegularExpressionMatch match;
            if (text.indexOf (QRegularExpression ("^(\\s*-\\s)+\\s*"), 0, &match) == 0)
            {
                if (match.capturedLength() == pos)
                    return false; // a start quote isn't escaped at the beginning of a list
            }
            /* Skip the start quote if it's inside a key or value.
               NOTE: In keys, "(?:(?!(\\{|\\[|,|:\\s|\\s#)).)*" is used instead of "[^{\\[:,#]*"
                     because ":" should be followed by a space to make a key-value. */
            if (format (pos) == neutralFormat)
            { // inside preformatted braces, when multiLineQuote() is called (not needed; repeated below)
                int index = text.lastIndexOf (QRegularExpression ("(^|{|,|\\[)\\s*\\K(?:(?!(\\{|\\[|,|:\\s|\\s#)).)*(:\\s+)?"), pos, &match);
                if (index > -1 && index <= pos && index + match.capturedLength() > pos
                    && isYamlKeyQuote (match.captured(), pos - index))
                {
                    return true;
                }
                index = text.lastIndexOf (QRegularExpression ("(^|{|,|\\[)[^:#]*:\\s+\\K[^{\\[,#\\s][^,#]*"), pos, &match);
                if (index > -1 && index < pos && index + match.capturedLength() > pos)
                    return true;
            }
            else
            {
                /* inside braces before preformatting (indirectly used by yamlOpenBraces()) */
                int index = text.lastIndexOf (QRegularExpression ("(^|{|,|\\[)\\s*\\K(?:(?!(\\{|\\[|,|:\\s|\\s#)).)*(:\\s+)?"), pos, &match);
                if (index > -1 && index <= pos && index + match.capturedLength() > pos
                    && isYamlKeyQuote (match.captured(), pos - index))
                {
                    return true;
                }
                index = text.lastIndexOf (QRegularExpression ("(^|{|,|\\[)[^:#]*:\\s+\\K[^{\\[,#\\s][^,#]*"), pos, &match);
                if (index > -1 && index < pos && index + match.capturedLength() > pos)
                    return true;
                /* outside braces */
                index = text.lastIndexOf (QRegularExpression ("^\\s*\\K(?:(?!(\\{|\\[|,|:\\s|\\s#)).)*(:\\s+)?"), pos, &match);
                if (index > -1 && index < pos && index + match.capturedLength() > pos
                    && isYamlKeyQuote (match.captured(), pos - index))
                {
                    return true;
                }
                index = text.lastIndexOf (QRegularExpression ("^[^:#]*:\\s+\\K[^\\[\\s#].*"), pos, &match);
                if (index > -1 && index < pos && index + match.capturedLength() > pos)
                    return true;
            }
        }
        else if (text.length() > pos && text.at (pos) == '\'')
        { // a pair of single quotes means escaping them
            static int lastEscapedQuote = -1;
            if (lastEscapedQuote == pos && pos > 0 && text.at (pos - 1) == '\'') // the second quote
                return true;
            if ((pos == 0 || text.at (pos - 1) != '\'' || lastEscapedQuote == pos - 1)
                && text.length() > pos + 1 && text.at (pos + 1) == '\'')
            { // the first quote
                lastEscapedQuote = pos + 1;
                return true;
            }
            lastEscapedQuote = -1;
        }
    }
    else if (progLan == "go")
    {
        if (text.at (pos) == '`')
            return false;
        if (isStartQuote)
        {
            if (text.at (pos) == '\"'
                && pos > 0 && pos < text.length() - 1
                && text.at (pos + 1) == '\''
                && (text.at (pos - 1) == '\''
                    || (pos > 1 && text.at (pos - 2) == '\'' && text.at (pos - 1) == '\\')))
            {
                return true; // '"' or '\"'
            }
            else
                return false;
        }
        return isEscapedChar (text, pos);
    }

    /* there's no need to check for quote marks because this function is used only with them */
    /*if (progLan == "perl"
        && pos != text.indexOf (quoteMark, pos)
        && pos != text.indexOf ("\'", pos)
        && pos != text.indexOf ("`", pos))
    {
        return false;
    }
    if (pos != text.indexOf (quoteMark, pos)
        && (progLan == "markdown" || pos != text.indexOf (QRegularExpression ("\'"), pos)))
    {
        return false;
    }*/

    /* check if the quote surrounds a here-doc delimiter */
    if ((currentBlockState() >= endState || currentBlockState() < -1)
        && currentBlockState() % 2 == 0)
    {
        QRegularExpressionMatch match;
        QRegularExpression delimPart (progLan == "ruby" ? "<<(-|~){0,1}" : "<<\\s*");
        if (text.lastIndexOf (delimPart, pos, &match) == pos - match.capturedLength())
            return true; // escaped start quote
        if (progLan == "perl") // space is allowed
            delimPart.setPattern ("<<(?:\\s*)(\'[A-Za-z0-9_\\s]+)|<<(?:\\s*)(\"[A-Za-z0-9_\\s]+)|<<(?:\\s*)(`[A-Za-z0-9_\\s]+)");
        else if (progLan == "ruby")
            delimPart.setPattern ("<<(?:-|~){0,1}(\'[A-Za-z0-9]+)|<<(?:-|~){0,1}(\"[A-Za-z0-9]+)");
        else
            delimPart.setPattern ("<<(?:\\s*)(\'[A-Za-z0-9_]+)|<<(?:\\s*)(\"[A-Za-z0-9_]+)");
        if (text.lastIndexOf (delimPart, pos, &match) == pos - match.capturedLength())
            return true; // escaped end quote
    }

    /* escaped start quotes are just for Bash, Perl, markdown and yaml */
    if (isStartQuote)
    {
        if (progLan == "perl")
        {
            if (pos >= 1)
            {
                if (text.at (pos - 1) == '$') // in Perl, $' has a (deprecated?) meaning
                    return true;
                if (text.at (pos) == '\'')
                {
                    if (text.at (pos - 1) == '&')
                        return true;
                    int index = pos - 1;
                    while (index >= 0 && (text.at (index).isLetterOrNumber() || text.at (index) == '_'))
                        -- index;
                    if (index >= 0 && (text.at (index) == '$' || text.at (index) == '@'
                                       || text.at (index) == '%' || text.at (index) == '*'
                                       /*|| text.at (index) == '!'*/))
                    {
                        return true;
                    }
                }
            }
            return false; // no other case of escaping at the start
        }
        else if (progLan != "sh" && progLan != "makefile" && progLan != "cmake"
                 && progLan != "markdown" && progLan != "yaml")
        {
            return false;
        }
    }

    if (isStartQuote && skipCommandSign && text.at (pos) == quoteMark.pattern().at (0)
        && text.indexOf (QRegularExpression ("[^\"]*\\$\\("), pos) == pos + 1)
    {
        return true;
    }

    int i = 0;
    while (pos - i > 0 && text.at (pos - i - 1) == '\\')
        ++i;
    /* only an odd number of backslashes means that the quote is escaped */
    if (
        i % 2 != 0
        && ((progLan == "yaml"
             && text.at (pos) == quoteMark.pattern().at (0))
            /* for these languages, both single and double quotes can be escaped (also for perl?) */
            || progLan == "c" || progLan == "cpp"
            || progLan == "javascript" || progLan == "qml"
            || progLan == "python"
            || progLan == "perl"
            || progLan == "dart"
            || progLan == "php"
            || progLan == "ruby"
            || progLan == "lua"
            /* markdown is an exception */
            || progLan == "markdown"
            /* however, in Bash, single quote can be escaped only at start */
            || ((progLan == "sh" || progLan == "makefile" || progLan == "cmake")
                && (isStartQuote || text.at (pos) == quoteMark.pattern().at (0))))
       )
    {
        return true;
    }

    if (progLan == "ruby" && text.at (pos) == quoteMark.pattern().at (0))
    { // a minimal support for command substitution "#{...}"
        QRegularExpressionMatch match;
        int index = text.lastIndexOf (QRegularExpression ("#\\{[^\\}]*"), pos, &match);
        if (index > -1 && index < pos && index + match.capturedLength() > pos)
            return true;
    }

    return false;
}
/*************************/
// Checks if a character is inside quotation marks, considering the language
// (should be used with care because it gives correct results only in special places).
// If "skipCommandSign" is true (only for SH), start double quotes are escaped before "$(".
bool Highlighter::isQuoted (const QString &text, const int index,
                            bool skipCommandSign, const int start)
{
    if (progLan == "perl" || progLan == "ruby")
        return isPerlQuoted (text, index);
    if (progLan == "javascript" || progLan == "qml")
        return isJSQuoted (text, index);

    if (index < 0 || start < 0 || index < start)
        return false;

    int pos = start - 1;

    bool res = false;
    int N;
    QRegularExpression quoteExpression;
    if (mixedQuotes_)
        quoteExpression = mixedQuoteMark;
    else
        quoteExpression = quoteMark;
    if (pos == -1)
    {
        int prevState = previousBlockState();
        if ((prevState < doubleQuoteState
             || prevState > SH_MixedSingleQuoteState)
                && prevState != htmlStyleSingleQuoteState
                && prevState != htmlStyleDoubleQuoteState)
        {
            N = 0;
        }
        else
        {
            N = 1;
            res = true;
            if (mixedQuotes_)
            {
                if (prevState == doubleQuoteState
                    || prevState == SH_DoubleQuoteState
                    || prevState == SH_MixedDoubleQuoteState
                    || prevState == htmlStyleDoubleQuoteState)
                {
                    quoteExpression = quoteMark;
                    if (skipCommandSign)
                    {
                        if (text.indexOf (QRegularExpression ("[^\"]*\\$\\("), 0) == 0)
                        {
                            N = 0;
                            res = false;
                        }
                        else
                        {
                            QTextBlock prevBlock = currentBlock().previous();
                            if (prevBlock.isValid())
                            {
                                if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                                {
                                    int N = prevData->openNests();
                                    if (N > 0 && (prevState == doubleQuoteState || !prevData->openQuotes().contains (N)))
                                    {
                                        N = 0;
                                        res = false;
                                    }
                                }
                            }
                        }
                    }
                }
                else
                    quoteExpression = singleQuoteMark;
            }
        }
    }
    else N = 0; // a new search from the last position

    int nxtPos;
    while ((nxtPos = text.indexOf (quoteExpression, pos + 1)) >= 0)
    {
        /* skip formatted comments */
        if (format (nxtPos) == commentFormat || format (nxtPos) == urlFormat)
        {
            pos = nxtPos;
            continue;
        }

        ++N;
        if ((N % 2 == 0 // an escaped end quote...
             && isEscapedQuote (text, nxtPos, false))
            || (N % 2 != 0 // ... or an escaped start quote Bash...
                && (isEscapedQuote (text, nxtPos, true, skipCommandSign)
                    /*|| isInsideRegex (text, nxtPos)*/))) // ... or a start quote inside regex
        {
            --N;
            pos = nxtPos;
            continue;
        }

        if (index < nxtPos)
        {
            if (N % 2 == 0) res = true;
            else res = false;
            break;
        }

        /* "pos" might be negative next time */
        if (N % 2 == 0) res = false;
        else res = true;

        if (mixedQuotes_)
        {
            if (N % 2 != 0)
            { // each quote neutralizes the other until it's closed
                if (text.at (nxtPos) == quoteMark.pattern().at (0))
                    quoteExpression = quoteMark;
                else
                    quoteExpression = singleQuoteMark;
            }
            else
                quoteExpression = mixedQuoteMark;
        }
        pos = nxtPos;
    }

    return res;
}
/*************************/
// Perl has a separate method to support backquotes.
// Also see multiLinePerlQuote().
bool Highlighter::isPerlQuoted (const QString &text, const int index)
{
    if (index < 0) return false;

    int pos = -1;

    if (format (index) == quoteFormat || format (index) == altQuoteFormat)
        return true;
    if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
    {
        pos = data->lastFormattedQuote() - 1;
        if (index <= pos) return false;
    }

    bool res = false;
    int N;
    QRegularExpression quoteExpression = mixedQuoteBackquote;
    if (pos == -1)
    {
        int prevState = previousBlockState();
        if (prevState != doubleQuoteState && prevState != singleQuoteState)
            N = 0;
        else
        {
            N = 1;
            res = true;
            if (prevState == doubleQuoteState)
                quoteExpression = quoteMark;
            else
            {
                bool backquoted (false);
                QTextBlock prevBlock = currentBlock().previous();
                if (prevBlock.isValid())
                {
                    TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData());
                    if (prevData && prevData->getProperty())
                        backquoted = true;
                }
                if (backquoted)
                    quoteExpression = backQuote;
                else
                    quoteExpression = singleQuoteMark;
            }
        }
    }
    else N = 0; // a new search from the last position

    int nxtPos;
    while ((nxtPos = text.indexOf (quoteExpression, pos + 1)) >= 0)
    {
        /* skip formatted comments */
        if (format (nxtPos) == commentFormat || format (nxtPos) == urlFormat
            || (N % 2 == 0 && isMLCommented (text, nxtPos, commentState)))
        {
            pos = nxtPos;
            continue;
        }

        ++N;
        if ((N % 2 == 0 // an escaped end quote...
             && isEscapedQuote (text, nxtPos, false))
            || (N % 2 != 0 // ... or an escaped start quote
                && (isEscapedQuote (text, nxtPos, true, false) || isInsideRegex (text, nxtPos))))
        {
            if (res)
            { // -> isEscapedRegex()
                pos = qMax (pos, 0);
                if (text.at (nxtPos) == quoteMark.pattern().at (0))
                    setFormat (pos, nxtPos - pos + 1, quoteFormat);
                else
                    setFormat (pos, nxtPos - pos + 1, altQuoteFormat);
            }
            --N;
            pos = nxtPos;
            continue;
        }

        if (N % 2 == 0)
        { // -> isEscapedRegex()
            if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
                data->insertLastFormattedQuote (nxtPos + 1);
            pos = qMax (pos, 0);
            if (text.at (nxtPos) == quoteMark.pattern().at (0))
                setFormat (pos, nxtPos - pos + 1, quoteFormat);
            else
                setFormat (pos, nxtPos - pos + 1, altQuoteFormat);
        }

        if (index < nxtPos)
        {
            if (N % 2 == 0) res = true;
            else res = false;
            break;
        }

        /* "pos" might be negative next time */
        if (N % 2 == 0) res = false;
        else res = true;

        if (N % 2 != 0)
        { // each quote neutralizes the other until it's closed
            if (text.at (nxtPos) == quoteMark.pattern().at (0))
                quoteExpression = quoteMark;
            else if (text.at (nxtPos) == '\'')
                quoteExpression = singleQuoteMark;
            else
                quoteExpression = backQuote;
        }
        else
            quoteExpression = mixedQuoteBackquote;
        pos = nxtPos;
    }

    return res;
}
/*************************/
// JS has a separate method to support backquotes (template literals).
// Also see multiLineJSlQuote().
bool Highlighter::isJSQuoted (const QString &text, const int index)
{
    if (index < 0) return false;

    int pos = -1;

    /* with regex, the text will be formatted below to know whether
       the regex start sign is quoted (-> isEscapedRegex) */
    if (format (index) == quoteFormat || format (index) == altQuoteFormat)
        return true;
    if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
    {
        pos = data->lastFormattedQuote() - 1;
        if (index <= pos) return false;
    }

    bool res = false;
    int N;
    QRegularExpression quoteExpression = mixedQuoteBackquote;
    if (pos == -1)
    {
        int prevState = previousBlockState();
        if (prevState != doubleQuoteState
            && prevState != singleQuoteState
            && prevState != JS_templateLiteralState)
        {
            N = 0;
        }
        else
        {
            N = 1;
            res = true;
            if (prevState == doubleQuoteState)
                quoteExpression = quoteMark;
            else if (prevState == singleQuoteState)
                quoteExpression = singleQuoteMark;
            else
                quoteExpression = backQuote;
        }
    }
    else N = 0; // a new search from the last position

    int nxtPos;
    while ((nxtPos = text.indexOf (quoteExpression, pos + 1)) >= 0)
    {
        /* skip formatted comments */
        if (format (nxtPos) == commentFormat || format (nxtPos) == urlFormat
            || (N % 2 == 0
                && (isMLCommented (text, nxtPos, commentState)
                    || isMLCommented (text, nxtPos, htmlJavaCommentState))))
        {
            pos = nxtPos;
            continue;
        }

        ++N;
        if ((N % 2 == 0 // an escaped end quote...
             && isEscapedQuote (text, nxtPos, false))
            || (N % 2 != 0 // ... or a start quote inside regex (JS has no escaped start quote)
                && isInsideRegex (text, nxtPos)))
        {
            if (res)
            { // -> isEscapedRegex()
                pos = qMax (pos, 0);
                if (text.at (nxtPos) == quoteMark.pattern().at (0))
                    setFormat (pos, nxtPos - pos + 1, quoteFormat);
                else
                    setFormat (pos, nxtPos - pos + 1, altQuoteFormat);
            }
            --N;
            pos = nxtPos;
            continue;
        }

        if (N % 2 == 0)
        { // -> isEscapedRegex()
            if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
                data->insertLastFormattedQuote (nxtPos + 1);
            pos = qMax (pos, 0);
            if (text.at (nxtPos) == quoteMark.pattern().at (0))
                setFormat (pos, nxtPos - pos + 1, quoteFormat);
            else
                setFormat (pos, nxtPos - pos + 1, altQuoteFormat);
        }

        if (index < nxtPos)
        {
            if (N % 2 == 0) res = true;
            else res = false;
            break;
        }

        /* "pos" might be negative next time */
        if (N % 2 == 0) res = false;
        else res = true;

        if (N % 2 != 0)
        { // each quote neutralizes the other until it's closed
            if (text.at (nxtPos) == quoteMark.pattern().at (0))
                quoteExpression = quoteMark;
            else if (text.at (nxtPos) == '\'')
                quoteExpression = singleQuoteMark;
            else
                quoteExpression = backQuote;
        }
        else
            quoteExpression = mixedQuoteBackquote;
        pos = nxtPos;
    }

    return res;
}
/*************************/
// Checks if a start quote or a start single-line comment sign is inside a multiline comment.
// If "start" > 0, it will be supposed that "start" is not inside a previous comment.
// (May give an incorrect result with other characters and works only with real comments
// whose state is "comState".)
bool Highlighter::isMLCommented (const QString &text, const int index, int comState,
                                 const int start)
{
    if (index < 0 || start < 0 || index < start
        // commentEndExpression is always set if commentStartExpression is
        || commentStartExpression.pattern().isEmpty())
    {
        return false;
    }

    /* not for Python */
    if (progLan == "python") return false;

    int prevState = previousBlockState();
    if (prevState == nextLineCommentState)
        return true; // see singleLineComment()

    bool res = false;
    int pos = start - 1;
    int N;
    QRegularExpressionMatch commentMatch;
    QRegularExpression commentExpression;
    if (pos >= 0 || prevState != comState)
    {
        N = 0;
        commentExpression = commentStartExpression;
    }
    else
    {
        N = 1;
        res = true;
        commentExpression = commentEndExpression;
    }

    while ((pos = text.indexOf (commentExpression, pos + 1, &commentMatch)) >= 0)
    {
        /* skip formatted quotations and regex */
        QTextCharFormat fi = format (pos);
        if (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
            || fi == regexFormat) // see multiLineRegex() for the reason
        {
            continue;
        }

        ++N;

        /* All (or most) multiline comments have more than one character
           and this trick is needed for knowing if a double slash follows
           an asterisk without using "lookbehind", for example. */
        if (index < pos + (N % 2 == 0 ? commentMatch.capturedLength() : 0))
        {
            if (N % 2 == 0) res = true;
            else res = false;
            break;
        }

        if (N % 2 != 0)
        {
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
// This handles multiline python comments separately because they aren't normal.
// It comes after singleLineComment() and before multiLineQuote().
void Highlighter::pythonMLComment (const QString &text, const int indx)
{
    if (progLan != "python") return;

    /* we reset the block state because this method is also called
       during the multiline quotation formatting after clearing formats */
    setCurrentBlockState (-1);

    int index = indx;
    int quote;

    /* find the start quote */
    int prevState = previousBlockState();
    if (prevState != pyDoubleQuoteState
        && prevState != pySingleQuoteState)
    {
        index = text.indexOf (commentStartExpression, indx);

        QTextCharFormat fi = format (index);
        while ((index > 0 && isQuoted (text, index - 1)) // because two quotes may follow an end quote
               || (index == 0 && (prevState == doubleQuoteState || prevState == singleQuoteState))
               || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat) // not needed
        {
            index = text.indexOf (commentStartExpression, index + 3);
            fi = format (index);
        }
        if (format (index) == commentFormat || format (index) == urlFormat)
            return; // inside a single-line comment

        /* if the comment start is found... */
        if (index >= indx)
        {
            /* ... distinguish between double and single quotes */
            if (index == text.indexOf (QRegularExpression ("\"\"\""), index))
            {
                commentStartExpression.setPattern ("\"\"\"");
                quote = pyDoubleQuoteState;
            }
            else
            {
                commentStartExpression.setPattern ("\'\'\'");
                quote = pySingleQuoteState;
            }
        }
    }
    else // but if we're inside a triple quotation...
    {
        /* ... distinguish between the two quote kinds
           by checking the previous line */
        quote = prevState;
        if (quote == pyDoubleQuoteState)
            commentStartExpression.setPattern ("\"\"\"");
        else
            commentStartExpression.setPattern ("\'\'\'");
    }

    while (index >= indx)
    {
        /* if the search is continued... */
        if (commentStartExpression.pattern() == "\"\"\"|\'\'\'")
        {
            /* ... distinguish between double and single quotes
               again because the quote mark may have changed... */
            if (text.at (index) == quoteMark.pattern().at (0))
            {
                commentStartExpression.setPattern ("\"\"\"");
                quote = pyDoubleQuoteState;
            }
            else
            {
                commentStartExpression.setPattern ("\'\'\'");
                quote = pySingleQuoteState;
            }
        }

        /* search for the end quote from the start quote */
        QRegularExpressionMatch startMatch;
        int endIndex = text.indexOf (commentStartExpression, index + 3, &startMatch);

        /* but if there's no start quote ... */
        if (index == indx
            && (prevState == pyDoubleQuoteState || prevState == pySingleQuoteState))
        {
            /* ... search for the end quote from the line start */
            endIndex = text.indexOf (commentStartExpression, indx, &startMatch);
        }

        /* check if the quote is escaped */
        while ((endIndex > 0 && text.at (endIndex - 1) == '\\'
                /* backslash shouldn't be escaped itself */
                && (endIndex < 2 || text.at (endIndex - 2) != '\\'))
                   /* also consider ^' and ^" */
                   || ((endIndex > 0 && text.at (endIndex - 1) == '^')
                       && (endIndex < 2 || text.at (endIndex - 2) != '\\')))
        {
            endIndex = text.indexOf (commentStartExpression, endIndex + 3, &startMatch);
        }

        /* if there's a comment end ... */
        if (endIndex >= 0)
        {
            /* ... clear the comment format from there to reformat
               because a single-line comment may have changed now */
            int badIndex = endIndex + startMatch.capturedLength();
            if (format (badIndex) == commentFormat || format (badIndex) == urlFormat)
                setFormat (badIndex, text.length() - badIndex, mainFormat);
            singleLineComment (text, badIndex);
        }

        int quoteLength;
        if (endIndex == -1)
        {
            setCurrentBlockState (quote);
            quoteLength = text.length() - index;
        }
        else
            quoteLength = endIndex - index
                          + startMatch.capturedLength(); // 3
        setFormat (index, quoteLength, commentFormat);

        /* format urls and email addresses inside the comment */
        QString str = text.mid (index, quoteLength);
        int pIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((pIndex = str.indexOf (urlPattern, pIndex, &urlMatch)) > -1)
        {
            setFormat (pIndex + index, urlMatch.capturedLength(), urlFormat);
            pIndex += urlMatch.capturedLength();
        }
        /* format note patterns too */
        pIndex = 0;
        while ((pIndex = str.indexOf (notePattern, pIndex, &urlMatch)) > -1)
        {
            if (format (pIndex + index) != urlFormat)
                setFormat (pIndex + index, urlMatch.capturedLength(), noteFormat);
            pIndex += urlMatch.capturedLength();
        }

        /* the next quote may be different */
        commentStartExpression.setPattern ("\"\"\"|\'\'\'");
        index = text.indexOf (commentStartExpression, index + quoteLength);
        QTextCharFormat fi = format (index);
        while ((index > 0 && isQuoted (text, index - 1))
               || (index == 0 && (prevState == doubleQuoteState || prevState == singleQuoteState))
               || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat)
        {
            index = text.indexOf (commentStartExpression, index + 3);
            fi = format (index);
        }
        if (format (index) == commentFormat || format (index) == urlFormat)
            return;
    }
}
/*************************/
void Highlighter::singleLineComment (const QString &text, const int start)
{
    for (const HighlightingRule &rule : qAsConst (highlightingRules))
    {
        if (rule.format == commentFormat)
        {
            int startIndex = qMax (start, 0);
            if (previousBlockState() == nextLineCommentState)
                startIndex = 0;
            else
            {
                startIndex = text.indexOf (rule.pattern, startIndex);
                /* skip quoted comments (and, automatically, those inside multiline python comments) */
                while (startIndex > -1
                           /* check quote formats (only for multiLineComment()) */
                       && (format (startIndex) == quoteFormat
                           || format (startIndex) == altQuoteFormat
                           || format (startIndex) == urlInsideQuoteFormat
                           /* check whether the comment sign is quoted or inside regex */
                           || isQuoted (text, startIndex, false, qMax (start, 0)) || isInsideRegex (text, startIndex)
                           /* with troff and LaTeX, the comment sign may be escaped */
                           || ((progLan == "troff" || progLan == "LaTeX")
                               && isEscapedChar(text, startIndex))))
                {
                    startIndex = text.indexOf (rule.pattern, startIndex + 1);
                }
            }
            if (startIndex > -1)
            {
                int l = text.length();
                setFormat (startIndex, l - startIndex, commentFormat);

                /* also format urls and email addresses inside the comment */
                QString str = text.mid (startIndex, l - startIndex);
                int pIndex = 0;
                QRegularExpressionMatch urlMatch;
                while ((pIndex = str.indexOf (urlPattern, pIndex, &urlMatch)) > -1)
                {
                    setFormat (pIndex + startIndex, urlMatch.capturedLength(), urlFormat);
                    pIndex += urlMatch.capturedLength();
                }
                /* format note patterns too */
                pIndex = 0;
                while ((pIndex = str.indexOf (notePattern, pIndex, &urlMatch)) > -1)
                {
                    if (format (pIndex + startIndex) != urlFormat)
                        setFormat (pIndex + startIndex, urlMatch.capturedLength(), noteFormat);
                    pIndex += urlMatch.capturedLength();
                }

                if (progLan == "javascript" || progLan == "qml")
                {
                    /* see NOTE of isEscapedRegex() and also the end of multiLineRegex() */
                    setCurrentBlockState (regexExtraState);
                }
                else if ((progLan == "c" || progLan == "cpp")
                         && text.endsWith (QLatin1Char('\\')))
                {
                    /* Take care of next-line comments with languages, for which
                       no highlighting function is called after singleLineComment()
                       and before the main formatting in highlightBlock()
                       (only c and c++ for now). */
                    setCurrentBlockState (nextLineCommentState);
                }
            }
            break;
        }
    }
}
/*************************/
bool Highlighter::multiLineComment (const QString &text,
                                    const int index,
                                    const QRegularExpression &commentStartExp, const QRegularExpression &commentEndExp,
                                    const int commState,
                                    const QTextCharFormat &comFormat)
{
    if (index < 0) return false;
    int prevState = previousBlockState();
    if (prevState == nextLineCommentState)
        return false;  // was processed by singleLineComment()

    /* CSS can have huge lines, which will take
       a lot of CPU time if they're formatted completely. */
    //bool hugeText = ((progLan == "css" || progLan == "scss" ) && text.length() > 50000);

    bool rehighlightNextBlock = false;
    int startIndex = index;

    QRegularExpressionMatch startMatch;
    QRegularExpressionMatch endMatch;

    if (startIndex > 0
        || (prevState != commState
            && prevState != commentInCssBlockState
            && prevState != commentInCssValueState))
    {
        startIndex = text.indexOf (commentStartExp, startIndex, &startMatch);
        /* skip quotations (usually all formatted to this point) and regexes */
        QTextCharFormat fi = format (startIndex);
        while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || isInsideRegex (text, startIndex))
        {
            startIndex = text.indexOf (commentStartExp, startIndex + 1, &startMatch);
            fi = format (startIndex);
        }
        /* skip single-line comments */
        if (format (startIndex) == commentFormat || format (startIndex) == urlFormat)
            startIndex = -1;

        /* special handling for markdown */
        if (progLan == "markdown" && startIndex > 0)
        {
            int extraBlockIndentation = 0;
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid())
            {
                if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                    extraBlockIndentation = prevData->labelInfo().length();
            }
            if (text.indexOf (QRegularExpression ("^#+\\s+.*"), 0) == 0
                || text.indexOf (QRegularExpression (QStringLiteral ("^( {")
                                                     + QString::number (4 + extraBlockIndentation)
                                                     + QStringLiteral (",}|\\s*\\t+\\s*).*")), 0) == 0)
            {
                return false; // no comment start sign inside headings or code blocks
            }
            /* no comment start sign inside footnotes, images or links */
            QRegularExpressionMatch mMatch;
            static const QRegularExpression mExp ("\\[\\^[^\\]]+\\]"
                                                  "|"
                                                  "\\!\\[[^\\]\\^]*\\]\\s*"
                                                  "(\\(\\s*[^\\)\\(\\s]+(\\s+\\\".*\\\")*\\s*\\)|\\s*\\[[^\\]]*\\])"
                                                  "|"
                                                  "\\[[^\\]\\^]*\\]\\s*\\[[^\\]\\s]*\\]"
                                                  "|"
                                                  "\\[[^\\]\\^]*\\]\\s*\\(\\s*[^\\)\\(\\s]+(\\s+\\\".*\\\")*\\s*\\)"
                                                  "|"
                                                  "\\[[^\\]\\^]*\\]:\\s+\\s*[^\\)\\(\\s]+(\\s+\\\".*\\\")*");
            int mStart = text.indexOf (mExp, 0, &mMatch);
            while (mStart >= 0 && mStart < startIndex)
            {
                int mEnd = mStart + mMatch.capturedLength();
                if (startIndex < mEnd)
                {
                    startIndex = text.indexOf (commentStartExp, mEnd, &startMatch);
                    if (startIndex == -1) return false;
                }
                mStart = text.indexOf (mExp, mEnd, &mMatch);
            }
        }
    }

    while (startIndex >= 0)
    {
        int endIndex;
        /* when the comment start is in the prvious line
           and the search for the comment end has just begun... */
        if ((prevState == commState
             || prevState == commentInCssBlockState
             || prevState == commentInCssValueState)
            && startIndex == 0)
            /* ... search for the comment end from the line start */
            endIndex = text.indexOf (commentEndExp, 0, &endMatch);
        else
            endIndex = text.indexOf (commentEndExp,
                                     startIndex + startMatch.capturedLength(),
                                     &endMatch);

        /* skip quotations */
        QTextCharFormat fi = format (endIndex);
        if (progLan != "fountain") // in Fountain, altQuoteFormat is used for notes
        { // FIXME: Is this really needed? Commented quotes are skipped in formatting multi-line quotes.
            while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat)
            {
                endIndex = text.indexOf (commentEndExp, endIndex + 1, &endMatch);
                fi = format (endIndex);
            }
        }

        if (/*!hugeText && */endIndex >= 0 && progLan != "xml" && progLan != "html")
        {
            /* because multiline commnets weren't taken into account in
               singleLineComment(), that method should be used here again */
            int badIndex = endIndex + endMatch.capturedLength();
            bool hadSingleLineComment = false;
            int i = 0;
            for (i = badIndex; i < text.length(); ++i)
            {
                if (format (i) == commentFormat || format (i) == urlFormat)
                {
                    setFormat (i, text.length() - i, mainFormat);
                    hadSingleLineComment = true;
                    break;
                }
            }
            singleLineComment (text, badIndex);
            /* because the previous single-line comment may have been
               removed now, quotes should be checked again from its start */
            if (hadSingleLineComment && multilineQuote_)
                rehighlightNextBlock = multiLineQuote (text, i, commState);
        }

        int commentLength;
        if (endIndex == -1)
        {
            if (currentBlockState() != cssBlockState
                && currentBlockState() != cssValueState)
            {
                setCurrentBlockState (commState);
            }
            else
            { // CSS
                if (currentBlockState() == cssValueState)
                    setCurrentBlockState (commentInCssValueState);
                else
                    setCurrentBlockState (commentInCssBlockState);
            }
            commentLength = text.length() - startIndex;
        }
        else
            commentLength = endIndex - startIndex
                            + endMatch.capturedLength();
        //if (!hugeText)
        //{
            setFormat (startIndex, commentLength, comFormat);

            /* format urls and email addresses inside the comment */
            QString str = text.mid (startIndex, commentLength);
            int pIndex = 0;
            QRegularExpressionMatch urlMatch;
            while ((pIndex = str.indexOf (urlPattern, pIndex, &urlMatch)) > -1)
            {
                setFormat (pIndex + startIndex, urlMatch.capturedLength(), urlFormat);
                pIndex += urlMatch.capturedLength();
            }
            /* format note patterns too */
            pIndex = 0;
            while ((pIndex = str.indexOf (notePattern, pIndex, &urlMatch)) > -1)
            {
                if (format (pIndex + startIndex) != urlFormat)
                    setFormat (pIndex + startIndex, urlMatch.capturedLength(), noteFormat);
                pIndex += urlMatch.capturedLength();
            }
        //}

        startIndex = text.indexOf (commentStartExp, startIndex + commentLength, &startMatch);

        /* skip single-line comments and quotations again */
        fi = format (startIndex);
        while (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || isInsideRegex (text, startIndex))
        {
            startIndex = text.indexOf (commentStartExp, startIndex + 1, &startMatch);
            fi = format (startIndex);
        }
        if (format (startIndex) == commentFormat || format (startIndex) == urlFormat)
            startIndex = -1;
    }

    /* reset the block state if this line created a next-line comment
       whose starting single-line comment sign is commented out now */
    if (currentBlockState() == nextLineCommentState
        && format (text.size() - 1) != commentFormat && format (text.size() - 1) != urlFormat)
    {
        setCurrentBlockState (0);
    }

    return rehighlightNextBlock;
}
/*************************/
// Handles escaped backslashes too.
bool Highlighter::textEndsWithBackSlash (const QString &text)
{
    QString str = text;
    int n = 0;
    while (!str.isEmpty() && str.endsWith ("\\"))
    {
        str.truncate (str.size() - 1);
        ++n;
    }
    return (n % 2 != 0);
}
/*************************/
// This also covers single-line quotes. It comes after single-line comments
// and before multi-line ones are formatted. "comState" is the comment state,
// whose default is "commentState" but may be different for some languages.
// Sometimes (with multi-language docs), formatting should be started from "start".
bool Highlighter::multiLineQuote (const QString &text, const int start, int comState)
{
    if (progLan == "perl" || progLan == "ruby")
    {
        multiLinePerlQuote (text);
        return false;
    }
    if (progLan == "javascript" || progLan == "qml")
    {
        multiLineJSlQuote (text, start, comState);
        return false;
    }
//--------------------
    /* these are only for C++11 raw string literals,
       whose pattern is R"(\bR"([^(]*)\(.*(?=\)\1"))" */
    bool rehighlightNextBlock = false;
    QString delimStr;
    QTextCharFormat rFormat;
    TextBlockData *cppData = nullptr;
    if (progLan == "cpp")
    {
        rFormat = quoteFormat;
        rFormat.setFontWeight (QFont::Bold);
        cppData = static_cast<TextBlockData *>(currentBlock().userData());
        QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid())
        {
            if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
                delimStr = prevData->labelInfo();
        }
    }
//--------------------

    int index = start;
    QRegularExpressionMatch quoteMatch;
    QRegularExpression quoteExpression;
    if (mixedQuotes_)
        quoteExpression = mixedQuoteMark;
    else
        quoteExpression = quoteMark;
    int quote = doubleQuoteState;

    /* find the start quote */
    int prevState = previousBlockState();
    if ((prevState != doubleQuoteState
         && prevState != singleQuoteState)
        || index > 0)
    {
        index = text.indexOf (quoteExpression, index);
        /* skip escaped start quotes and all comments */
        while (isEscapedQuote (text, index, true)
               || isInsideRegex (text, index)
               || isMLCommented (text, index, comState))
        {
            index = text.indexOf (quoteExpression, index + 1);
        }
        while (format (index) == commentFormat || format (index) == urlFormat) // single-line and Python
            index = text.indexOf (quoteExpression, index + 1);

        /* with markdown, "`" should be distinguished from "```" for code bocks (-> highlightBlock()) */
        if (index >= 0 && progLan == "markdown")
        {
            while (index >= 0 && index == text.indexOf (QRegularExpression ("```(?!`)"), index))
            {
                index = text.indexOf (quoteExpression, index + 3);
                while (isEscapedQuote (text, index, true)
                       || isMLCommented (text, index, comState))
                {
                    index = text.indexOf (quoteExpression, index + 1);
                }
                while (format (index) == commentFormat || format (index) == urlFormat)
                    index = text.indexOf (quoteExpression, index + 1);
            }
        }

        /* if the start quote is found... */
        if (index >= 0)
        {
            if (mixedQuotes_)
            {
                /* ... distinguish between double and single quotes */
                if (text.at (index) == quoteMark.pattern().at (0))
                {
                    if (progLan == "cpp" && index > start)
                    {
                        QRegularExpressionMatch cppMatch;
                        if (text.at (index - 1) == 'R' && index - 1 == text.indexOf (QRegularExpression (R"(\bR"([^(]*)\()"), index - 1, &cppMatch))
                        {
                            delimStr = ")" + cppMatch.captured (1);
                            setFormat (index - 1, 1, rFormat);
                        }
                    }
                    quoteExpression = quoteMark;
                    quote = doubleQuoteState;
                }
                else
                {
                    quoteExpression = singleQuoteMark;
                    quote = singleQuoteState;
                }
            }
        }
    }
    else // but if we're inside a quotation...
    {
        /* ... distinguish between the two quote kinds
           by checking the previous line */
        if (mixedQuotes_)
        {
            quote = prevState;
            if (quote == doubleQuoteState)
                quoteExpression = quoteMark;
            else
                quoteExpression = singleQuoteMark;
        }
    }

    while (index >= 0)
    {
        /* if the search is continued... */
        if (quoteExpression == mixedQuoteMark)
        {
            /* ... distinguish between double and single quotes
               again because the quote mark may have changed */
            if (text.at (index) == quoteMark.pattern().at (0))
            {
                if (progLan == "cpp" && index > start)
                {
                    QRegularExpressionMatch cppMatch;
                    if (text.at (index - 1) == 'R'  && index - 1 == text.indexOf (QRegularExpression (R"(\bR"([^(]*)\()"), index - 1, &cppMatch))
                    {
                        delimStr = ")" + cppMatch.captured (1);
                        setFormat (index - 1, 1, rFormat);
                    }
                }
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else
            {
                quoteExpression = singleQuoteMark;
                quote = singleQuoteState;
            }
        }

        int endIndex;
        /* if there's no start quote ... */
        if (index == 0
            && (prevState == doubleQuoteState || prevState == singleQuoteState))
        {
            /* ... search for the end quote from the line start */
            endIndex = text.indexOf (quoteExpression, 0, &quoteMatch);
        }
        else // otherwise, search from the start quote
            endIndex = text.indexOf (quoteExpression, index + 1, &quoteMatch);

        if (delimStr.isEmpty())
        { // check if the quote is escaped
            while (isEscapedQuote (text, endIndex, false))
                endIndex = text.indexOf (quoteExpression, endIndex + 1, &quoteMatch);
        }
        else
        { // check if the quote is inside a C++11 raw string literal
            while (endIndex > -1
                   && (endIndex - delimStr.length() < start
                       || text.mid (endIndex - delimStr.length(), delimStr.length()) != delimStr))
            {
                endIndex = text.indexOf (quoteExpression, endIndex + 1, &quoteMatch);
            }
        }

        bool isQuotation = true;
        if (endIndex == -1)
        {
            if (progLan == "c" || progLan == "cpp")
            {
                /* In c and cpp, multiline double quotes need backslash and
                   there's no multiline single quote. Moreover, In C++11,
                   there can be multiline raw string literals. */
                if (quoteExpression == singleQuoteMark
                    || (quoteExpression == quoteMark && delimStr.isEmpty() && !textEndsWithBackSlash (text)))
                {
                    endIndex = text.length();
                }
            }
            else if (progLan == "markdown")
            { // this is the main difference of a markdown inline code from a single-line quote
                isQuotation = false;
            }
            else if (progLan == "go")
            {
                if (quoteExpression == quoteMark) // no multiline double quote
                    endIndex = text.length();
            }
        }

        int quoteLength;
        if (endIndex == -1)
        {
            if (isQuotation)
                setCurrentBlockState (quote);
            quoteLength = text.length() - index;

            /* set the delimiter string for C++11 */
            if (cppData && !delimStr.isEmpty())
            {
                /* with a multiline C++11 raw string literal, if the delimiter is changed
                   but the state of the current block isn't changed, the next block won't
                   be highlighted automatically, so it should be rehighlighted forcefully */
                if (cppData->lastState() == quote)
                {
                    QTextBlock nextBlock = currentBlock().next();
                    if (nextBlock.isValid())
                    {
                        if (TextBlockData *nextData = static_cast<TextBlockData *>(nextBlock.userData()))
                        {
                            if (nextData->labelInfo() != delimStr)
                                rehighlightNextBlock = true;
                        }
                    }
                }
                cppData->insertInfo (delimStr);
                setCurrentBlockUserData (cppData);
            }
        }
        else
            quoteLength = endIndex - index
                          + quoteMatch.capturedLength(); // 1 or 0 (open quotation without ending backslash)
        if (isQuotation)
        {
            setFormat (index, quoteLength, quoteExpression == quoteMark ? quoteFormat
                                                                        : altQuoteFormat);
            /* URLs should be formatted in a different way inside quotes because,
               otherwise, there would be no difference between URLs inside quotes and
               those inside comments and so, they couldn't be escaped correctly when needed. */
            QString str = text.mid (index, quoteLength);
            int urlIndex = 0;
            QRegularExpressionMatch urlMatch;
            while ((urlIndex = str.indexOf (urlPattern, urlIndex, &urlMatch)) > -1)
            {
                 setFormat (urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
                 urlIndex += urlMatch.capturedLength();
            }
        }

        /* the next quote may be different */
        if (mixedQuotes_)
            quoteExpression = mixedQuoteMark;
        index = text.indexOf (quoteExpression, index + quoteLength);

        /* skip escaped start quotes and all comments */
        while (isEscapedQuote (text, index, true)
               || isInsideRegex (text, index)
               || isMLCommented (text, index, comState, endIndex + 1))
        {
            index = text.indexOf (quoteExpression, index + 1);
        }
        while (format (index) == commentFormat || format (index) == urlFormat)
            index = text.indexOf (quoteExpression, index + 1);

        if (index >= 0 && progLan == "markdown")
        {
            while (index >= 0 && index == text.indexOf (QRegularExpression ("```(?!`)"), index))
            {
                index = text.indexOf (quoteExpression, index + 3);
                while (isEscapedQuote (text, index, true)
                       || isMLCommented (text, index, comState))
                {
                    index = text.indexOf (quoteExpression, index + 1);
                }
                while (format (index) == commentFormat || format (index) == urlFormat)
                    index = text.indexOf (quoteExpression, index + 1);
            }
        }
        delimStr.clear();
    }
    return rehighlightNextBlock;
}
/*************************/
// Perl's (and Ruby's) multiline quote highlighting comes here to support backquotes.
// Also see isPerlQuoted().
void Highlighter::multiLinePerlQuote (const QString &text)
{
    int index = 0;
    QRegularExpressionMatch quoteMatch;
    QRegularExpression quoteExpression = mixedQuoteBackquote;
    int quote = doubleQuoteState;

    /* find the start quote */
    int prevState = previousBlockState();
    if (prevState != doubleQuoteState && prevState != singleQuoteState)
    {
        index = text.indexOf (quoteExpression, index);
        /* skip escaped start quotes and all comments */
        while (isEscapedQuote (text, index, true) || isInsideRegex (text, index))
            index = text.indexOf (quoteExpression, index + 1);
        while (format (index) == commentFormat || format (index) == urlFormat)
            index = text.indexOf (quoteExpression, index + 1);

        /* if the start quote is found... */
        if (index >= 0)
        {
            /* ... distinguish between the three kinds of quote */
            if (text.at (index) == quoteMark.pattern().at (0))
            {
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else
            {
                if (text.at (index) == '\'')
                    quoteExpression = singleQuoteMark;
                else
                    quoteExpression = backQuote;
                quote = singleQuoteState;
            }
        }
    }
    else // but if we're inside a quotation...
    {
        /* ... distinguish between the three kinds of quote */
        quote = prevState;
        if (quote == doubleQuoteState)
            quoteExpression = quoteMark;
        else
        {
            bool backquoted (false);
            QTextBlock prevBlock = currentBlock().previous();
            if (prevBlock.isValid())
            {
                TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData());
                if (prevData && prevData->getProperty())
                    backquoted = true;
            }
            if (backquoted)
                quoteExpression = backQuote;
            else
                quoteExpression = singleQuoteMark;
        }
    }

    while (index >= 0)
    {
        /* if the search is continued... */
        if (quoteExpression == mixedQuoteBackquote)
        {
            /* ... distinguish between the three kinds of quote
               again because the quote mark may have changed */
            if (text.at (index) == quoteMark.pattern().at (0))
            {
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else
            {
                if (text.at (index) == '\'')
                    quoteExpression = singleQuoteMark;
                else
                    quoteExpression = backQuote;
                quote = singleQuoteState;
            }
        }

        int endIndex;
        /* if there's no start quote ... */
        if (index == 0
            && (prevState == doubleQuoteState || prevState == singleQuoteState))
        {
            /* ... search for the end quote from the line start */
            endIndex = text.indexOf (quoteExpression, 0, &quoteMatch);
        }
        else // otherwise, search from the start quote
            endIndex = text.indexOf (quoteExpression, index + 1, &quoteMatch);

        // check if the quote is escaped
        while (isEscapedQuote (text, endIndex, false))
            endIndex = text.indexOf (quoteExpression, endIndex + 1, &quoteMatch);

        int quoteLength;
        if (endIndex == -1)
        {
            setCurrentBlockState (quote);
            if (quoteExpression == backQuote)
            {
                if (TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData()))
                    data->setProperty (true);
                /* NOTE: The next block will be rehighlighted at highlightBlock()
                         (-> multiLineRegex (text, 0);) if the property is changed. */
            }
            quoteLength = text.length() - index;
        }
        else
            quoteLength = endIndex - index
                          + quoteMatch.capturedLength(); // 1
        setFormat (index, quoteLength, quoteExpression == quoteMark ? quoteFormat
                                                                    : altQuoteFormat);
        QString str = text.mid (index, quoteLength);
        int urlIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((urlIndex = str.indexOf (urlPattern, urlIndex, &urlMatch)) > -1)
        {
             setFormat (urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
             urlIndex += urlMatch.capturedLength();
        }

        /* the next quote may be different */
        quoteExpression = mixedQuoteBackquote;
        index = text.indexOf (quoteExpression, index + quoteLength);

        /* skip escaped start quotes and all comments */
        while (isEscapedQuote (text, index, true) || isInsideRegex (text, index))
            index = text.indexOf (quoteExpression, index + 1);
        while (format (index) == commentFormat || format (index) == urlFormat)
            index = text.indexOf (quoteExpression, index + 1);
    }
}
/*************************/
// JS multiline quote highlighting comes here to support backquotes (template literals).
// Also see isJSQuoted().
void Highlighter::multiLineJSlQuote (const QString &text, const int start, int comState)
{
    int index = start;
    QRegularExpressionMatch quoteMatch;
    QRegularExpression quoteExpression = mixedQuoteBackquote;
    int quote = doubleQuoteState;

    /* find the start quote */
    int prevState = previousBlockState();
    if ((prevState != doubleQuoteState
         && prevState != singleQuoteState
         && prevState != JS_templateLiteralState)
        || index > 0)
    {
        index = text.indexOf (quoteExpression, index);
        /* skip escaped start quotes and all comments */
        while (isEscapedQuote (text, index, true)
               || isInsideRegex (text, index)
               || isMLCommented (text, index, comState))
        {
            index = text.indexOf (quoteExpression, index + 1);
        }
        while (format (index) == commentFormat || format (index) == urlFormat) // single-line
            index = text.indexOf (quoteExpression, index + 1);

        /* if the start quote is found... */
        if (index >= 0)
        {
            /* ... distinguish between double and single quotes */
            if (text.at (index) == quoteMark.pattern().at (0))
            {
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else if (text.at (index) == '\'')
            {
                quoteExpression = singleQuoteMark;
                quote = singleQuoteState;
            }
            else
            {
                quoteExpression = backQuote;
                quote = JS_templateLiteralState;
            }
        }
    }
    else // but if we're inside a quotation...
    {
        /* ... distinguish between the two quote kinds
           by checking the previous line */
        quote = prevState;
        if (quote == doubleQuoteState)
            quoteExpression = quoteMark;
        else if (quote == singleQuoteState)
            quoteExpression = singleQuoteMark;
        else
            quoteExpression = backQuote;
    }

    while (index >= 0)
    {
        /* if the search is continued... */
        if (quoteExpression == mixedQuoteBackquote)
        {
            /* ... distinguish between double and single quotes
               again because the quote mark may have changed */
            if (text.at (index) == quoteMark.pattern().at (0))
            {
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else if (text.at (index) == '\'')
            {
                quoteExpression = singleQuoteMark;
                quote = singleQuoteState;
            }
            else
            {
                quoteExpression = backQuote;
                quote = JS_templateLiteralState;
            }
        }

        int endIndex;
        /* if there's no start quote ... */
        if (index == 0
            && (prevState == doubleQuoteState || prevState == singleQuoteState
                || prevState == JS_templateLiteralState))
        {
            /* ... search for the end quote from the line start */
            endIndex = text.indexOf (quoteExpression, 0, &quoteMatch);
        }
        else // otherwise, search from the start quote
            endIndex = text.indexOf (quoteExpression, index + 1, &quoteMatch);

        /* check if the quote is escaped */
        while (isEscapedQuote (text, endIndex, false))
            endIndex = text.indexOf (quoteExpression, endIndex + 1, &quoteMatch);

        int quoteLength;
        if (endIndex == -1)
        {
            /* In JS, multiline double and single quotes need backslash. */
            if ((quoteExpression == singleQuoteMark || quoteExpression == quoteMark)
                && !textEndsWithBackSlash (text))
            { // see NOTE of isEscapedRegex() and also the end of multiLineRegex()
                setCurrentBlockState (regexExtraState);
            }
            else
                setCurrentBlockState (quote);
            quoteLength = text.length() - index;
        }
        else
            quoteLength = endIndex - index
                          + quoteMatch.capturedLength(); // 1

        setFormat (index, quoteLength, quoteExpression == quoteMark ? quoteFormat
                                                                    : altQuoteFormat);

        QString str = text.mid (index, quoteLength);
        int urlIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((urlIndex = str.indexOf (urlPattern, urlIndex, &urlMatch)) > -1)
        {
            setFormat (urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
            urlIndex += urlMatch.capturedLength();
        }

        /* the next quote may be different */
        quoteExpression = mixedQuoteBackquote;
        index = text.indexOf (quoteExpression, index + quoteLength);

        /* skip escaped start quotes and all comments */
        while (isEscapedQuote (text, index, true)
               || isInsideRegex (text, index)
               || isMLCommented (text, index, comState, endIndex + 1))
        {
            index = text.indexOf (quoteExpression, index + 1);
        }
        while (format (index) == commentFormat || format (index) == urlFormat)
            index = text.indexOf (quoteExpression, index + 1);
    }
}
/*************************/
// Generalized form of setFormat(), where "oldFormat" shouldn't be reformatted.
void Highlighter::setFormatWithoutOverwrite (int start,
                                             int count,
                                             const QTextCharFormat &newFormat,
                                             const QTextCharFormat &oldFormat)
{
    int index = start; // always >= 0
    int indx;
    while (index < start + count)
    {
        QTextCharFormat fi = format (index);
        while (index < start + count
               && (fi == oldFormat
                   /* skip comments and quotes */
                   || fi == commentFormat || fi == urlFormat
                   || fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat))
        {
            ++ index;
            fi = format (index);
        }
        if (index < start + count)
        {
            indx = index;
            fi = format (indx);
            while (indx < start + count
                   && fi != oldFormat
                   && fi != commentFormat && fi != urlFormat
                   && fi != quoteFormat && fi != altQuoteFormat && fi != urlInsideQuoteFormat)
            {
                ++ indx;
                fi = format (indx);
            }
            setFormat (index, indx - index , newFormat);
            index = indx;
        }
    }
}
/*************************/
// XML quotes are handled as multiline quotes for possible XML doc mistakes
// to be seen easily and also because the double quote can be written as "&quot;".
// This comes after values are formatted.
void Highlighter::xmlQuotes (const QString &text)
{
    int index = 0;
    static const QRegularExpression xmlMixedQuote ("\"|&quot;|\'");
    static const QRegularExpression doubleQuote ("\"|&quot;");
    static const QRegularExpression virtualQuote ("&quot;");
    QRegularExpressionMatch quoteMatch;
    QRegularExpression quoteExpression = xmlMixedQuote;
    int quote = doubleQuoteState;

    /* find the start quote */
    int prevState = previousBlockState();
    if (prevState != doubleQuoteState
        && prevState != singleQuoteState)
    {
        index = text.indexOf (quoteExpression);
        /* skip all comments and all values (that are formatted by multiLineComment()) */
        while (isMLCommented (text, index, commentState)
               || format (index) == neutralFormat)
        {
            index = text.indexOf (quoteExpression, index + 1);
        }

        /* if the start quote is found... */
        if (index >= 0)
        {
            /* ... distinguish between double, virtual and single quotes */
            if (text.at (index) == '\"')
            {
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else if (text.mid (index, 6) == "&quot;")
            {
                quoteExpression = virtualQuote;
                quote = doubleQuoteState;
            }
            else
            {
                quoteExpression = singleQuoteMark;
                quote = singleQuoteState;
            }
        }
    }
    else // but if we're inside a quotation...
    {
        /* ... distinguish between the two quote kinds
           by checking the previous line */
        quote = prevState;
        if (quote == doubleQuoteState)
            quoteExpression = doubleQuote;
        else
            quoteExpression = singleQuoteMark;
    }

    while (index >= 0)
    {
        /* if the search is continued... */
        if (quoteExpression == xmlMixedQuote)
        {
            /* ... distinguish between double, virtual and single
               quotes again because the quote mark may have changed */
            if (text.at (index) == '\"')
            {
                quoteExpression = quoteMark;
                quote = doubleQuoteState;
            }
            else if (text.mid (index, 6) == "&quot;")
            {
                quoteExpression = virtualQuote;
                quote = doubleQuoteState;
            }
            else
            {
                quoteExpression = singleQuoteMark;
                quote = singleQuoteState;
            }
        }

        /* search for the end quote from the start quote */
        int endIndex = text.indexOf (quoteExpression, index + 1, &quoteMatch);

        /* but if there's no start quote ... */
        if (index == 0
            && (prevState == doubleQuoteState || prevState == singleQuoteState))
        {
            /* ... search for the end quote from the line start */
            endIndex = text.indexOf (quoteExpression, 0, &quoteMatch);
        }

        if (endIndex == -1)
        {
            /* tolerate a mismatch between `"` and `&quot;` as far as possible
               but show the error by formatting `>` or `&gt;` */
            QRegularExpressionMatch match;
            int closing = text.indexOf (xmlGt, index, &match);
            if (closing > -1)
                endIndex = closing + match.capturedLength();
        }

        int quoteLength;
        if (endIndex == -1)
        {
            setCurrentBlockState (quote);
            quoteLength = text.length() - index;
        }
        else
            quoteLength = endIndex - index
                          + quoteMatch.capturedLength();
        setFormat (index, quoteLength,
                   quoteExpression == quoteMark
                   || quoteExpression == virtualQuote ? quoteFormat : altQuoteFormat);

        QString str = text.mid (index, quoteLength);
        int urlIndex = 0;
        QRegularExpressionMatch urlMatch;
        while ((urlIndex = str.indexOf (urlPattern, urlIndex, &urlMatch)) > -1)
        {
             setFormat (urlIndex + index, urlMatch.capturedLength(), urlInsideQuoteFormat);
             urlIndex += urlMatch.capturedLength();
        }

        /* the next quote may be different */
        quoteExpression = xmlMixedQuote;
        index = text.indexOf (quoteExpression, index + quoteLength);

        while (isMLCommented (text, index, commentState, endIndex + quoteMatch.capturedLength())
               || format (index) == neutralFormat)
        {
            index = text.indexOf (quoteExpression, index + 1);
        }
    }
}
/*************************/
// Check if the current block is inside a "here document" and format it accordingly.
// (Open quotes aren't taken into account when they happen after the start delimiter.)
bool Highlighter::isHereDocument (const QString &text)
{
    /*if (progLan != "sh" && progLan != "makefile" && progLan != "cmake"
        && progLan != "perl" && progLan != "ruby")
    {
        return false;
        // "<<([A-Za-z0-9_]+)|<<(\'[A-Za-z0-9_]+\')|<<(\"[A-Za-z0-9_]+\")"
    }*/

    QTextBlock prevBlock = currentBlock().previous();
    int prevState = previousBlockState();

    QTextCharFormat blockFormat;
    blockFormat.setForeground (Violet);
    QTextCharFormat delimFormat = blockFormat;
    delimFormat.setFontWeight (QFont::Bold);
    QString delimStr;

    /* format the start delimiter */
    if (!prevBlock.isValid()
        || (prevState >= 0 && prevState < endState))
    {
        int pos = 0;
        QRegularExpressionMatch match;
        while ((pos = text.indexOf (hereDocDelimiter, pos, &match)) >= 0
               && (isQuoted (text, pos, progLan == "sh") // escaping start double quote before "$("
                   || (progLan == "perl" && isInsideRegex (text, pos))))

        {
            pos += match.capturedLength();
        }
        if (pos >= 0)
        {
            int insideCommentPos;
            if (progLan == "sh")
            {
                static const QRegularExpression commentSH ("^#.*|\\s+#.*");
                insideCommentPos = text.indexOf (commentSH);
            }
            else
            {
                static const QRegularExpression commentOthers ("#.*");
                insideCommentPos = text.indexOf (commentOthers);
            }
            if (insideCommentPos == -1 || pos < insideCommentPos
                || isQuoted (text, insideCommentPos, progLan == "sh")
                || (progLan == "perl" && isInsideRegex (text, insideCommentPos)))
            { // the delimiter isn't (single-)commented out
                int i = 1;
                while ((delimStr = match.captured (i)).isEmpty() && i <= 3)
                {
                    ++i;
                    delimStr = match.captured (i);
                }

                if (progLan == "perl")
                {
                    if (delimStr.contains ('`')) // Perl's delimiter can have backquotes
                        delimStr = delimStr.split ('`').at (1);
                }

                if (!delimStr.isEmpty())
                {
                    /* remove quotes */
                    if (delimStr.contains ('\''))
                        delimStr = delimStr.split ('\'').at (1);
                    if (delimStr.contains ('\"'))
                        delimStr = delimStr.split ('\"').at (1);
                    /* remove the start backslash if it exists */
                    if (QString (delimStr.at (0)) == "\\")
                        delimStr = delimStr.remove (0, 1);
                }

                if (!delimStr.isEmpty())
                {
                    int n = static_cast<int>(qHash (delimStr));
                    int state = 2 * (n + (n >= 0 ? endState/2 + 1 : 0)); // always an even number but maybe negative
                    if (progLan == "sh")
                    {
                        if (isQuoted (text, pos, false))
                        { // to know whether a double quote is added/removed before "$(" in the current line
                            state > 0 ? state += 2 : state -= 2;
                        }
                        if (prevState == doubleQuoteState || prevState == SH_DoubleQuoteState)
                        { // to know whether a double quote is added/removed before "$(" in a previous line
                            state > 0 ? state += 4 : state -= 4; // not 2 -> not to be canceled above
                        }
                    }
                    setCurrentBlockState (state);
                    setFormat (text.indexOf (delimStr, pos),
                               delimStr.length(),
                               delimFormat);

                    TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData());
                    if (!data) return false;
                    data->insertInfo (delimStr);
                    setCurrentBlockUserData (data);

                    return false;
                }
            }
        }
    }

    if (prevState >= endState || prevState < -1)
    {
        TextBlockData *prevData = nullptr;
        if (prevBlock.isValid())
            prevData = static_cast<TextBlockData *>(prevBlock.userData());
        if (!prevData) return false;

        delimStr = prevData->labelInfo();
        int l = 0;
        if (progLan == "perl" || progLan == "ruby")
        {
            QRegularExpressionMatch rMatch;
            /* the terminating string must appear on a line by itself */
            QRegularExpression r ("\\s*" + delimStr + "(?=\\s*$)");
            if (text.indexOf (r, 0, &rMatch) == 0)
                l = rMatch.capturedLength();
        }
        else if (text == delimStr
                 || (text.startsWith (delimStr)
                     && text.indexOf (QRegularExpression ("\\W+")) == delimStr.length()))
        {
            l = delimStr.length();
        }
        if (l > 0)
        {
            /* format the end delimiter */
            setFormat (0, l, delimFormat);
            return false;
        }
        else
        {
            /* format the contents */
            TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData());
            if (!data) return false;
            data->insertInfo (delimStr);
            /* inherit the previous data property and open nests */
            if (bool p = prevData->getProperty())
                data->setProperty (p);
            int N = prevData->openNests();
            if (N > 0)
            {
                data->insertNestInfo (N);
                QSet<int> Q = prevData->openQuotes();
                if (!Q.isEmpty())
                    data->insertOpenQuotes (Q);
            }
            setCurrentBlockUserData (data);
            if (prevState % 2 == 0)
                setCurrentBlockState (prevState - 1);
            else
                setCurrentBlockState (prevState);
            setFormat (0, text.length(), blockFormat);

            /* also, format whitespaces */
            for (const HighlightingRule &rule : qAsConst (highlightingRules))
            {
                if (rule.format == whiteSpaceFormat)
                {
                    QRegularExpressionMatch match;
                    int index = text.indexOf (rule.pattern, 0, &match);
                    while (index >= 0)
                    {
                        setFormat (index, match.capturedLength(), rule.format);
                        index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                    }
                    break;
                }
            }

            return true;
        }
    }

    return false;
}
/*************************/
void Highlighter::debControlFormatting (const QString &text)
{
    if (text.isEmpty()) return;
    bool formatFurther (false);
    QRegularExpressionMatch expMatch;
    QRegularExpression exp;
    int indx = 0;
    QTextCharFormat debFormat;
    if (text.indexOf (QRegularExpression ("^[^\\s:]+:(?=\\s*)")) == 0)
    {
        formatFurther = true;
        exp.setPattern ("^[^\\s:]+(?=:)");
        if (text.indexOf (exp, 0, &expMatch) == 0)
        {
            /* before ":" */
            debFormat.setFontWeight (QFont::Bold);
            debFormat.setForeground (DarkBlue);
            setFormat (0, expMatch.capturedLength(), debFormat);

            /* ":" */
            debFormat.setForeground (DarkMagenta);
            indx = text.indexOf (":");
            setFormat (indx, 1, debFormat);
            indx ++;

            if (indx < text.count())
            {
                /* after ":" */
                debFormat.setFontWeight (QFont::Normal);
                debFormat.setForeground (DarkGreenAlt);
                setFormat (indx, text.count() - indx , debFormat);
            }
        }
    }
    else if (text.indexOf (QRegularExpression ("^\\s+")) == 0)
    {
        formatFurther = true;
        debFormat.setForeground (DarkGreenAlt);
        setFormat (0, text.count(), debFormat);
    }

    if (formatFurther)
    {
        /* parentheses and brackets */
        exp.setPattern ("\\([^\\(\\)\\[\\]]+\\)|\\[[^\\(\\)\\[\\]]+\\]");
        int index = indx;
        debFormat = neutralFormat;
        debFormat.setFontItalic (true);
        while ((index = text.indexOf (exp, index, &expMatch)) > -1)
        {
            int ml = expMatch.capturedLength();
            setFormat (index, ml, neutralFormat);
            if (ml > 2)
            {
                setFormat (index + 1, ml - 2 , debFormat);

                QRegularExpression rel ("<|>|\\=|~");
                int i = index;
                while ((i = text.indexOf (rel, i)) > -1 && i < index + ml - 1)
                {
                    QTextCharFormat relFormat;
                    relFormat.setForeground (DarkMagenta);
                    setFormat (i, 1, relFormat);
                    ++i;
                }
            }
            index = index + ml;
        }

        /* non-commented URLs */
        debFormat.setForeground (DarkGreenAlt);
        debFormat.setFontUnderline (true);
        QRegularExpressionMatch urlMatch;
        while ((indx = text.indexOf (urlPattern, indx, &urlMatch)) > -1)
        {
            setFormat (indx, urlMatch.capturedLength(), debFormat);
            indx += urlMatch.capturedLength();
        }
    }
}
/*************************/
void Highlighter::latexFormula (const QString &text)
{
    int index = 0;
    QString exp;
    TextBlockData *data = static_cast<TextBlockData *>(currentBlock().userData());
    static const QRegularExpression latexFormulaStart ("\\${2}|\\$|\\\\\\(|\\\\\\[|\\\\begin\\s*{math}|\\\\begin\\s*{displaymath}|\\\\begin\\s*{equation}|\\\\begin\\s*{verbatim}|\\\\begin\\s*{verbatim\\*}");
    QRegularExpressionMatch startMatch;
    QRegularExpression endExp;
    QRegularExpressionMatch endMatch;

    QTextBlock prevBlock = currentBlock().previous();
    if (prevBlock.isValid())
    {
        if (TextBlockData *prevData = static_cast<TextBlockData *>(prevBlock.userData()))
            exp = prevData->labelInfo();
    }

    if (exp.isEmpty())
    {
        index = text.indexOf (latexFormulaStart, index, &startMatch);
        while (isEscapedChar (text, index))
            index = text.indexOf (latexFormulaStart, index + 1, &startMatch);
        /* skip single-line comments */
        if (format (index) == commentFormat || format (index) == urlFormat)
            index = -1;
    }

    while (index >= 0)
    {
        int badIndex = -1;
        int endIndex;

        if (!exp.isEmpty() && index == 0)
        {
            endExp.setPattern (exp);
            endIndex = text.indexOf (endExp, 0, &endMatch);
        }
        else
        {
            if (startMatch.capturedLength() == 1)
                endExp.setPattern ("\\$");
            else if (startMatch.capturedLength() == 2)
            {
                if (text.at (index + 1) == '$')
                    endExp.setPattern ("\\${2}");
                else if (text.at (index + 1) == '(')
                    endExp.setPattern ("\\\\\\)");
                else// if (text.at (index + 1) == '[')
                    endExp.setPattern ("\\\\\\]");
            }
            else
            {
                if (text.at (index + startMatch.capturedLength() - 2) == 'h')
                {
                    if (startMatch.capturedLength() > 6
                        && text.at (index + startMatch.capturedLength() - 6) == 'y')
                    {
                        endExp.setPattern ("\\\\end\\s*{displaymath}");
                    }
                    else
                        endExp.setPattern ("\\\\end\\s*{math}");
                }
                else if (text.at (index + startMatch.capturedLength() - 2) == 'n')
                    endExp.setPattern ("\\\\end\\s*{equation}");
                else if (text.at (index + startMatch.capturedLength() - 2) == 'm')
                    endExp.setPattern ("\\\\end\\s*{verbatim}");
                else
                    endExp.setPattern ("\\\\end\\s*{verbatim\\*}");
            }
            endIndex = text.indexOf (endExp,
                                     index + startMatch.capturedLength(),
                                     &endMatch);
            /* don't format "\begin{math}" or "\begin{equation}" or... */
            if (startMatch.capturedLength() > 2)
                index += startMatch.capturedLength();
        }

        while (isEscapedChar (text, endIndex))
            endIndex = text.indexOf (endExp, endIndex + 1, &endMatch);

        /* if the formula ends ... */
        if (endIndex >= 0)
        {
            /* ... clear the comment format from there to reformat later
               because "%" may be inside a formula now */
            badIndex = endIndex + (endMatch.capturedLength() > 2 ? 0 : endMatch.capturedLength());
            for (int i = badIndex; i < text.length(); ++i)
            {
                if (format (i) == commentFormat || format (i) == urlFormat)
                    setFormat (i, 1, neutralFormat);
            }
        }

        int formulaLength;
        if (endIndex == -1)
        {
            if (data)
                data->insertInfo (endExp.pattern());
            formulaLength = text.length() - index;
        }
        else
            formulaLength = endIndex - index
                            + (endMatch.capturedLength() > 2
                                   ? 0 // don't format "\end{math}" or "\end{equation}"
                                   : endMatch.capturedLength());

        setFormat (index, formulaLength, codeBlockFormat);

        /* reformat the single-line comment from here if the format was cleared before */
        if (badIndex >= 0)
        {
            for (const HighlightingRule &rule : qAsConst (highlightingRules))
            {
                if (rule.format == commentFormat)
                {
                    int INDX = text.indexOf (rule.pattern, badIndex);
                    if (INDX >= 0)
                        setFormat (INDX, text.length() - INDX, commentFormat);
                    /* URLs and notes were cleared too */
                    QString str = text.mid (INDX, text.length() - INDX);
                    int pIndex = 0;
                    QRegularExpressionMatch urlMatch;
                    while ((pIndex = str.indexOf (urlPattern, pIndex, &urlMatch)) > -1)
                    {
                        setFormat (pIndex + INDX, urlMatch.capturedLength(), urlFormat);
                        pIndex += urlMatch.capturedLength();
                    }
                    pIndex = 0;
                    while ((pIndex = str.indexOf (notePattern, pIndex, &urlMatch)) > -1)
                    {
                        if (format (pIndex + INDX) != urlFormat)
                            setFormat (pIndex + INDX, urlMatch.capturedLength(), noteFormat);
                        pIndex += urlMatch.capturedLength();
                    }
                    break;
                }
            }
        }

        index = text.indexOf (latexFormulaStart, index + formulaLength, &startMatch);
        while (isEscapedChar (text, index))
            index = text.indexOf (latexFormulaStart, index + 1, &startMatch);
        if (format (index) == commentFormat || format (index) == urlFormat)
            index = -1;
    }
}
/*************************/
// Start syntax highlighting!
void Highlighter::highlightBlock (const QString &text)
{
    if (progLan.isEmpty()) return;

    if (progLan == "json")
    { // Json's huge lines are also handled separately because of its special syntax
        highlightJsonBlock (text);
        return;
    }

    int bn = currentBlock().blockNumber();
    bool mainFormatting (bn >= startCursor.blockNumber() && bn <= endCursor.blockNumber());

    int txtL = text.length();
    if (txtL <= 10000)
    {
        /* If the paragraph separators are shown, the unformatted text
           will be grayed out. So, we should restore its real color here.
           This is also safe when the paragraph separators are hidden. */
        if (mainFormatting)
            setFormat (0, txtL, mainFormat);

        if (progLan == "fountain")
        {
            highlightFountainBlock (text);
            return;
        }
        if (progLan == "yaml")
        {
            highlightYamlBlock (text);
            return;
        }
        if (progLan == "markdown")
        {
            highlightMarkdownBlock (text);
            return;
        }
        if (progLan == "reST")
        {
            highlightReSTBlock (text);
            return;
        }
    }

    bool rehighlightNextBlock = false;
    int oldOpenNests = 0; QSet<int> oldOpenQuotes; // to be used in SH_CmndSubstVar() (and perl, ruby and css)
    bool oldProperty = false; // to be used with perl, ruby, pascal and java
    QString oldLabel; // to be used with perl, ruby and LaTeX
    if (TextBlockData *oldData = static_cast<TextBlockData *>(currentBlockUserData()))
    {
        oldOpenNests = oldData->openNests();
        oldOpenQuotes = oldData->openQuotes();
        oldProperty = oldData->getProperty();
        oldLabel = oldData->labelInfo();
    }

    int index;
    TextBlockData *data = new TextBlockData;
    data->setLastState (currentBlockState()); // remember the last state (which may not be -1)
    setCurrentBlockUserData (data); // to be fed in later
    setCurrentBlockState (0); // start highlightng, with 0 as the neutral state

    /* set a limit on line length */
    if (txtL > 10000)
    {
        setFormat (0, txtL, translucentFormat);
        data->setHighlighted(); // completely highlighted
        return;
    }

    /* Java is formatted separately, partially because of "Javadoc"
       but also because its single quotes are for literal characters */
    if (progLan == "java")
    {
        singleLineJavaComment (text);
        JavaQuote (text);
        multiLineJavaComment (text);

        if (mainFormatting)
            javaMainFormatting (text);

        javaBraces (text);

        setCurrentBlockUserData (data);
        if (currentBlockState() == data->lastState()
            && data->getProperty() != oldProperty)
        {
            QTextBlock nextBlock = currentBlock().next();
            if (nextBlock.isValid())
                QMetaObject::invokeMethod (this, "rehighlightBlock", Qt::QueuedConnection, Q_ARG (QTextBlock, nextBlock));
        }
        return;
    }

    /********************
     * "Here" Documents *
     ********************/

    if (progLan == "sh" || progLan == "perl" || progLan == "ruby")
    {
        /* first, handle "__DATA__" in perl */
        if (progLan == "perl")
        {
            static const QRegularExpression perlData ("^\\s*__(DATA|END)__");
            QRegularExpressionMatch match;
            if (previousBlockState() == updateState // only used below to distinguish "__DATA__"
                || (previousBlockState() <= 0
                    && text.indexOf (perlData, 0, &match) == 0))
            {
                /* ensure that the main format is applied */
                if (!mainFormatting)
                    setFormat (0, txtL, mainFormat);

                if (match.capturedLength() > 0)
                {
                    QTextCharFormat dataFormat = neutralFormat;
                    dataFormat.setFontWeight (QFont::Bold);
                    setFormat (0, match.capturedLength(), dataFormat);
                }
                for (const HighlightingRule &rule : qAsConst (highlightingRules))
                {
                    if (rule.format == whiteSpaceFormat)
                    {
                        index = text.indexOf (rule.pattern, 0, &match);
                        while (index >= 0)
                        {
                            setFormat (index, match.capturedLength(), rule.format);
                            index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                        }
                        break;
                    }
                }
                setCurrentBlockState (updateState); // completely highlighted
                data->setHighlighted();
                return;
            }
        }
        if (isHereDocument (text))
        {
            data->setHighlighted(); // completely highlighted
            /* transfer the info on open quotes inside code blocks downwards */
            if (data->openNests() > 0)
            {
                QTextBlock nextBlock = currentBlock().next();
                if (nextBlock.isValid())
                {
                    if (TextBlockData *nextData = static_cast<TextBlockData *>(nextBlock.userData()))
                    {
                        if (nextData->openQuotes() != data->openQuotes()
                            || (nextBlock.userState() >= 0 && nextBlock.userState() < endState)) // end delimiter
                        {
                            QMetaObject::invokeMethod (this, "rehighlightBlock", Qt::QueuedConnection, Q_ARG (QTextBlock, nextBlock));
                        }
                    }
                }
            }
            return;
        }
    }
    /* just for debian control file */
    else if (progLan == "deb")
        debControlFormatting (text);

    /************************
     * Single-Line Comments *
     ************************/

    if (progLan != "html")
        singleLineComment (text, 0);

    /* this is only for setting the format of
       command substitution variables in bash */
    rehighlightNextBlock |= SH_CmndSubstVar (text, data, oldOpenNests, oldOpenQuotes);

    /*******************
     * Python Comments *
     *******************/

    pythonMLComment (text, 0);

    /**********************************
     * Pascal Quotations and Comments *
     **********************************/
    if (progLan == "pascal")
    {
        singleLinePascalComment (text);
        pascalQuote (text);
        multiLinePascalComment (text);
        if (currentBlockState() == data->lastState())
            rehighlightNextBlock |= (data->getProperty() != oldProperty);
    }
    /*******************************
     * XML Quotations and Comments *
     *******************************/
    else if (progLan == "xml")
    {
        /* value is handled as a kind of comment */
        multiLineComment (text, 0, xmlGt, xmlLt, xmlValueState, neutralFormat);
        /* multiline quotes as signs of errors in the xml doc */
        xmlQuotes (text);
    }
    /******************
     * LaTeX Formulae *
     ******************/
    else if (progLan == "LaTeX")
    {
        latexFormula (text);
        if (data->labelInfo() != oldLabel)
            rehighlightNextBlock = true;
    }
    /*****************************************
     * (Multiline) Quotations as well as CSS *
     *****************************************/
    else if (progLan == "sh") // bash has its own method
        SH_MultiLineQuote (text);
    else if (progLan == "css")
    { // quotes and urls are highlighted by cssHighlighter() inside CSS values
        cssHighlighter (text, mainFormatting);
        rehighlightNextBlock |= (data->openNests() != oldOpenNests);
    }
    else if (multilineQuote_)
        rehighlightNextBlock |= multiLineQuote (text);

    /**********************
     * Multiline Comments *
     **********************/

    if (!commentStartExpression.pattern().isEmpty() && progLan != "python")
        rehighlightNextBlock |= multiLineComment (text, 0,
                                                  commentStartExpression, commentEndExpression,
                                                  commentState, commentFormat);

    /* only javascript, qml, perl and ruby */
    multiLineRegex (text, 0);
    /* "Property" is used for knowing about backquotes, "label" is used
       for delimiter strings and "OpenNests" for paired delimiters. */
    if ((progLan == "perl" || progLan == "ruby") && currentBlockState() == data->lastState())
        rehighlightNextBlock |= (data->labelInfo() != oldLabel || data->getProperty() != oldProperty
                                 || data->openNests() != oldOpenNests);

    QTextCharFormat fi;

    /*************
     * HTML Only *
     *************/

    if (progLan == "html")
    {
        htmlBrackets (text);
        htmlCSSHighlighter (text);
        htmlJavascript (text);
        /* also consider quotes and URLs inside CSS values */
        rehighlightNextBlock |= (data->openNests() != oldOpenNests);
        /* go to braces matching */
    }

    /*******************
     * Main Formatting *
     *******************/

    // we format html embedded javascript in htmlJavascript()
    else if (mainFormatting)
    {
        data->setHighlighted(); // completely highlighted
        for (const HighlightingRule &rule : qAsConst (highlightingRules))
        {
            /* single-line comments are already formatted */
            if (rule.format == commentFormat)
                continue;

            QRegularExpressionMatch match;
            index = text.indexOf (rule.pattern, 0, &match);
            /* skip quotes and all comments */
            if (rule.format != whiteSpaceFormat)
            {
                fi = format (index);
                while (index >= 0
                       && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                           || fi == commentFormat || fi == urlFormat
                           || fi == regexFormat))
                {
                    index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                    fi = format (index);
                }
            }

            while (index >= 0)
            {
                int length = match.capturedLength();
                int l = length;
                /* In c/c++, the neutral pattern after "#define" may contain
                   a (double-)slash but it's always good to check whether a
                   part of the match is inside an already formatted region. */
                if (rule.format != whiteSpaceFormat)
                {
                    while (format (index + l - 1) == commentFormat
                           /*|| format (index + l - 1) == commentFormat
                           || format (index + l - 1) == urlFormat
                           || format (index + l - 1) == quoteFormat
                           || format (index + l - 1) == altQuoteFormat
                           || format (index + l - 1) == urlInsideQuoteFormat
                           || format (index + l - 1) == regexFormat*/)
                    {
                        -- l;
                    }
                }
                setFormat (index, l, rule.format);
                index = text.indexOf (rule.pattern, index + length, &match);

                if (rule.format != whiteSpaceFormat)
                {
                    fi = format (index);
                    while (index >= 0
                           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
                               || fi == commentFormat || fi == urlFormat
                               || fi == regexFormat))
                    {
                        index = text.indexOf (rule.pattern, index + match.capturedLength(), &match);
                        fi = format (index);
                    }
                }
            }
        }
    }

    /*********************************************
     * Parentheses, Braces and brackets Matching *
     *********************************************/

    /* left parenthesis */
    index = text.indexOf ('(');
    fi = format (index);
    while (index >= 0
           && (fi == quoteFormat || fi == altQuoteFormat || fi == urlInsideQuoteFormat
               || fi == commentFormat || fi == urlFormat
               || fi == regexFormat
               || (progLan == "sh" && isEscapedChar (text, index))))
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
                   || fi == commentFormat || fi == urlFormat
                   || fi == regexFormat
                   || (progLan == "sh" && isEscapedChar (text, index))))
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
               || fi == commentFormat || fi == urlFormat
               || fi == regexFormat
               || (progLan == "sh" && isEscapedChar (text, index))))
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
                   || fi == commentFormat || fi == urlFormat
                   || fi == regexFormat
                   || (progLan == "sh" && isEscapedChar (text, index))))
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
               || fi == regexFormat))
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
                   || fi == regexFormat))
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
               || fi == regexFormat))
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
                   || fi == regexFormat))
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
               || fi == regexFormat
               || (progLan == "sh" && isEscapedChar (text, index))))
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
                   || fi == regexFormat
                   || (progLan == "sh" && isEscapedChar (text, index))))
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
               || fi == regexFormat
               || (progLan == "sh" && isEscapedChar (text, index))))
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
                   || fi == regexFormat
                   || (progLan == "sh" && isEscapedChar (text, index))))
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
