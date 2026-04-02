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

# --- Core ---
enjin_add_test(TestMath               Unit/Core/TestMath.cpp)
enjin_add_test(TestMemory             Unit/Core/TestMemory.cpp)
enjin_add_test(TestSpline             Unit/Core/TestSpline.cpp)
enjin_add_test(TestObjectPool         Unit/Core/TestObjectPool.cpp)
enjin_add_test(TestProfiler           Unit/Core/TestProfiler.cpp)
enjin_add_test(TestLocalization       Unit/Core/TestLocalization.cpp)

# --- ECS ---
enjin_add_test(TestECS                Unit/ECS/TestECS.cpp)
enjin_add_test(TestCollisionFiltering Unit/ECS/TestCollisionFiltering.cpp)
enjin_add_test(TestCamera             Unit/ECS/TestCamera.cpp)
enjin_add_test(TestLOD                Unit/ECS/TestLOD.cpp)
enjin_add_test(TestLightComponent     Unit/ECS/TestLightComponent.cpp)
enjin_add_test(TestMaterial           Unit/ECS/TestMaterial.cpp)
enjin_add_test(TestTransformComponent Unit/ECS/TestTransformComponent.cpp)
enjin_add_test(TestGameplayComponents Unit/ECS/TestGameplayComponents.cpp)

# --- Physics ---
enjin_add_test(TestPhysicsTypes       Unit/Physics/TestPhysicsTypes.cpp)
enjin_add_test(TestPhysics2D          Unit/Physics/TestPhysics2D.cpp)
enjin_add_test(TestPhysicsBackends    Unit/Physics/TestPhysicsBackends.cpp)

# --- Renderer ---
enjin_add_test(TestRTConfig           Unit/Renderer/TestRTConfig.cpp)
enjin_add_test(TestSceneRenderSettings Unit/Renderer/TestSceneRenderSettings.cpp)
enjin_add_test(TestRetroEffects       Unit/Renderer/TestRetroEffects.cpp)
enjin_add_test(TestRendererCamera     Unit/Renderer/TestRendererCamera.cpp)
enjin_add_test(TestPathTracerConfig   Unit/Renderer/TestPathTracerConfig.cpp)
enjin_add_test(TestSceneRenderSettingsExt Unit/Renderer/TestSceneRenderSettingsExt.cpp)

# --- Editor ---
enjin_add_test(TestEditorSettings     Unit/Editor/TestEditorSettings.cpp)
enjin_add_test(TestMarketplace        Unit/Editor/TestMarketplace.cpp)
enjin_add_test(TestTemplateCreator    Unit/Editor/TestTemplateCreator.cpp)
enjin_add_test(TestAccessibility      Unit/Editor/TestAccessibility.cpp)
enjin_add_test(TestPlayMode           Unit/Editor/TestPlayMode.cpp)
enjin_add_test(TestCameraController   Unit/Editor/TestCameraController.cpp)

# --- Scene ---
enjin_add_test(TestSaveSystem         Unit/Scene/TestSaveSystem.cpp)
enjin_add_test(TestSerializerComponents Unit/Scene/TestSceneSerializerComponents.cpp)
enjin_add_test(TestPrefab             Unit/Scene/TestPrefab.cpp)
enjin_add_test(TestSceneManager       Unit/Scene/TestSceneManager.cpp)
enjin_add_test(TestLevelStreaming      Unit/Scene/TestLevelStreaming.cpp)

# --- Scripting ---
enjin_add_test(TestScriptEngine       Unit/Scripting/TestScriptEngine.cpp)
enjin_add_test(TestScriptBindings     Unit/Scripting/TestScriptBindings.cpp)
enjin_add_test(TestVisualScript       Unit/Scripting/TestVisualScript.cpp)

# --- AI ---
enjin_add_test(TestAIBehaviors        Unit/AI/TestAIBehaviors.cpp)
enjin_add_test(TestBehaviorTree       Unit/AI/TestBehaviorTree.cpp)
enjin_add_test(TestNavmesh            Unit/AI/TestNavmesh.cpp)

# --- Audio ---
enjin_add_test(TestAudio              Unit/Audio/TestAudio.cpp)
enjin_add_test(TestAudioTypes         Unit/Audio/TestAudioTypes.cpp)

