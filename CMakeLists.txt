cmake_minimum_required(VERSION 3.16)
project(ServerBackup)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(ServerBackup main.cpp Socket.cpp SyncedFileServer.cpp UploadJobs.cpp ServerSocket.cpp Jobs.cpp User.cpp exceptions/socketException.h exceptions/dataException.h exceptions/filesystemException.h)
target_link_libraries(${PROJECT_NAME} PUBLIC ssl crypto)
