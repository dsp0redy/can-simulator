include_directories(inc)

add_executable(${PROJECT_NAME} 
src/main.cpp)

target_link_libraries(${PROJECT_NAME}
can-module
can-setup
db-reader)

target_include_directories(${PROJECT_NAME}  PRIVATE
    ${CMAKE_SOURCE_DIR}/can-setup/inc
    ${CMAKE_SOURCE_DIR}/can-module/inc
)