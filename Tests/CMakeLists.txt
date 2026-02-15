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

# ---------------------------------------------------------------------------
# Unit Test Framework
# ---------------------------------------------------------------------------
enable_testing()

set(ENJIN_TEST_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/Framework)

function(enjin_add_test NAME SRC)
    add_executable(${NAME} ${SRC})
    target_link_libraries(${NAME} PRIVATE EnjinEngine)
    target_compile_features(${NAME} PRIVATE cxx_std_20)
    target_include_directories(${NAME} PRIVATE ${ENJIN_TEST_INCLUDE})
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

enjin_add_test(TestMath         Unit/TestMath.cpp)
enjin_add_test(TestECS          Unit/TestECS.cpp)
enjin_add_test(TestPhysicsTypes Unit/TestPhysicsTypes.cpp)
enjin_add_test(TestMemory       Unit/TestMemory.cpp)

# Register existing integration tests with CTest
add_test(NAME SerializerRoundTrip COMMAND SerializerRoundTrip)
add_test(NAME DungeonCrawlerTest  COMMAND DungeonCrawlerTest)
add_test(NAME SMTTemplateTest     COMMAND SMTTemplateTest)
add_test(NAME StressTest          COMMAND StressTest)
