# FeatherPad for OS/2 (by josch1710)

## Clipboard
The Qt5 port doesn't support the system clipboard, yet
(cf https://github.com/bitwiseworks/qtbase-os2/issues/123).
Because of this, FeatherPad can't support the system clipboard, either.
The application clipboard will work, though.

## Command line

Because of limitation of Presentation Manager application, you will not see
any output on the command line. If you want to see the help or version strings
on the command line, you have to redirect the output, e.g. `featherpad -h | tee -`.

## Miscellanea
* This port was made by Jochen Schäfer <os2@joschs-robotics.de>. You find OS/2 binaries
 on [my GitHub repository](https://github.com/josch1710/FeatherPad/releases).
* The application icon was made by the great David Graser from the original.
