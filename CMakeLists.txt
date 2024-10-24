cmake_minimum_required(VERSION 3.16)
project(CPP_GAME)

set(CMAKE_CXX_STANDARD 14)

file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
        src/*.cpp
        src/graphicsEngine3D/*.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/dependencies/include
        ${CMAKE_SOURCE_DIR}/dependencies/lib
)

link_directories(
        ${CMAKE_SOURCE_DIR}/dependencies/lib
)

add_executable(CPP_GAME ${SOURCE_FILES})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(CPP_GAME PUBLIC sggd)
else ()
    target_link_libraries(CPP_GAME PUBLIC sgg)
endif ()

set_target_properties(CPP_GAME PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

add_custom_command(TARGET CPP_GAME POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/dependencies/bin $<TARGET_FILE_DIR:CPP_GAME>
)