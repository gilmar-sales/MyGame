cmake_minimum_required(VERSION 3.29)
project(MyGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_SCAN_FOR_MODULES 0)

set(FRIGGA_SDK "" CACHE PATH "Frigga SDK or source tree")
if(NOT FRIGGA_SDK AND DEFINED ENV{FRIGGA_SDK} AND NOT "$ENV{FRIGGA_SDK}" STREQUAL "")
  set(FRIGGA_SDK "$ENV{FRIGGA_SDK}" CACHE PATH "Frigga SDK or source tree" FORCE)
endif()
if(NOT FRIGGA_SDK)
  message(FATAL_ERROR "Frigga SDK not found. Configure with -DFRIGGA_SDK=<path>, set FRIGGA_SDK, or use CMakeUserPresets.json.")
endif()
if(NOT EXISTS "${FRIGGA_SDK}/cmake/FriggaSdk.cmake")
  message(FATAL_ERROR "Invalid Frigga SDK: ${FRIGGA_SDK}/cmake/FriggaSdk.cmake not found")
endif()
include("${FRIGGA_SDK}/cmake/FriggaSdk.cmake")

frigga_add_game(MyGame
  NAME "MyGame"
  DISPLAY_NAME "MyGame"
  PUBLISHER "Frigga"
  COPYRIGHT "Copyright © Frigga"
  VERSION "1.0.0"
  IDENTIFIER "com.frigga.mygame"
)

# FRIGGA_MANAGED_MODULE_SUBDIRS_BEGIN
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Modules/charactermovement/CMakeLists.txt")
  add_subdirectory(Modules/charactermovement)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Modules/thirdpersoncamera/CMakeLists.txt")
  add_subdirectory(Modules/thirdpersoncamera)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Modules/pokemon/CMakeLists.txt")
  add_subdirectory(Modules/pokemon)
endif()
# FRIGGA_MANAGED_MODULE_SUBDIRS_END

frigga_install_game(MyGame "MyGame")
