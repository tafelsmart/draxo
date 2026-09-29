cmake_minimum_required(VERSION 3.20)
project(DraxoClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── MinHook (local) ─────────────────────────────────────────────────────
add_subdirectory(minhook-master)

# ── ImGui sources ────────────────────────────────────────────────────────
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

# ── Project sources ──────────────────────────────────────────────────────
# CLIENT_SRC_DIR can be overridden (e.g. by tools/vanilla_builder.py) to
# compile from a translated copy of the source tree (obfuscated names).
if(NOT DEFINED CLIENT_SRC_DIR)
    set(CLIENT_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
endif()
file(GLOB_RECURSE PROJECT_SOURCES "${CLIENT_SRC_DIR}/*.cpp")

# ── DLL target ───────────────────────────────────────────────────────────
add_library(draxo SHARED ${PROJECT_SOURCES} ${IMGUI_SOURCES})

target_include_directories(draxo PRIVATE
    ${CLIENT_SRC_DIR}
    ${CLIENT_SRC_DIR}/jni_headers
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(draxo PRIVATE
    minhook
    opengl32
    gdiplus
)

target_compile_definitions(draxo PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

# Disable MSVC warnings for external code
target_compile_options(draxo PRIVATE /W3 /wd4005 /wd4996 /GR-)

# ── Integrity patch (post-build) ───────────────────────────────────
add_custom_command(TARGET draxo POST_BUILD
    COMMAND python "${CMAKE_SOURCE_DIR}/tools/patch_integrity.py"
        "$<TARGET_FILE:draxo>"
    COMMENT "Patching DLL integrity hash...")

# ── Logo + Version neben die DLL kopieren ──────────────────────────────────
set(DRAXO_LOGO_SRC "${CMAKE_SOURCE_DIR}/Logo.png")
if(EXISTS "${DRAXO_LOGO_SRC}")
    add_custom_command(TARGET draxo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DRAXO_LOGO_SRC}"
            "$<TARGET_FILE_DIR:draxo>/draxo_logo.png")
endif()

set(DRAXO_VERSION_SRC "${CMAKE_SOURCE_DIR}/VERSION")
if(EXISTS "${DRAXO_VERSION_SRC}")
    add_custom_command(TARGET draxo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DRAXO_VERSION_SRC}"
            "$<TARGET_FILE_DIR:draxo>/VERSION")
endif()

# ── Unit Test executable (standalone, no Minecraft needed) ────────────
# Links against the SAME config.cpp used by draxo.dll (translated or original)
# so it always tests the actual Config implementation.
set(CONFIG_SOURCES
    ${CLIENT_SRC_DIR}/core/config.cpp
    ${CLIENT_SRC_DIR}/core/log.cpp
    ${CLIENT_SRC_DIR}/core/auth.cpp
)
add_executable(test_client tools/test_client.cpp ${CONFIG_SOURCES})
target_include_directories(test_client PRIVATE
    ${CLIENT_SRC_DIR}
    ${CLIENT_SRC_DIR}/jni_headers
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    minhook-master/include
)
target_link_libraries(test_client PRIVATE minhook)
target_compile_definitions(test_client PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
)
target_compile_options(test_client PRIVATE /W3 /wd4005 /wd4996 /GR-)

# ── Fonts neben die DLL kopieren ────────────────────────────────────────
set(DRAXO_FONTS_DIR "${CMAKE_SOURCE_DIR}/assets/fonts")
if(EXISTS "${DRAXO_FONTS_DIR}")
    add_custom_command(TARGET draxo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:draxo>/assets/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DRAXO_FONTS_DIR}/Inter-Regular.ttf"
            "$<TARGET_FILE_DIR:draxo>/assets/fonts/Inter-Regular.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DRAXO_FONTS_DIR}/Inter-Bold.ttf"
            "$<TARGET_FILE_DIR:draxo>/assets/fonts/Inter-Bold.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DRAXO_FONTS_DIR}/JetBrainsMono-Regular.ttf"
            "$<TARGET_FILE_DIR:draxo>/assets/fonts/JetBrainsMono-Regular.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DRAXO_FONTS_DIR}/JetBrainsMono-Bold.ttf"
            "$<TARGET_FILE_DIR:draxo>/assets/fonts/JetBrainsMono-Bold.ttf")
endif()
