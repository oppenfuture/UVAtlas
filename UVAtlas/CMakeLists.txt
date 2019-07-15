file(GLOB SRC ./*.h ./*.hpp ./*.cpp ./geodesics/*.h ./geodesics/*.cpp ./geodesics/*.hpp ./inc/*.h ./isochart/*.h ./isochart/*.hpp ./isochart/*.cpp)
include_directories(. geodesics isochart inc)

add_library(UVAtlas STATIC ${SRC})

if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    target_compile_options(UVAtlas PRIVATE -Wall -w -fdeclspec -Wpedantic -Wextra )
    if (${CMAKE_SIZEOF_VOID_P} EQUAL "4")
        target_compile_options(UVAtlas PRIVATE /arch:SSE2 )
    endif()
endif()
