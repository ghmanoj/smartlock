

cmake_minimum_required(VERSION 3.10.0)

set (PROJECT_NAME smartlock_device)
project( ${PROJECT_NAME} )

add_executable( ${PROJECT_NAME} smartlock_device.c smartlock_device.h logger.h )
target_link_libraries( ${PROJECT_NAME} zmq cjson ssl crypto )