cmake_minimum_required(VERSION 3.24)
project(task1)

add_executable(receiver receiver.c err.h common.h)
add_executable(sender sender.c err.h common.h)
