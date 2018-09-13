QT += core gui \
      widgets \
      printsupport \
      network \
      svg
haiku {
	TARGET = FeatherPad
}else {
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
           highlighter.cpp \
           find.cpp \
           replace.cpp \
           pref.cpp \
           config.cpp \
           brackets.cpp \
           syntax.cpp \
           highlighter-sh.cpp \
           highlighter-html.cpp \
           highlighter-patterns.cpp \
           highlighter-jsregex.cpp \
           vscrollbar.cpp \
           loading.cpp \
           tabpage.cpp \
           searchbar.cpp \
           session.cpp \
           sidepane.cpp \
           svgicons.cpp

HEADERS += singleton.h \
           fpwin.h \
           encoding.h \
           tabwidget.h \
           lineedit.h \
           textedit.h \
           tabbar.h \
           highlighter.h \
           vscrollbar.h \
           filedialog.h \
           config.h \
           pref.h \
           loading.h \
           messagebox.h \
           tabpage.h \
           searchbar.h \
           session.h \
           warningbar.h \
           utils.h \
           sidepane.h \
           svgicons.h

FORMS += fp.ui \
         predDialog.ui \
         sessionDialog.ui \
         about.ui

RESOURCES += data/fp.qrc

contains(WITHOUT_X11, YES) {
  message("Compiling without X11...")
}
else:unix:!macx:!haiku {
  QT += x11extras
  SOURCES += x11.cpp
  HEADERS += x11.h
  LIBS += -lX11
  DEFINES += HAS_X11
}

unix {
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
unix:!haiku {
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

  iconsvg.path = $$DATADIR/icons/hicolor/scalable/apps
  iconsvg.files += ./data/$${TARGET}.svg

  help.path = $$DATADIR/featherpad
  help.files += ./data/help

  trans.path = $$DATADIR/featherpad
  trans.files += data/translations/translations

  INSTALLS += target slink desktop iconsvg help trans

}else:haiku {
	isEmpty(PREFIX) {
	PREFIX = /boot/home/config/non-packaged/apps/FeatherPad
  }
	BINDIR = $$PREFIX
	DATADIR =$$PREFIX/data

	DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

	target.path =$$BINDIR

	help.path = $$DATADIR
	help.files += ./data/help

	trans.path = $$PREFIX
	trans.files += data/translations/translations
	INSTALLS += target help trans
}
