cmake_minimum_required(VERSION 3.16)
project(CPP_GAME)

# Import the PowerShell script
include(${CMAKE_SOURCE_DIR}/CMakeScripts/PowerShell.cmake)

set(CMAKE_CXX_STANDARD 17)
set(DEP_LIB ${CMAKE_SOURCE_DIR}/dependencies/lib)
set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
set(APP_ICON "${CMAKE_SOURCE_DIR}/assets/sprites/logo.ico")

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
        src/*.h
        src/*.cpp
)

file(GLOB SHADER_FILES "${ASSETS_DIR}/shaders/*")
file(GLOB SPRITE_FILES "${ASSETS_DIR}/sprites/*")

include_directories(
        ${CMAKE_SOURCE_DIR}/dependencies/include
        ${CMAKE_SOURCE_DIR}/dependencies/lib
)

link_directories(
        ${CMAKE_SOURCE_DIR}/dependencies/lib
)

set(LIBS
        ${DEP_LIB}/SDL2_mixer.lib
        ${DEP_LIB}/SDL2main.lib
        ${DEP_LIB}/SDL2test.lib
        ${DEP_LIB}/glew32.lib
        ${DEP_LIB}/glew32s.lib
        ${DEP_LIB}/SDL2.lib
        ${DEP_LIB}/freetype.lib
        ${DEP_LIB}/OpenGL32.lib
)

add_executable(CPP_GAME ${SOURCE_FILES} ${APP_ICON})

add_custom_target(Assets ALL
        DEPENDS ${SHADER_FILES} ${SPRITE_FILES}
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND LIBS ${DEP_LIB}/sggd.lib)
    target_link_libraries(${PROJECT_NAME} PUBLIC sggd)
else ()
    list(APPEND LIBS ${DEP_LIB}/sgg.lib)
    target_link_libraries(${PROJECT_NAME} PUBLIC sgg)
endif ()

# Set the output directory for the executable directly to 'game'
set_target_properties(CPP_GAME PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/game")

set_target_properties(CPP_GAME PROPERTIES
        LINK_FLAGS "/ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS")

add_custom_command(TARGET CPP_GAME POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/dependencies/bin $<TARGET_FILE_DIR:CPP_GAME>

        # Copy assets folder directly to the build directory
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets ${CMAKE_SOURCE_DIR}/build/assets
)

# Use the execute_powershell command to create the shortcut with output and result variables
execute_powershell(
        "$ErrorActionPreference = 'Stop';
        $s=(New-Object -COMObject WScript.Shell).CreateShortcut('${CMAKE_SOURCE_DIR}/build/CPP_GAME.lnk');
        $s.TargetPath='${CMAKE_SOURCE_DIR}/build/game/CPP_GAME.exe';
        $s.Save()"
        OUTPUT_VARIABLE PS_OUTPUT
        RESULT_VARIABLE PS_RESULT
)

# Use another execute_powershell command to set the icon for the shortcut
execute_powershell(
        "$ErrorActionPreference = 'Stop';
        $shortcutPath='${CMAKE_SOURCE_DIR}/build/CPP_GAME.lnk';
        $iconPath='${CMAKE_SOURCE_DIR}/assets/sprites/logo.ico';
        $s=(New-Object -COMObject WScript.Shell).CreateShortcut($shortcutPath);
        $s.IconLocation=$iconPath;
        $s.Save()"
        OUTPUT_VARIABLE ICON_OUTPUT
        RESULT_VARIABLE ICON_RESULT
)
