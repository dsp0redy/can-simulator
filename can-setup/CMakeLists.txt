include_directories(inc)

set(APP_NAME can-setup)

add_library(${APP_NAME} STATIC
    src/can-setup.cpp
)

# target_link_libraries(${APP_NAME}
#     hw_grpc_proto
#     dl
# )
