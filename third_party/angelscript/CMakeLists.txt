# AngelScript scripting library
# Vendored from https://www.angelcode.com/angelscript/ (zlib license)

file(GLOB ANGELSCRIPT_SOURCES
    "source/*.cpp"
)

# Add-on sources
file(GLOB ADDON_SCRIPTSTDSTRING "add_on/scriptstdstring/*.cpp")
file(GLOB ADDON_SCRIPTARRAY     "add_on/scriptarray/*.cpp")
file(GLOB ADDON_SCRIPTMATH      "add_on/scriptmath/*.cpp")
file(GLOB ADDON_CONTEXTMGR      "add_on/contextmgr/*.cpp")
file(GLOB ADDON_SCRIPTBUILDER   "add_on/scriptbuilder/*.cpp")
file(GLOB ADDON_SCRIPTDICTIONARY "add_on/scriptdictionary/*.cpp")
file(GLOB ADDON_SCRIPTHANDLE    "add_on/scripthandle/*.cpp")

# On Windows x64 with MSVC, AngelScript needs an assembly file for native calling conventions
set(ANGELSCRIPT_ASM_SOURCES "")
if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    enable_language(ASM_MASM)
    set(ANGELSCRIPT_ASM_SOURCES "source/as_callfunc_x64_msvc_asm.asm")
endif()

add_library(angelscript STATIC
    ${ANGELSCRIPT_SOURCES}
    ${ANGELSCRIPT_ASM_SOURCES}
    ${ADDON_SCRIPTSTDSTRING}
    ${ADDON_SCRIPTARRAY}
    ${ADDON_SCRIPTMATH}
    ${ADDON_CONTEXTMGR}
    ${ADDON_SCRIPTBUILDER}
    ${ADDON_SCRIPTDICTIONARY}
    ${ADDON_SCRIPTHANDLE}
)

target_include_directories(angelscript PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/add_on
)

target_compile_features(angelscript PUBLIC cxx_std_17)

# Suppress warnings in third-party code
if(MSVC)
    target_compile_options(angelscript PRIVATE /W0)
else()
    target_compile_options(angelscript PRIVATE -w)
endif()

# On 64-bit platforms, AngelScript needs AS_64BIT_PTR
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_compile_definitions(angelscript PRIVATE AS_64BIT_PTR)
endif()
