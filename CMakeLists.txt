cmake_minimum_required(VERSION 3.20)
project(phlash C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# "-" signs ad hoc: fine while running from CLion (TCC attributes permissions to
# CLion), but every rebuild changes the signature. Set this to a self-signed
# code-signing certificate's name before running phlash on its own.
set(PHLASH_SIGN_IDENTITY "-" CACHE STRING "codesign identity for phlash")

# InstantSpaceSwitcher's switching core, vendored unmodified (see its README).
# Built on its own so upstream code keeps upstream's dialect and warnings.
add_library(iss STATIC
        third_party/InstantSpaceSwitcher/ISS.c
        third_party/InstantSpaceSwitcher/event_serialize.c)
set_target_properties(iss PROPERTIES C_EXTENSIONS ON)
target_include_directories(iss PUBLIC third_party/InstantSpaceSwitcher/include)
target_link_libraries(iss PUBLIC
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework ApplicationServices")

# config.example compiled in as default_config[], written to
# ~/.config/phlash/config on first start.
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/default_config.c
        COMMAND xxd -i -n default_config config.example ${CMAKE_CURRENT_BINARY_DIR}/default_config.c
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS config.example
        VERBATIM)

add_executable(phlash
        ${CMAKE_CURRENT_BINARY_DIR}/default_config.c
        src/main.c
        src/tap.c
        src/keys.c
        src/config.c
        src/actions.c
        src/switch.c
        src/spaces.c
        src/windows.c
        src/list.c
        src/apps.c
        src/log.c
        src/proc.c
        src/capslock.c)

target_include_directories(phlash PRIVATE include)

target_compile_options(phlash PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
target_link_options(phlash PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>)

target_link_libraries(phlash PRIVATE
        iss
        "-framework Carbon"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework ApplicationServices")

add_custom_command(TARGET phlash POST_BUILD
        COMMAND codesign --force --sign "${PHLASH_SIGN_IDENTITY}"
                --identifier phlash $<TARGET_FILE:phlash>
        VERBATIM)
