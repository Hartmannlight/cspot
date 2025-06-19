cmake_minimum_required(VERSION 3.16)
project(yajl)

set(YAJL_SRCS
    src/yajl_alloc.c
    src/yajl_buf.c
    src/yajl_encode.c
    src/yajl_gen.c
    src/yajl_lex.c
    src/yajl_parser.c
    src/yajl_tree.c
    src/yajl_version.c
    src/yajl.c
)
# Add the library target
add_library(yajl STATIC ${YAJL_SRCS})
target_include_directories(yajl PRIVATE "src" PUBLIC "src/yajl")
