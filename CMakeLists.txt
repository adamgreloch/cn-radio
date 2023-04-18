cmake_minimum_required(VERSION 3.24)
project(task1)

add_executable(sikradio-sender sender.c err.h common.h err.c common.c
        opts.h)
add_executable(sikradio-receiver common.h pack_buffer.h err.h pack_buffer.c
        common.c receiver.c err.c opts.h)
