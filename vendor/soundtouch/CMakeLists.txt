# Local integration of the unmodified dependency closure recorded in UPSTREAM.json.
add_library(gunblade_soundtouch STATIC
 source/SoundTouch/AAFilter.cpp source/SoundTouch/cpu_detect_x86.cpp
 source/SoundTouch/FIFOSampleBuffer.cpp source/SoundTouch/FIRFilter.cpp
 source/SoundTouch/InterpolateCubic.cpp source/SoundTouch/InterpolateLinear.cpp
 source/SoundTouch/InterpolateShannon.cpp source/SoundTouch/mmx_optimized.cpp
 source/SoundTouch/RateTransposer.cpp source/SoundTouch/SoundTouch.cpp
 source/SoundTouch/sse_optimized.cpp source/SoundTouch/TDStretch.cpp)
target_include_directories(gunblade_soundtouch PUBLIC include)
target_compile_definitions(gunblade_soundtouch PUBLIC SOUNDTOUCH_FLOAT_SAMPLES=1)
set_target_properties(gunblade_soundtouch PROPERTIES POSITION_INDEPENDENT_CODE ON
 CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
 target_compile_options(gunblade_soundtouch PRIVATE -O3 -ffast-math -Wno-unknown-pragmas)
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(armv8.*|aarch64.*|arm64.*)$")
 target_compile_definitions(gunblade_soundtouch PRIVATE SOUNDTOUCH_USE_NEON=1)
endif()
