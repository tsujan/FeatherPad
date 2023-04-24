set(QT5_MINIMUM_VERSION "5.15.0")
set(QT6_MINIMUM_VERSION "6.2.0")
set(HUNSPELL_MINIMUM_VERSION "1.6")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

if(NOT ENABLE_QT5)
  set(QT_VERSION_MAJOR 6)
  find_package(Qt6 "${QT6_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport DBus)
else()
  set(QT_VERSION_MAJOR 5)
  find_package(Qt5 "${QT5_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport DBus)
endif()

# for spell checking (see FindHUNSPELL.cmake)
find_package(HUNSPELL "${HUNSPELL_MINIMUM_VERSION}" REQUIRED)

# optional localization
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS LinguistTools)

# OS/2 needs an additional lib and tune the processor
if(OS2)
  add_definitions(-mtune=pentium4 -march=pentium4)
  set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lcx")
endif()

# X11 for Linux/Unix
if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  if(QT_VERSION_MAJOR EQUAL 5)
    find_package(Qt5 "${QT5_MINIMUM_VERSION}" REQUIRED COMPONENTS X11Extras)
  endif()
  find_package(X11 REQUIRED)
endif()

if(HAIKU)
  set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lnetwork")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# for the old code; not needed anymore
#find_package(MAGIC_INCLUDE_DIR magic.h)
#find_library(MAGIC_LIBRARY magic $ENV{MAGIC_LIB_PATH})

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  if(QT_VERSION_MAJOR EQUAL 6)
    include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                        ${Qt6Core_INCLUDE_DIRS}
                        ${Qt6Gui_INCLUDE_DIRS}
                        ${Qt6Widgets_INCLUDE_DIRS}
                        ${Qt6Svg_INCLUDE_DIRS}
                        ${Qt6PrintSupport_INCLUDE_DIRS}
                        ${Qt6DBus_INCLUDE_DIRS}
                        ${X11_INCLUDE_DIR}
                        ${HUNSPELL_INCLUDE_DIRS})
  else()
    include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                        ${Qt5Core_INCLUDE_DIRS}
                        ${Qt5Gui_INCLUDE_DIRS}
                        ${Qt5Widgets_INCLUDE_DIRS}
                        ${Qt5Svg_INCLUDE_DIRS}
                        ${Qt5X11Extras_INCLUDE_DIRS}
                        ${Qt5PrintSupport_INCLUDE_DIRS}
                        ${Qt5DBus_INCLUDE_DIRS}
                        ${X11_INCLUDE_DIR}
                        ${HUNSPELL_INCLUDE_DIRS})
  endif()
  add_definitions(-DHAS_X11)
else()
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt${QT_VERSION_MAJOR}Core_INCLUDE_DIRS}
                      ${Qt${QT_VERSION_MAJOR}Gui_INCLUDE_DIRS}
                      ${Qt${QT_VERSION_MAJOR}Widgets_INCLUDE_DIRS}
                      ${Qt${QT_VERSION_MAJOR}Svg_INCLUDE_DIRS}
                      ${Qt${QT_VERSION_MAJOR}PrintSupport_INCLUDE_DIRS}
                      ${Qt${QT_VERSION_MAJOR}DBus_INCLUDE_DIRS}
                      ${HUNSPELL_INCLUDE_DIRS})
endif()

if(HAIKU)
  add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/data")
elseif(UNIX OR OS2)
  add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")
endif()

set(featherpad_SRCS
    main.cpp
    singleton.cpp
    signalDaemon.cpp
    fpwin.cpp
    encoding.cpp
    tabwidget.cpp
    menubartitle.cpp
    lineedit.cpp
    textedit.cpp
    tabbar.cpp
    find.cpp
    replace.cpp
    pref.cpp
    config.cpp
    brackets.cpp
    syntax.cpp
    vscrollbar.cpp
    loading.cpp
    printing.cpp
    tabpage.cpp
    searchbar.cpp
    session.cpp
    fontDialog.cpp
    sidepane.cpp
    svgicons.cpp
    spellChecker.cpp
    spellDialog.cpp
    highlighter/highlighter.cpp
    highlighter/highlighter-cmake.cpp
    highlighter/highlighter-css.cpp
    highlighter/highlighter-fountain.cpp
    highlighter/highlighter-java.cpp
    highlighter/highlighter-json.cpp
    highlighter/highlighter-html.cpp
    highlighter/highlighter-lua.cpp
    highlighter/highlighter-markdown.cpp
    highlighter/highlighter-pascal.cpp
    highlighter/highlighter-patterns.cpp
    highlighter/highlighter-perl-regex.cpp
    highlighter/highlighter-regex.cpp
    highlighter/highlighter-rest.cpp
    highlighter/highlighter-ruby.cpp
    highlighter/highlighter-rust.cpp
    highlighter/highlighter-sh.cpp
    highlighter/highlighter-tcl.cpp
    highlighter/highlighter-toml.cpp
    highlighter/highlighter-xml.cpp
    highlighter/highlighter-yaml.cpp)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  set(featherpad_SRCS ${featherpad_SRCS} x11.cpp)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
  qt6_add_dbus_adaptor(featherpad_DBUS_SRCS
      org.featherpad.Application.xml
      singleton.h
      FeatherPad::FPsingleton
      featherpadadaptor
      FeatherPadAdaptor
  )
