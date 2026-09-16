cmake_minimum_required(VERSION 3.21)
project(RE3MouseFix VERSION 1.0.0 LANGUAGES CXX)

add_library(RE3MouseFix SHARED src/plugin.cpp)
target_compile_features(RE3MouseFix PRIVATE cxx_std_20)
target_include_directories(RE3MouseFix SYSTEM PRIVATE third_party/reframework)
target_compile_definitions(RE3MouseFix PRIVATE WIN32_LEAN_AND_MEAN NDEBUG)

if(MSVC)
    target_compile_options(RE3MouseFix PRIVATE /W4 /permissive- /Zc:preprocessor /Brepro)
    target_link_options(RE3MouseFix PRIVATE /OPT:REF /OPT:ICF /Brepro /NOIMPLIB)
endif()
