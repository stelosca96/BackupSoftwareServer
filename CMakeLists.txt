cmake_minimum_required(VERSION 3.16)
project(ServerBackup)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(ServerBackup main.cpp SyncedFileServer.cpp User.cpp exceptions/socketException.h exceptions/dataException.h exceptions/filesystemException.h Server.cpp Server.h Session.cpp Session.h)
target_link_libraries(${PROJECT_NAME} PUBLIC ssl crypto)
