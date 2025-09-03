cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(l6470_codec_tests)

target_sources(app PRIVATE
    test_l6470_codec.c
)
