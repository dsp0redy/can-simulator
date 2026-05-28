include_directories(inc)

set(APP_NAME db-reader)

add_library(${APP_NAME} STATIC
    src/db-reader.cpp
)

target_include_directories(${APP_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)