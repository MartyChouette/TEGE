# nanosvg - header-only SVG parsing and rasterization library
# https://github.com/memononen/nanosvg
# License: zlib

add_library(nanosvg INTERFACE)
target_include_directories(nanosvg INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
