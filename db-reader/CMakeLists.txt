include_directories(inc)

set(APP_NAME db-reader)

add_library(${APP_NAME} STATIC
    src/db-reader.cpp
)