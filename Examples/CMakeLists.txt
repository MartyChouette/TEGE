cmake_minimum_required(VERSION 3.20)

# Examples use Enjin's EntryPoint.h, which provides WinMain on Windows.
# WIN32 marks these as GUI subsystem apps so the linker looks for WinMain
# instead of main. (No console window pops up — same as the Editor/Player.)
set(ENJIN_EXAMPLE_TYPE "")
if(WIN32)
    set(ENJIN_EXAMPLE_TYPE WIN32)
endif()

# Example: Triangle (raw Vulkan + ECS boilerplate)
add_executable(ExampleTriangle ${ENJIN_EXAMPLE_TYPE}
    Triangle/main.cpp
)
target_link_libraries(ExampleTriangle PRIVATE EnjinEngine EnjinCore)
target_compile_features(ExampleTriangle PUBLIC cxx_std_20)

# Example: Simple3D (Enjin::App facade — minimal 3D scene)
add_executable(ExampleSimple3D ${ENJIN_EXAMPLE_TYPE}
    Simple3D/main.cpp
)
target_link_libraries(ExampleSimple3D PRIVATE EnjinEngine EnjinCore)
target_compile_features(ExampleSimple3D PUBLIC cxx_std_20)

# Example: OrbCollector (C++ tier of the three-workflow demo — mirrors the
# "Components Only" and "Script Only" editor templates)
add_executable(ExampleOrbCollector ${ENJIN_EXAMPLE_TYPE}
    OrbCollector/main.cpp
)
target_link_libraries(ExampleOrbCollector PRIVATE EnjinEngine EnjinCore)
target_compile_features(ExampleOrbCollector PUBLIC cxx_std_20)

# Example: Simple2D (Enjin::App2D facade — minimal 2D scene)
add_executable(ExampleSimple2D ${ENJIN_EXAMPLE_TYPE}
    Simple2D/main.cpp
)
target_link_libraries(ExampleSimple2D PRIVATE EnjinEngine EnjinCore)
target_compile_features(ExampleSimple2D PUBLIC cxx_std_20)
