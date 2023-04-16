cmake_minimum_required(VERSION 3.24)
project(task1)

add_executable(receiver common.h pack_buffer.h err.h pack_buffer.c common.c
        receiver.c err.c)
add_executable(sender sender.c err.h common.h err.c common.c)
add_executable(receiver_seq common.h pack_buffer_seq.h err.h pack_buffer_seq.c
        common.c receiver_seq.c err.c)
