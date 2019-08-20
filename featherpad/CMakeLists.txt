set(QT_MINIMUM_VERSION "5.7.1")
set(HUNSPELL_MINIMUM_VERSION "1.6")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

include_directories(/usr/local/include/)

find_package(Qt5Core "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Gui "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Widgets "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Svg "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Network "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5PrintSupport "${QT_MINIMUM_VERSION}" REQUIRED)

# for spell checking (see FindHUNSPELL.cmake)
find_package(HUNSPELL "${HUNSPELL_MINIMUM_VERSION}" REQUIRED)

# optional localization
find_package(Qt5LinguistTools QUIET)

# X11 for Linux/Unix
if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  find_package(Qt5X11Extras "${QT_MINIMUM_VERSION}" REQUIRED)
  find_package(X11 REQUIRED)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# for the old code; not needed anymore
#find_package(MAGIC_INCLUDE_DIR magic.h)
#find_library(MAGIC_LIBRARY magic $ENV{MAGIC_LIB_PATH})

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt5Core_INCLUDE_DIRS}
                      ${Qt5Gui_INCLUDE_DIRS}
                      ${Qt5Widgets_INCLUDE_DIRS}
                      ${Qt5Svg_INCLUDE_DIRS}
                      ${Qt5X11Extras_INCLUDE_DIRS}
                      ${Qt5Network_INCLUDE_DIRS}
                      ${Qt5PrintSupport_INCLUDE_DIRS}
                      ${X11_INCLUDE_DIR}
                      ${HUNSPELL_INCLUDE_DIRS})
  add_definitions(-DHAS_X11)
else()
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt5Core_INCLUDE_DIRS}
                      ${Qt5Gui_INCLUDE_DIRS}
                      ${Qt5Widgets_INCLUDE_DIRS}
                      ${Qt5Svg_INCLUDE_DIRS}
                      ${Qt5Network_INCLUDE_DIRS}
                      ${Qt5PrintSupport_INCLUDE_DIRS}
                      ${HUNSPELL_INCLUDE_DIRS})
endif()

if(HAIKU)
  add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/data")
elseif(APPLE)
  #add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app")
elseif(UNIX)
  add_definitions(-DDATADIR="${CMAKE_INSTALL_PREFIX}/share")
endif()

set(featherpad_SRCS
    main.cpp
    singleton.cpp
    fpwin.cpp
    encoding.cpp
    tabwidget.cpp
    lineedit.cpp
    textedit.cpp
    tabbar.cpp
    highlighter.cpp
    find.cpp
    replace.cpp
    pref.cpp
    config.cpp
    brackets.cpp
    syntax.cpp
    highlighter-sh.cpp
    highlighter-html.cpp
    highlighter-patterns.cpp
    highlighter-regex.cpp
    highlighter-perl-regex.cpp
    vscrollbar.cpp
    loading.cpp
    tabpage.cpp
    searchbar.cpp
    session.cpp
    fontDialog.cpp
    sidepane.cpp
    svgicons.cpp
    spellChecker.cpp
    spellDialog.cpp)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  set(featherpad_SRCS ${featherpad_SRCS} x11.cpp)
endif()

set(featherpad_RESOURCES data/fp.qrc)
set(DESKTOP_FILE data/featherpad.desktop)
set(ICON_FILE data/featherpad.svg)
set(HELP_FILE data/help)
file(GLOB HELP_TR_FILES data/help_*)

if(CMAKE_VERSION VERSION_LESS "3.10.0") # CMP0071 in 3.10: Let AUTOMOC and AUTOUIC process GENERATED files
  qt5_wrap_cpp(EXTRA_HEADERS warningbar.h messagebox.h filedialog.h)
else()
  set(EXTRA_HEADERS warningbar.h messagebox.h filedialog.h)
endif()

# translations
if(Qt5LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/featherpad_*.ts)
  qt5_add_translation(QM_FILES ${TS_FILES})
  add_executable(featherpad ${featherpad_SRCS} ${featherpad_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE} ${QM_FILES})
  if(HAIKU)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}")
  elseif(APPLE)
    SET_SOURCE_FILES_PROPERTIES(${QM_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/translations)
  elseif(UNIX)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/featherpad/translations")
  endif()
else(Qt5LinguistTools_FOUND)
  add_executable(featherpad ${featherpad_SRCS} ${featherpad_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE})
endif(Qt5LinguistTools_FOUND)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  target_link_libraries(featherpad ${Qt5Core_LIBRARIES}
                                   ${Qt5Gui_LIBRARIES}
                                   ${Qt5Widgets_LIBRARIES}
                                   ${Qt5Svg_LIBRARIES}
                                   ${Qt5X11Extras_LIBRARIES}
                                   ${Qt5Network_LIBRARIES}
                                   ${Qt5PrintSupport_LIBRARIES}
                                   ${X11_LIBRARIES}
                                   ${HUNSPELL_LIBRARIES})
else()
  target_link_libraries(featherpad ${Qt5Core_LIBRARIES}
                                   ${Qt5Gui_LIBRARIES}
                                   ${Qt5Widgets_LIBRARIES}
                                   ${Qt5Svg_LIBRARIES}
                                   ${Qt5Network_LIBRARIES}
                                   ${Qt5PrintSupport_LIBRARIES}
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
  install(FILES ${ICON_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
  install(FILES ${HELP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/featherpad")
  # also install translated help files
  install(FILES ${HELP_TR_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/featherpad")
  # install symlink
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fpad DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()
