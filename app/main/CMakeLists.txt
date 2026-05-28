include_directories(inc)

add_executable(${PROJECT_NAME} 
src/main.cpp)

target_link_libraries(${PROJECT_NAME}
db-reader
can-module
process-data)