else()
  qt5_add_dbus_adaptor(featherpad_DBUS_SRCS
      org.featherpad.Application.xml
      singleton.h
      FeatherPad::FPsingleton
      featherpadadaptor
      FeatherPadAdaptor
  )
endif()

set(featherpad_RESOURCES data/fp.qrc)
set(DESKTOP_FILE data/featherpad.desktop)
set(METAINFO_FILE data/featherpad.metainfo.xml)
if(NOT OS2)
  set(ICON_FILE data/featherpad.svg)
else()
  enable_language(RC)
  set_source_files_properties(data/featherpad_os2.rc PROPERTIES LANGUAGE RC)
  set(featherpad_SRCS ${featherpad_SRCS} data/featherpad_os2.rc)
endif()
set(HELP_FILE data/help)
file(GLOB HELP_TR_FILES data/help_*)

if(QT_VERSION_MAJOR EQUAL 6)
  set(EXTRA_HEADERS warningbar.h messagebox.h filedialog.h)
else()
  if(CMAKE_VERSION VERSION_LESS "3.10.0") # CMP0071 in 3.10: Let AUTOMOC and AUTOUIC process GENERATED files
    qt5_wrap_cpp(EXTRA_HEADERS warningbar.h messagebox.h filedialog.h)
  else()
    set(EXTRA_HEADERS warningbar.h messagebox.h filedialog.h)
  endif()
endif()

# translations
if(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/featherpad_*.ts)
  if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_translation(QM_FILES ${TS_FILES})
  else()
    qt5_add_translation(QM_FILES ${TS_FILES})
  endif()
  add_executable(featherpad ${featherpad_SRCS} ${featherpad_DBUS_SRCS} ${featherpad_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE} ${QM_FILES})
  if(HAIKU)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/translations")
  elseif(APPLE)
    SET_SOURCE_FILES_PROPERTIES(${QM_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/translations)
  elseif(UNIX OR OS2)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/featherpad/translations")
  endif()
else(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
  add_executable(featherpad ${featherpad_SRCS} ${featherpad_DBUS_SRCS} ${featherpad_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE})
endif(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(featherpad ${Qt6Core_LIBRARIES}
                                     ${Qt6Gui_LIBRARIES}
                                     ${Qt6Widgets_LIBRARIES}
                                     ${Qt6Svg_LIBRARIES}
                                     ${Qt6PrintSupport_LIBRARIES}
                                     ${Qt6DBus_LIBRARIES}
                                     ${X11_LIBRARIES}
                                     ${HUNSPELL_LIBRARIES})
  else()
    target_link_libraries(featherpad ${Qt5Core_LIBRARIES}
                                     ${Qt5Gui_LIBRARIES}
                                     ${Qt5Widgets_LIBRARIES}
                                     ${Qt5Svg_LIBRARIES}
                                     ${Qt5X11Extras_LIBRARIES}
                                     ${Qt5PrintSupport_LIBRARIES}
                                     ${Qt5DBus_LIBRARIES}
                                     ${X11_LIBRARIES}
                                     ${HUNSPELL_LIBRARIES})
  endif()
else()
  target_link_libraries(featherpad ${Qt${QT_VERSION_MAJOR}Core_LIBRARIES}
                                   ${Qt${QT_VERSION_MAJOR}Gui_LIBRARIES}
                                   ${Qt${QT_VERSION_MAJOR}Widgets_LIBRARIES}
                                   ${Qt${QT_VERSION_MAJOR}Svg_LIBRARIES}
                                   ${Qt${QT_VERSION_MAJOR}PrintSupport_LIBRARIES}
                                   ${Qt${QT_VERSION_MAJOR}DBus_LIBRARIES}
                                   ${HUNSPELL_LIBRARIES})
endif()

# installation
if(HAIKU)
  install(TARGETS featherpad RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
  install(FILES ${HELP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/data")
  install(FILES ${HELP_TR_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/data")
elseif(APPLE)
  SET_TARGET_PROPERTIES(featherpad PROPERTIES RESOURCE ${HELP_FILE})
  SET_TARGET_PROPERTIES(featherpad PROPERTIES RESOURCE ${HELP_TR_FILES})
  SET_SOURCE_FILES_PROPERTIES(FILES ${HELP_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/help)
  SET_SOURCE_FILES_PROPERTIES(FILES ${HELP_TR_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/helps)
  SET_TARGET_PROPERTIES(featherpad PROPERTIES MACOSX_BUNDLE TRUE)
  INSTALL(TARGETS featherpad DESTINATION ${CMAKE_INSTALL_PREFIX})
elseif(UNIX)
  # make symlink
  add_custom_target(fpad_symlink ALL ${CMAKE_COMMAND} -E create_symlink featherpad fpad DEPENDS featherpad COMMENT "Creating fpad as a symlink to featherpad")

  install(TARGETS featherpad RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
  install(FILES ${DESKTOP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
  install(FILES ${METAINFO_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/metainfo")
  install(FILES ${ICON_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
  install(FILES ${HELP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/featherpad")
  # also install translated help files
  install(FILES ${HELP_TR_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/featherpad")
  # install symlink
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fpad DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
elseif(OS2)
  SET_TARGET_PROPERTIES(featherpad PROPERTIES
  	OS2_DEF_EXEType "WINDOWAPI"
  )
  install(TARGETS featherpad RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
  install(FILES ${HELP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/featherpad")
  # also install translated help files
  install(FILES ${HELP_TR_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/featherpad")
endif()
