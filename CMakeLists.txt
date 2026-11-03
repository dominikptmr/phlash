cmake_minimum_required(VERSION 3.20)
project(workspeed C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# "-" signs ad hoc: fine while running from CLion (TCC attributes permissions to
# CLion), but every rebuild changes the signature. Set this to a self-signed
# code-signing certificate's name before running workspeed on its own.
set(WORKSPEED_SIGN_IDENTITY "-" CACHE STRING "codesign identity for workspeed")

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

add_executable(workspeed
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

target_include_directories(workspeed PRIVATE include)

target_compile_options(workspeed PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
target_link_options(workspeed PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>)

target_link_libraries(workspeed PRIVATE
        iss
        "-framework Carbon"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework ApplicationServices")

add_custom_command(TARGET workspeed POST_BUILD
        COMMAND codesign --force --sign "${WORKSPEED_SIGN_IDENTITY}"
                --identifier workspeed $<TARGET_FILE:workspeed>
        VERBATIM)
