cmake_minimum_required(VERSION 3.10)
project(guess_the_number LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra")

# Задание 1 (с сигналами)
add_executable(guess_the_number 
    guess_the_number.c
)
target_compile_definitions(guess_signal PRIVATE _GNU_SOURCE)

# Задание 2 (с каналами)
add_executable(us_signals
    us_signals.c
)
