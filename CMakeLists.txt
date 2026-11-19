cmake_minimum_required(VERSION 3.22)
project(RELINKKickEnhancer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

juce_add_plugin(RELINKKickEnhancer
  COMPANY_NAME "RELINK"
  BUNDLE_ID "com.relink.kickenhancer"
  PLUGIN_MANUFACTURER_CODE Rlnk
  PLUGIN_CODE RkE1
  FORMATS VST3
  PRODUCT_NAME "RELINK Kick Enhancer")

target_sources(RELINKKickEnhancer PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(RELINKKickEnhancer PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(RELINKKickEnhancer PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
