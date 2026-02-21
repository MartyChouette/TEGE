cmake_minimum_required(VERSION 3.20)

if(ENJIN_PLATFORM_WEB)
    add_executable(EnjinPlayer
        src/web_main.cpp
    )
else()
    add_executable(EnjinPlayer
        src/main.cpp
    )
endif()

target_link_libraries(EnjinPlayer PRIVATE
    EnjinEngine
    EnjinCore
)

target_compile_features(EnjinPlayer PRIVATE cxx_std_20)

target_compile_definitions(EnjinPlayer PRIVATE ENJIN_PLAYER=1)

# Copy player executable to bin directory alongside editor
set_target_properties(EnjinPlayer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
