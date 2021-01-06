# CMAKELISTS.TXT UNDER util
set(UTIL_SRCS
    "coding.cc"
    "coding.h"
    "comparator.cc"
    "comparator.h"
    "env.cc"
    "env.h"
    "io_report.cc"
    "io_report.h"
    "port_posix.cc"
    "port_posix.h"
    "slice.cc"
    "slice.h"
    "status.cc"
    "status.h"
    "string_util.cc"
    "string_util.h"
    "test_util.cc"
    "test_util.h"
    "threadpool_imp.cc"
    "threadpool_imp.h"
    "trace.cc"
    "trace.h"
)
set_source_files_properties(
    crc32c.cc
    PROPERTIES COMPILE_FLAGS "-msse4.2 -mpclmul")

# Declare the library
add_library(util STATIC
    ${UTIL_SRCS}
)

add_dependencies(util pcm)

# Specify here the include directories exported by this library
target_include_directories(util PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)