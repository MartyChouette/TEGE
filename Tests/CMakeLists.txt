cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Integration Tests (Tests/Integration/)
# ---------------------------------------------------------------------------

add_executable(SerializerRoundTrip
    Integration/SerializerRoundTrip.cpp
)

target_link_libraries(SerializerRoundTrip PRIVATE
    EnjinEngine
)

target_compile_features(SerializerRoundTrip PRIVATE cxx_std_20)

add_executable(DungeonCrawlerTest
    Integration/DungeonCrawlerTest.cpp
)

target_link_libraries(DungeonCrawlerTest PRIVATE
    EnjinEngine
)

target_compile_features(DungeonCrawlerTest PRIVATE cxx_std_20)

add_executable(SMTTemplateTest
    Integration/SMTTemplateTest.cpp
)

target_link_libraries(SMTTemplateTest PRIVATE
    EnjinEngine
)

target_compile_features(SMTTemplateTest PRIVATE cxx_std_20)

add_executable(StressTest
    Integration/StressTest.cpp
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

# --- Unit Tests (Tests/Unit/) ---
enjin_add_test(TestMath               Unit/TestMath.cpp)
enjin_add_test(TestECS                Unit/TestECS.cpp)
enjin_add_test(TestPhysicsTypes       Unit/TestPhysicsTypes.cpp)
enjin_add_test(TestMemory             Unit/TestMemory.cpp)
enjin_add_test(TestSpline             Unit/TestSpline.cpp)
enjin_add_test(TestProceduralGen      Unit/TestProceduralGen.cpp)
enjin_add_test(TestDialogue           Unit/TestDialogue.cpp)
enjin_add_test(TestAssetPack          Unit/TestAssetPack.cpp)
enjin_add_test(TestPhysics2D          Unit/TestPhysics2D.cpp)
enjin_add_test(TestVisualScript       Unit/TestVisualScript.cpp)
enjin_add_test(TestBehaviorTree       Unit/TestBehaviorTree.cpp)
enjin_add_test(TestUISystem           Unit/TestUISystem.cpp)
enjin_add_test(TestNetworking         Unit/TestNetworking.cpp)
enjin_add_test(TestStressFuzz         Unit/TestStressFuzz.cpp)
enjin_add_test(TestMarketplace        Unit/TestMarketplace.cpp)
enjin_add_test(TestCollisionFiltering Unit/TestCollisionFiltering.cpp)
enjin_add_test(TestSaveSystem         Unit/TestSaveSystem.cpp)

# Register integration tests with CTest
add_test(NAME SerializerRoundTrip COMMAND SerializerRoundTrip)
add_test(NAME DungeonCrawlerTest  COMMAND DungeonCrawlerTest)
add_test(NAME SMTTemplateTest     COMMAND SMTTemplateTest)
add_test(NAME StressTest          COMMAND StressTest)
