##
# @file CMakeLists.txt
# @brief leds-pixel example build
#/

if (CONFIG_ENABLE_LEDS_PIXEL STREQUAL "y" OR CONFIG_ENABLE_QSPI STREQUAL "y")

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)

# APP_INC
set(APP_INC ${APP_PATH}/include)

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

else()
message(FATAL_ERROR "leds-pixel requires CONFIG_ENABLE_LEDS_PIXEL or CONFIG_ENABLE_QSPI")
endif()
