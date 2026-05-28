include_directories(inc)

set(APP_NAME can-module)

add_library(${APP_NAME} STATIC
    src/can-setup.cpp
    src/can-receive.cpp
    src/can-send.cpp
)

target_include_directories(${APP_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)