# FeatherPad for OS/2 (by josch1710)

## Clipboard
Before version 5.15.2-1, the OS/2 port of Qt5 didn't support the system clipboard
(cf https://github.com/bitwiseworks/qtbase-os2/issues/123).
If you have an older version of Qt5, please update to the newest version.
Otherwise, FeatherPad can't support the system clipboard. The application clipboard will work, though.

## Command line

Because of limitations of Presentation Manager applications, you will not see
any output on the command line. If you want to see the help or version strings
on the command line, you have to redirect the output, e.g. `featherpad -h | tee -`.

## Miscellanea
* This port was made by Jochen Schäfer <os2@joschs-robotics.de>. You find OS/2 binaries
 on [my GitHub repository](https://github.com/josch1710/FeatherPad/releases).
* The application icon was made by the great David Graser from the original.
