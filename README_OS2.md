# FeatherPad for OS/2

## Clipboard
The Qt5 port doesn't support the system clipboard, yet 
(cf https://github.com/bitwiseworks/qtbase-os2/issues/123).
Because of the, this FeatherPad can't support system clipboard, too.
The application clipboard will work, though. 

## Command line
* Because of limitation of Presentation Manager application, you will not see
any output on the command line. If you want to see the help or version strings
on the command line, you have to redirect the output, e.g. 'featherpad -h | tee'.

* The Qt5 port seems to have trouble to recognize OS/2 native path names 
like c:\config.sys, when called by application parameter, e.g. by WPS object.
If you prepend 'file:' to the file name, e.g. 'featherpad file:c:\config.sys', 
FeatherPad will accept the file.
If you use Unix style path names like '/@unixroot/config.sys', the recognition
will work as expected.

## Miscellanea
* This port was made by Jochen Schäfer. You find OS/2 binaries on my Github repository
(https://github.com/josch1710/FeatherPad).
* The application icon was made by the great David Graser from the original.

