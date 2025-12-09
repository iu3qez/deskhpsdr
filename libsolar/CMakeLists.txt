cmake_minimum_required(VERSION 3.10)

# libsolar

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBCURL_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
)

add_library(solar STATIC solar.c)

target_link_libraries(solar
    ${LIBCURL_LIBRARIES}
    ${LIBXML2_LIBRARIES}
)
