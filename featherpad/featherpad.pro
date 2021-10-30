lessThan(QT_MAJOR_VERSION, 6) {
  lessThan(QT_MAJOR_VERSION, 5) {
    error("FeatherPad needs at least Qt 5.12.0.")
  } else {
    lessThan(QT_MINOR_VERSION, 12) {
        error("FeatherPad needs at least Qt 5.12.0.")
    }
  }
} else {
  equals(QT_MAJOR_VERSION, 6) {
    lessThan(QT_MINOR_VERSION, 2) {
      error("FeatherPad needs at least Qt 6.2.0.")
    }
  } else {
    error("FeatherPad cannot be compiled against this version of Qt.")
  }
}

QT += core gui \
      widgets \
      printsupport \
      network \
      svg

haiku|macx {
  TARGET = FeatherPad
}
else {
  TARGET = featherpad
}

TEMPLATE = app
CONFIG += c++11

SOURCES += main.cpp \
           singleton.cpp \
           fpwin.cpp \
           encoding.cpp \
           tabwidget.cpp \
           lineedit.cpp \
           textedit.cpp \
           tabbar.cpp \
           find.cpp \
           replace.cpp \
           pref.cpp \
           config.cpp \
           brackets.cpp \
           syntax.cpp \
           vscrollbar.cpp \
           loading.cpp \
           printing.cpp \
           tabpage.cpp \
           searchbar.cpp \
           session.cpp \
           fontDialog.cpp \
           sidepane.cpp \
           svgicons.cpp \
           spellChecker.cpp \
           spellDialog.cpp \
           highlighter/highlighter.cpp \
           highlighter/highlighter-css.cpp \
           highlighter/highlighter-fountain.cpp \
           highlighter/highlighter-java.cpp \
           highlighter/highlighter-json.cpp \
           highlighter/highlighter-html.cpp \
           highlighter/highlighter-markdown.cpp \
           highlighter/highlighter-pascal.cpp \
           highlighter/highlighter-patterns.cpp \
           highlighter/highlighter-perl-regex.cpp \
           highlighter/highlighter-regex.cpp \
           highlighter/highlighter-rest.cpp \
           highlighter/highlighter-ruby.cpp \
           highlighter/highlighter-sh.cpp \
           highlighter/highlighter-yaml.cpp

HEADERS += singleton.h \
           fpwin.h \
           encoding.h \
           tabwidget.h \
           lineedit.h \
           textedit.h \
           tabbar.h \
           vscrollbar.h \
           filedialog.h \
           config.h \
           pref.h \
           loading.h \
           printing.h \
           messagebox.h \
           tabpage.h \
           searchbar.h \
           session.h \
           fontDialog.h \
           warningbar.h \
           sidepane.h \
           svgicons.h \
           spellChecker.h \
           spellDialog.h \
           highlighter/highlighter.h

FORMS += fp.ui \
         prefDialog.ui \
         sessionDialog.ui \
         fontDialog.ui \
         about.ui \
         spellDialog.ui

RESOURCES += data/fp.qrc

contains(WITHOUT_X11, YES) {
  message("Compiling without X11...")
}
else:unix:!macx:!haiku:!os2 {
  lessThan(QT_MAJOR_VERSION, 6) {
    QT += x11extras
  }
  SOURCES += x11.cpp
  HEADERS += x11.h
  LIBS += -lX11
  DEFINES += HAS_X11
}

unix {
  CONFIG += link_pkgconfig
  PKGCONFIG += hunspell
  #TRANSLATIONS
  exists($$[QT_INSTALL_BINS]/lrelease) {
    TRANSLATIONS = $$system("find data/translations/ -name 'featherpad_*.ts'")
    updateqm.input = TRANSLATIONS
    updateqm.output = data/translations/translations/${QMAKE_FILE_BASE}.qm
    updateqm.commands = $$[QT_INSTALL_BINS]/lrelease ${QMAKE_FILE_IN} -qm data/translations/translations/${QMAKE_FILE_BASE}.qm
    updateqm.CONFIG += no_link target_predeps
    QMAKE_EXTRA_COMPILERS += updateqm
  }
}
else:os2 {
  LIBS += -lhunspell-1.7_dll
  RC_FILE = data/featherpad_os2.rc
  #TRANSLATIONS
  exists($$[QT_INSTALL_BINS]/lrelease.exe) {
    TRANSLATIONS = $$system("find data/translations/ -name 'featherpad_*.ts'")
    updateqm.input = TRANSLATIONS
    updateqm.output = data/translations/translations/${QMAKE_FILE_BASE}.qm
    updateqm.commands = $$[QT_INSTALL_BINS]/lrelease ${QMAKE_FILE_IN} -qm data/translations/translations/${QMAKE_FILE_BASE}.qm
    updateqm.CONFIG += no_link target_predeps
    QMAKE_EXTRA_COMPILERS += updateqm
  }
}

unix:!haiku:!macx:!os2 {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  BINDIR = $$PREFIX/bin
  DATADIR =$$PREFIX/share

  DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

  #MAKE INSTALL

  target.path =$$BINDIR

  # add the fpad symlink
  slink.path = $$BINDIR
  slink.extra += ln -sf $${TARGET} fpad && cp -P fpad $(INSTALL_ROOT)$$BINDIR

  desktop.path = $$DATADIR/applications
  desktop.files += ./data/$${TARGET}.desktop

  metainfo.path = $$DATADIR/metainfo
  metainfo.files += ./data/$${TARGET}.metainfo.xml

  iconsvg.path = $$DATADIR/icons/hicolor/scalable/apps
  iconsvg.files += ./data/$${TARGET}.svg

  help.path = $$DATADIR/featherpad
  help.files += ./data/help
  help.files += ./data/help_*

  trans.path = $$DATADIR/featherpad
  trans.files += data/translations/translations

  INSTALLS += target slink desktop metainfo iconsvg help trans
}
else:haiku {
  isEmpty(PREFIX) {
    PREFIX = /boot/home/config/non-packaged/apps/FeatherPad
  }
  BINDIR = $$PREFIX
  DATADIR =$$PREFIX/data

  DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

  target.path =$$BINDIR

  help.path = $$DATADIR
  help.files += ./data/help
  help.files += ./data/help_*

  trans.path = $$PREFIX
  trans.files += data/translations/translations
  INSTALLS += target help trans
}
else:macx{
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /Applications
  }
  BINDIR = $$PREFIX
  DATADIR = "$$BINDIR/$$TARGET".app

  DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

  #MAKE INSTALL

  target.path =$$BINDIR

  help.path = $$DATADIR/Contents/Resources/
  help.files += ./data/help
  help.files += ./data/help_*

  trans.path = $$DATADIR/Contents/Resources/
  trans.files += data/translations/translations

  INSTALLS += target help trans
}
else:os2 {
  #VARIABLES

  isEmpty(PREFIX) {
    PREFIX = /@unixroot/usr
  }
  BINDIR = $$PREFIX/bin
  DATADIR =$$PREFIX/share

  DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

  #MAKE INSTALL

  target.path =$$BINDIR

  help.path = $$DATADIR/featherpad
  help.files += ./data/help
  help.files += ./data/help_*

  trans.path = $$DATADIR/featherpad
  trans.files += data/translations/translations

  INSTALLS += target help trans
}
