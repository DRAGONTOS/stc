cmake_minimum_required(VERSION 3.28.2)

# Get version
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/props.json PROPS)
string(JSON VER GET ${PROPS} version)

# find curl
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
project(stc
  DESCRIPTION "Easily download collections and mods from steam."
  VERSION ${VER})

set(CMAKE_CXX_STANDARD 26)

# check if build type is set if it isnt set it to release
if(NOT CMAKE_BUILD_TYPE)
  set(DCKMAKE_BUILD_TYPE Release)
endif()

# Add the path to the source files for stc
file(GLOB_RECURSE STC_SOURCE_FILES "src/*.cpp")

# add compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-02")

# Add the executable target for stc
add_executable(stc ${STC_SOURCE_FILES})
install(TARGETS stc DESTINATION bin)

target_link_libraries(stc PRIVATE ${CURL_LIBRARIES} ${LIBARCHIVE_LIBRARIES})
target_include_directories(stc PRIVATE ${LIBARCHIVE_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})

# DEBIAN
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "DRAGONTOS")
set(CPACK_DEBIAN_PACKAGE_LICENSE "GPLv3")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION ${DESCRIPTION})
set(CPACK_DEBIAN_PACKAGE_RELEASE_DIST ON)
set(CPACK_GENERATOR DEBIAN)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

# RPM
set(CPACK_PACKAGE_VENDOR "DRAGONTOS")
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
set(CPACK_RPM_PACKAGE_DESCRIPTION ${DESCRIPTION})
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_GENERATOR RPM)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
include(CPack)
