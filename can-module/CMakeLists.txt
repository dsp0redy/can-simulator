include_directories(inc)

set(APP_NAME can-module)

add_library(${APP_NAME} STATIC
    src/can-send.cpp
    src/can-receive.cpp
)

 find_package(Threads REQUIRED)
 
 target_include_directories(${APP_NAME} PUBLIC
     ${CMAKE_SOURCE_DIR}/can-setup/inc
 )
 target_link_libraries(${APP_NAME} PUBLIC
     can-setup
     Threads::Threads
 )
