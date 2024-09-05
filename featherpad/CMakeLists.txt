set(QT6_MINIMUM_VERSION "6.2.0")
set(HUNSPELL_MINIMUM_VERSION "1.6")

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(Qt6 "${QT6_MINIMUM_VERSION}" REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport DBus)

# for spell checking (see FindHUNSPELL.cmake)
find_package(HUNSPELL "${HUNSPELL_MINIMUM_VERSION}" REQUIRED)

# optional localization
find_package(Qt6 QUIET COMPONENTS LinguistTools)

if(HAIKU)
  set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lnetwork")
elseif(OS2)
  # OS/2 needs an additional lib and tune the processor
  add_definitions(-mtune=pentium4 -march=pentium4)
  set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lcx")
elseif(WIN32)
  set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,--subsystem,windows")
endif()

# X11 for Linux/Unix
if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU AND NOT WIN32)
  find_package(X11 REQUIRED)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt6Core_INCLUDE_DIRS}
                      ${Qt6Gui_INCLUDE_DIRS}
                      ${Qt6Widgets_INCLUDE_DIRS}
                      ${Qt6Svg_INCLUDE_DIRS}
                      ${Qt6PrintSupport_INCLUDE_DIRS}
                      ${Qt6DBus_INCLUDE_DIRS}
                      ${X11_INCLUDE_DIR}
                      ${HUNSPELL_INCLUDE_DIRS})
  add_definitions(-DHAS_X11)
else()
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt6Core_INCLUDE_DIRS}
                      ${Qt6Gui_INCLUDE_DIRS}
                      ${Qt6Widgets_INCLUDE_DIRS}
                      ${Qt6Svg_INCLUDE_DIRS}
                      ${Qt6PrintSupport_INCLUDE_DIRS}
                      ${Qt6DBus_INCLUDE_DIRS}
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

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU AND NOT WIN32)
  set(featherpad_SRCS ${featherpad_SRCS} x11.cpp)
endif()

if(NOT WIN32)
  set(featherpad_SRCS ${featherpad_SRCS} signalDaemon.cpp)
endif()

qt6_add_dbus_adaptor(featherpad_DBUS_SRCS
    org.featherpad.Application.xml
    singleton.h
    FeatherPad::FPsingleton
    featherpadadaptor
    FeatherPadAdaptor
)

set(featherpad_RESOURCES data/fp.qrc)
set(DESKTOP_FILE data/featherpad.desktop)
set(METAINFO_FILE data/featherpad.metainfo.xml)

if(OS2)
  enable_language(RC)
  set_source_files_properties(data/featherpad_os2.rc PROPERTIES LANGUAGE RC)
  set(featherpad_SRCS ${featherpad_SRCS} data/featherpad_os2.rc)
elseif(NOT WIN32)
  set(ICON_FILE data/featherpad.svg)
endif()

set(HELP_FILE data/help)
file(GLOB HELP_TR_FILES data/help_*)

set(EXTRA_HEADERS warningbar.h messagebox.h filedialog.h)

# translations
if(Qt6LinguistTools_FOUND)
  file(GLOB TS_FILES data/translations/featherpad_*.ts)
  qt6_add_translation(QM_FILES ${TS_FILES})
  add_executable(featherpad ${featherpad_SRCS} ${featherpad_DBUS_SRCS} ${featherpad_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE} ${QM_FILES})
  if(HAIKU)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/translations")
  elseif(APPLE)
    SET_SOURCE_FILES_PROPERTIES(${QM_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/translations)
  elseif(UNIX OR OS2)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/featherpad/translations")
  elseif(WIN32)
    install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/data/translations")
  endif()
else(Qt6LinguistTools_FOUND)
  add_executable(featherpad ${featherpad_SRCS} ${featherpad_DBUS_SRCS} ${featherpad_RESOURCES} ${EXTRA_HEADERS} ${DESKTOP_FILE})
endif(Qt6LinguistTools_FOUND)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU AND NOT WIN32)
  target_link_libraries(featherpad ${Qt6Core_LIBRARIES}
                                   ${Qt6Gui_LIBRARIES}
                                   ${Qt6Widgets_LIBRARIES}
                                   ${Qt6Svg_LIBRARIES}
                                   ${Qt6PrintSupport_LIBRARIES}
                                   ${Qt6DBus_LIBRARIES}
                                   ${X11_LIBRARIES}
                                   ${HUNSPELL_LIBRARIES})
elseif(WIN32)
  target_link_libraries(featherpad ${Qt6Core_LIBRARIES}
                                   ${Qt6Gui_LIBRARIES}
                                   ${Qt6Widgets_LIBRARIES}
                                   ${Qt6Svg_LIBRARIES}
                                   ${Qt6PrintSupport_LIBRARIES}
                                   ${Qt6DBus_LIBRARIES}
                                   ${HUNSPELL_LIBRARIES}
                                   ${CMAKE_CURRENT_SOURCE_DIR}/data/featherpad_win.o)
else()
  target_link_libraries(featherpad ${Qt6Core_LIBRARIES}
                                   ${Qt6Gui_LIBRARIES}
                                   ${Qt6Widgets_LIBRARIES}
                                   ${Qt6Svg_LIBRARIES}
                                   ${Qt6PrintSupport_LIBRARIES}
                                   ${Qt6DBus_LIBRARIES}
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

elseif(WIN32)
  add_custom_target(
    clean_dist ALL
    ${CMAKE_COMMAND} -E remove_directory "${CMAKE_INSTALL_PREFIX}"
  )

  install(TARGETS featherpad RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
  install(FILES ${HELP_FILE} DESTINATION "${CMAKE_INSTALL_PREFIX}/data")
  install(FILES ${HELP_TR_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/data")
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/win/qt.conf DESTINATION "${CMAKE_INSTALL_PREFIX}/")

  add_custom_target(
    copy_scripts ALL
    ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/win/copy-win-dlls.sh" "${CMAKE_CURRENT_SOURCE_DIR}/win/make_installer.cmd" "${CMAKE_CURRENT_SOURCE_DIR}/win/make-installer.nsi"
    ${CMAKE_BINARY_DIR}/
  )

  install(CODE [[
    execute_process(
      COMMAND
        bash "copy-win-dlls.sh"
      WORKING_DIRECTORY
        ${CMAKE_BINARY_DIR}
    )
    execute_process(
      COMMAND
        "cmd.exe" /c make_installer.cmd
      WORKING_DIRECTORY
        ${CMAKE_BINARY_DIR}
    )
  ]])

endif()
