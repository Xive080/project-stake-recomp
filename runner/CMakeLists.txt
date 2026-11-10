cmake_minimum_required(VERSION 3.20)
project(ProjectStakeRecomp CXX C)

# Opciones del runtime de PS2Recomp (dependencia externa en third_party/, GPL-3.0)
set(PS2X_BUILD_RECOMP OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_ANALYZER OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_STUDIO OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_FFMPEG OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_SCCACHE OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_DEBUG_UI OFF CACHE BOOL "" FORCE)

get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(PS2R "${ROOT}/third_party/PS2Recomp")
add_subdirectory("${PS2R}" ps2recomp EXCLUDE_FROM_ALL)

# El raiz de PS2Recomp aplica esto solo a sus subdirectorios: repetirlo para el codigo generado
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64|ARM64")
  add_compile_definitions(USE_SSE2NEON)
  include(FetchContent)
  FetchContent_GetProperties(sse2neon SOURCE_DIR SSE2NEON_DIR)
  include_directories("${SSE2NEON_DIR}")
endif()

set(GEN "${ROOT}/work")

# Programa principal
file(GLOB MAIN_SRC CONFIGURE_DEPENDS "${GEN}/output2/*.cpp")
# Cada modulo tiene sus propias cabeceras generadas: biblioteca de objetos por modulo
set(OV_OBJS)
file(GLOB OV_DIRS LIST_DIRECTORIES true "${GEN}/ov/*")
foreach(d ${OV_DIRS})
  if(IS_DIRECTORY "${d}/output")
    get_filename_component(m "${d}" NAME)
    file(GLOB s CONFIGURE_DEPENDS "${d}/output/*.cpp")
    add_library(ov_${m} OBJECT ${s})
    target_include_directories(ov_${m} PRIVATE "${d}/output")
    target_link_libraries(ov_${m} PRIVATE ps2_runtime)
    list(APPEND OV_OBJS $<TARGET_OBJECTS:ov_${m}>)
  endif()
endforeach()

add_executable(stake_recomp
  "${PS2R}/ps2xRuntime/src/main.cpp"
  ${MAIN_SRC}
  "${GEN}/overlay_registry.cpp"
  ${OV_OBJS})
target_include_directories(stake_recomp PRIVATE "${GEN}/output2")
target_link_libraries(stake_recomp PRIVATE ps2_runtime)
