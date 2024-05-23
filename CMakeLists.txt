cmake_minimum_required(VERSION 3.10)
project(TCP_ChatRoom)

set(CMAKE_C_STANDARD 99)

add_executable(server server.c)
target_link_libraries(server ws2_32)

add_executable(client_1 client_1.c)
target_link_libraries(client_1 ws2_32)

add_executable(client_2 client_2.c)
target_link_libraries(client_2 ws2_32)
