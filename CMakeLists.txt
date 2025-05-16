cmake_minimum_required(VERSION 3.10)
project(guess_game LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra")

add_executable(guess_the_number
    guess_the_number.c
)

add_executable(us_signals
    us_signals.c
)
target_compile_definitions(us_signals PRIVATE _GNU_SOURCE)
