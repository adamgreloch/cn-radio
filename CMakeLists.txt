cmake_minimum_required(VERSION 3.17)
project(sikradio)

add_executable(sikradio-sender sender.c err.h common.h
        opts.h rexmit_set.c rexmit_set.h ctrl_protocol.h ctrl_protocol.c)
add_executable(sikradio-receiver common.h pack_buffer.h err.h pack_buffer.c
        receiver.c opts.h ctrl_protocol.h ctrl_protocol.c)
add_executable(ctrl_protocol_tests ctrl_protocol.h ctrl_protocol.c
        ctrl_protocol_tests.c)
target_link_libraries(sikradio-receiver pthread)
