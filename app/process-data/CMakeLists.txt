include_directories(inc)

set(APP_NAME process-data)

add_library(${APP_NAME} STATIC
    src/process-data.cpp
)

target_include_directories(${APP_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/db-reader/inc
)