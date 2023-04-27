cmake_minimum_required(VERSION 3.17)
project(task1)

add_executable(sikradio-sender sender.c err.h common.h
        opts.h)
add_executable(sikradio-receiver common.h pack_buffer.h err.h pack_buffer.c
        receiver.c opts.h)
target_link_libraries(sikradio-receiver pthread)
