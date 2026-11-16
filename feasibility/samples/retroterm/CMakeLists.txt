cmake_minimum_required(VERSION 3.13)
project(RetroTerm C)

add_application(RetroTerm
    src/main.c
    retroterm.r
    src/user_events.c
)

set_target_properties(RetroTerm PROPERTIES
    COMPILE_OPTIONS -ffunction-sections
    LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-single"
)
