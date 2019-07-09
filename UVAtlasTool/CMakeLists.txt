file(GLOB SRC ./*.h ./*.cpp)

include_directories(../UVAtlas/inc)

add_executable(UVAtlasTool ${SRC})
target_link_libraries(UVAtlasTool directxmesh UVAtlas)

if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    target_compile_options(UVAtlasTool PRIVATE -Wall -std=c++11 -w -fdeclspec -Wpedantic -Wextra )
    if (${CMAKE_SIZEOF_VOID_P} EQUAL "4")
        target_compile_options(UVAtlasTool PRIVATE /arch:SSE2 )
    endif()
endif()

install(TARGETS UVAtlasTool DESTINATION /usr/local/bin)

