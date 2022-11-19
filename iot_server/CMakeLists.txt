

cmake_minimum_required(VERSION 3.10.0)

project( iot_server )

add_executable( iot_server main.c logger.h )
target_link_libraries( iot_server zmq cjson )