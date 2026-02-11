cmake_minimum_required(VERSION 3.20)

add_executable(SerializerRoundTrip
    SerializerRoundTrip.cpp
)

target_link_libraries(SerializerRoundTrip PRIVATE
    EnjinEngine
)

target_compile_features(SerializerRoundTrip PRIVATE cxx_std_20)

add_executable(DungeonCrawlerTest
    DungeonCrawlerTest.cpp
)

target_link_libraries(DungeonCrawlerTest PRIVATE
    EnjinEngine
)

target_compile_features(DungeonCrawlerTest PRIVATE cxx_std_20)

add_executable(SMTTemplateTest
    SMTTemplateTest.cpp
)

target_link_libraries(SMTTemplateTest PRIVATE
    EnjinEngine
)

target_compile_features(SMTTemplateTest PRIVATE cxx_std_20)

add_executable(StressTest
    StressTest.cpp
)

target_link_libraries(StressTest PRIVATE
    EnjinEngine
)

target_compile_features(StressTest PRIVATE cxx_std_20)
