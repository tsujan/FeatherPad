cmake_minimum_required(VERSION 3.5.0)
project(featherpad)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_definitions(-Wall)

if(WIN32)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/dist/FeatherPad")
elseif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(HAIKU)
    set(CMAKE_INSTALL_PREFIX "/boot/home/config/non-packaged/apps/FeatherPad")
  elseif(APPLE)
    set(CMAKE_INSTALL_PREFIX "/Applications")
  elseif(UNIX)
    set(CMAKE_INSTALL_PREFIX "/usr")
  elseif(OS2)
    set(CMAKE_INSTALL_PREFIX "/@unixroot/usr/")
  endif()
endif()

option(WITHOUT_X11 "Compiling without X11..." OFF)
# OS/2 never has X11
if (OS2)
  option(WITHOUT_X11 "Compiling without X11..." ON)
endif()

add_subdirectory(featherpad)
