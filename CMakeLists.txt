cmake_minimum_required(VERSION 3.24)
project(task1)

add_executable(sikradio-sender sender.c err.h common.h
        opts.h)
add_executable(sikradio-receiver common.h pack_buffer.h err.h pack_buffer.c
        receiver.c opts.h)
add_executable(pack_buffer_test common.h pack_buffer.h err.h
        pack_buffer.c pack_buffer_test.c opts.h)