# --- Effects ---
enjin_add_test(TestWorldTime          Unit/Effects/TestWorldTime.cpp)
enjin_add_test(TestInteractiveWater   Unit/Effects/TestInteractiveWater.cpp)
enjin_add_test(TestReactionDiffusion  Unit/Effects/TestReactionDiffusion.cpp)
enjin_add_test(TestScreenTransition   Unit/Effects/TestScreenTransition.cpp)
enjin_add_test(TestWeather            Unit/Effects/TestWeather.cpp)
enjin_add_test(TestWorldTimeSystem    Unit/Effects/TestWorldTimeSystem.cpp)

# --- GUI ---
enjin_add_test(TestUISystem           Unit/GUI/TestUISystem.cpp)
enjin_add_test(TestDialogue           Unit/GUI/TestDialogue.cpp)
enjin_add_test(TestUICanvas           Unit/GUI/TestUICanvas.cpp)
enjin_add_test(TestDialogueTree       Unit/GUI/TestDialogueTree.cpp)
enjin_add_test(TestUITheme            Unit/GUI/TestUITheme.cpp)

# --- Gameplay ---
enjin_add_test(TestQuestSystem        Unit/Gameplay/TestQuestSystem.cpp)
enjin_add_test(TestCinematic          Unit/Gameplay/TestCinematic.cpp)
enjin_add_test(TestElementalSystem    Unit/Gameplay/TestElementalSystem.cpp)
enjin_add_test(TestInputAction        Unit/Gameplay/TestInputAction.cpp)
enjin_add_test(TestInputActionMap     Unit/Gameplay/TestInputActionMap.cpp)
enjin_add_test(TestRewindSystem       Unit/Gameplay/TestRewindSystem.cpp)

# --- Networking ---
enjin_add_test(TestNetworking         Unit/Networking/TestNetworking.cpp)
enjin_add_test(TestNetworkAdversarial Unit/Networking/TestNetworkAdversarial.cpp)
enjin_add_test(TestNetworkSecurity    Unit/Networking/TestNetworkSecurity.cpp)

# --- Assets ---
enjin_add_test(TestAssetPack          Unit/Assets/TestAssetPack.cpp)
enjin_add_test(TestAssetLoaders       Unit/Assets/TestAssetLoaders.cpp)
enjin_add_test(TestDataAsset          Unit/Assets/TestDataAsset.cpp)
enjin_add_test(TestBuildPipeline      Unit/Assets/TestBuildPipeline.cpp)
enjin_add_test(TestBuildConfig        Unit/Assets/TestBuildConfig.cpp)

# --- Animation ---
enjin_add_test(TestAnimation          Unit/Animation/TestAnimation.cpp)
enjin_add_test(TestTimeline           Unit/Animation/TestTimeline.cpp)
enjin_add_test(TestIKSolver           Unit/Animation/TestIKSolver.cpp)
enjin_add_test(TestSpriteAnimation    Unit/Animation/TestSpriteAnimation.cpp)
enjin_add_test(TestTimelineComponent  Unit/Animation/TestTimelineComponent.cpp)
enjin_add_test(TestBlendTree          Unit/Animation/TestBlendTree.cpp)

# --- Procedural ---
enjin_add_test(TestProceduralGen      Unit/Procedural/TestProceduralGen.cpp)
enjin_add_test(TestProceduralAdvanced Unit/Procedural/TestProceduralAdvanced.cpp)

# --- Stress ---
enjin_add_test(TestStressFuzz         Unit/Stress/TestStressFuzz.cpp)
enjin_add_test(TestHardeningRegression Unit/Stress/TestHardeningRegression.cpp)

# --- Plugin ---
enjin_add_test(TestPluginSystem       Unit/Plugin/TestPluginSystem.cpp)

# Register integration tests with CTest
add_test(NAME SerializerRoundTrip COMMAND SerializerRoundTrip)
add_test(NAME DungeonCrawlerTest  COMMAND DungeonCrawlerTest)
add_test(NAME SMTTemplateTest     COMMAND SMTTemplateTest)
add_test(NAME StressTest          COMMAND StressTest)
