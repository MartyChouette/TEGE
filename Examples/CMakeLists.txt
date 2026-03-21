cmake_minimum_required(VERSION 3.20)

# Example: Triangle (raw Vulkan + ECS boilerplate)
add_executable(ExampleTriangle
    Triangle/main.cpp
)
target_link_libraries(ExampleTriangle PRIVATE EnjinEngine EnjinCore)
target_compile_features(ExampleTriangle PUBLIC cxx_std_20)

# Example: Simple3D (SimpleApp facade — minimal 3D scene)
add_executable(ExampleSimple3D
    Simple3D/main.cpp
)
target_link_libraries(ExampleSimple3D PRIVATE EnjinEngine EnjinCore)
target_compile_features(ExampleSimple3D PUBLIC cxx_std_20)

# Example: Simple2D (SimpleApp2D facade — minimal 2D scene)
add_executable(ExampleSimple2D
    Simple2D/main.cpp
)
target_link_libraries(ExampleSimple2D PRIVATE EnjinEngine EnjinCore)
target_compile_features(ExampleSimple2D PUBLIC cxx_std_20)
