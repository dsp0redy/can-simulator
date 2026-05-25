include_directories(inc)

set(APP_NAME can-module)

add_library(${APP_NAME} STATIC
    src/can-send.cpp
    src/can-receive.cpp
)

target_include_directories(${APP_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/can-setup/inc
)
