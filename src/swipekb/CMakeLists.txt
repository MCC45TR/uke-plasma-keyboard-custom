# SPDX-FileCopyrightText: 2026 Destin Moss <https://youcoded.ai>
# SPDX-License-Identifier: MIT
#
# Vendored copy of the MIT swipe engine of the Swipe Keyboard project,
# https://github.com/itsdestin/swipe-keyboard, commit
# f48099755cd1ebeb3f3a9e40f680a683ccc95b2b (2026-10-24). Only engine/src and
# engine/include are copied; the upstream CMakeLists and its GoogleTest-based
# tests are not part of this copy. See README.md in this directory.

add_library(swipekb_engine STATIC
    src/context.cpp
    src/language_engine.cpp
    src/learning.cpp
    src/ngram.cpp
    src/ngram_predictor.cpp
    src/normalize.cpp
    src/nwp_model.cpp
    src/nwp_predictor.cpp
    src/prediction.cpp
    src/spelling.cpp
    src/swipe_beam.cpp
    src/swipe_engine.cpp
    src/swipe_geo.cpp
    src/swipe_layout.cpp
    src/swipe_lm.cpp
    src/swipe_net.cpp
)

target_include_directories(swipekb_engine PUBLIC include)
target_link_libraries(swipekb_engine PUBLIC Qt6::Core)
target_compile_features(swipekb_engine PUBLIC cxx_std_20)

# The keyboard's CMAKE_CXX_FLAGS disable exceptions; the engine relies on them
# for std::optional and Qt containers (the same reason whisper.cpp gets them
# back in src/CMakeLists.txt).
target_compile_options(swipekb_engine PRIVATE -fexceptions)

# Linked into the keyboard executable, and later into the QML plugin.
set_target_properties(swipekb_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Native source generator: no interpreter or network access during compilation.
add_executable(uke-dictionary-tool ../../tools/dictionary-tool.cpp)
target_link_libraries(uke-dictionary-tool PRIVATE Qt::Core)
set(_swipe_dictionary_dir ${CMAKE_CURRENT_BINARY_DIR}/dictionaries)
set(_swipe_dictionary_files)
foreach(_language IN ITEMS en ru)
    set(_output ${_swipe_dictionary_dir}/${_language}.combined)
    add_custom_command(OUTPUT ${_output}
        COMMAND uke-dictionary-tool ${PROJECT_SOURCE_DIR}/src/prediction/data/${_language}.bin ${_output} ${_language}
        DEPENDS uke-dictionary-tool ${PROJECT_SOURCE_DIR}/src/prediction/data/${_language}.bin
        VERBATIM)
    list(APPEND _swipe_dictionary_files ${_output})
endforeach()
add_custom_target(swipe_dictionaries ALL DEPENDS ${_swipe_dictionary_files})
add_dependencies(swipekb_engine swipe_dictionaries)
install(FILES ${_swipe_dictionary_files} model/swipe-model.bin model/NOTICE.md model/FUTO-SWIPE-DATA-LICENSE.txt
    DESTINATION ${KDE_INSTALL_DATADIR}/uke-plasma-keyboard-custom/swipe)
