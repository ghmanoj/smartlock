cmake_minimum_required(VERSION 3.10.0)

# project name
set (PROJECT_NAME smartlock)
project (${PROJECT_NAME})


add_subdirectory(iot_server)
add_subdirectory(smartlock_device)
