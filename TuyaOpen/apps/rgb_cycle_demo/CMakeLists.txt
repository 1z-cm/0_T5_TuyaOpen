##
# @file CMakeLists.txt
# @brief Standalone P25 addressable RGB LED demo.
#/

set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

aux_source_directory(${APP_PATH}/src APP_SRCS)

add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
)

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_PATH}/include
)
