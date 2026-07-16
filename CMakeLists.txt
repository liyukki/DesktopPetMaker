cmake_minimum_required(VERSION 3.22)
project(pro VERSION 1.0.0 LANGUAGES CXX)

option(DESKTOP_PET_ENABLE_LIVE2D "Enable the optional Live2D/Cubism adapter interface" OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Test)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)
include(CTest)

# Source layout
# -------------
# Keep these groups aligned with docs/SOURCE_FILE_INDEX.md. Grouping the files
# here makes IDE project trees mirror the on-disk architecture.
set(APP_SOURCES
    src/app/main.cpp
    src/app/petcontrolcenterwindow.h
    src/app/petcontrolcenterwindow.cpp
    src/app/systemtraycontroller.h
    src/app/systemtraycontroller.cpp
)

set(PROJECT_SOURCES
    src/project/petproject.h
    src/project/petproject.cpp
    src/project/petprojectregistry.h
    src/project/petprojectregistry.cpp
    src/project/archivecommandrunner.h
    src/project/archivecommandrunner.cpp
)

set(RUNTIME_SOURCES
    src/runtime/runtimepetwindow.h
    src/runtime/runtimepetwindow.cpp
    src/runtime/runtimepetmanager.h
    src/runtime/runtimepetmanager.cpp
    src/runtime/petruntimeinstance.h
    src/runtime/petruntimeinstance.cpp
    src/runtime/renderbackend.h
    src/runtime/renderbackend.cpp
    src/runtime/screenplacementutil.h
    src/runtime/screenplacementutil.cpp
    src/runtime/petspeechbubblewindow.h
    src/runtime/petspeechbubblewindow.cpp
    src/runtime/runtimeactionresult.h
    src/runtime/petbehaviorrule.h
)

set(AI_CORE_SOURCES
    src/ai/core/aiprovider.h
    src/ai/core/aiprovider.cpp
    src/ai/core/aiproviderprofileregistry.h
    src/ai/core/aiproviderprofileregistry.cpp
    src/ai/core/aiproviderprofileservice.h
    src/ai/core/aiproviderprofileservice.cpp
    src/ai/core/credentialstore.h
    src/ai/core/credentialstore.cpp
    src/ai/core/petairequestcoordinator.h
    src/ai/core/petairequestcoordinator.cpp
    src/ai/core/aiactiondescriptor.h
    src/ai/core/aiactionvalidator.h
    src/ai/core/aiactionvalidator.cpp
    src/ai/core/chathistoryentry.h
    src/ai/core/petconversationcontext.h
)

set(AI_CHAT_SOURCES
    src/ai/chat/aiconversationroom.h
    src/ai/chat/aiconversationroom.cpp
    src/ai/chat/aiconversationroomrepository.h
    src/ai/chat/aiconversationroomrepository.cpp
    src/ai/chat/aiconversationroommanager.h
    src/ai/chat/aiconversationroommanager.cpp
)

set(AI_UI_SOURCES
    src/ai/ui/aidialogwindow.h
    src/ai/ui/aidialogwindow.cpp
    src/ai/ui/aiconversationconsolewindow.h
    src/ai/ui/aiconversationconsolewindow.cpp
)

set(EDITOR_SOURCES
    src/editor/editorwindow.h
    src/editor/editorwindow.cpp
    src/editor/previewcanvas.h
    src/editor/previewcanvas.cpp
    src/editor/actionmaterialwindow.h
    src/editor/actionmaterialwindow.cpp
    src/editor/alphaboundingboxutil.h
    src/editor/alphaboundingboxutil.cpp
    src/editor/assetqualityanalyzer.h
    src/editor/assetqualityanalyzer.cpp
    src/editor/proceduralmotiongenerator.h
    src/editor/proceduralmotiongenerator.cpp
    src/editor/motionpromptlibrary.h
    src/editor/motionpromptlibrary.cpp
    src/editor/aiassetpromptwizard.h
    src/editor/aiassetpromptwizard.cpp
)

set(IMPORT_SOURCES
    src/import/spritesheetslicer.h
    src/import/spritesheetslicer.cpp
    src/import/spritesheetimportdialog.h
    src/import/spritesheetimportdialog.cpp
    src/import/shimejiimportwizard.h
    src/import/shimejiimportwizard.cpp
)

set(JOURNAL_SOURCES
    src/journal/moodjournal.h
    src/journal/moodjournal.cpp
    src/journal/journalwindow.h
    src/journal/journalwindow.cpp
)

set(INTEGRATION_SOURCES
    src/integrations/toolintegrationmanager.h
    src/integrations/toolintegrationmanager.cpp
)

set(THEME_SOURCES
    src/ui/theme/apptheme.h
    src/ui/theme/apptheme.cpp
    src/ui/theme/themeconstants.h
    src/ui/theme/iconprovider.h
    src/ui/theme/iconprovider.cpp
)

set(DESKTOP_PET_LIBRARY_SOURCES
    ${PROJECT_SOURCES}
    ${RUNTIME_SOURCES}
    ${AI_CORE_SOURCES}
    ${AI_CHAT_SOURCES}
    ${AI_UI_SOURCES}
    ${EDITOR_SOURCES}
    ${IMPORT_SOURCES}
    ${JOURNAL_SOURCES}
    ${INTEGRATION_SOURCES}
    ${THEME_SOURCES}
    resources.qrc
)

set(DESKTOP_PET_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/project
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai/chat
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor
    ${CMAKE_CURRENT_SOURCE_DIR}/src/import
    ${CMAKE_CURRENT_SOURCE_DIR}/src/journal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/theme
)

add_library(test_support STATIC
    tests/test_support.h
    tests/test_support.cpp
)
target_include_directories(test_support PUBLIC ${DESKTOP_PET_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(test_support PUBLIC Qt6::Widgets Qt6::Test)
set_target_properties(test_support PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

function(register_desktop_pet_test target)
    qt_add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE ${DESKTOP_PET_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(${target} PRIVATE test_support Qt6::Widgets Qt6::Network Qt6::Test)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    if(WIN32)
        target_link_libraries(${target} PRIVATE Advapi32)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    if(WIN32)
        set_tests_properties(${target} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=windows"
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    else()
        set_tests_properties(${target} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endfunction()

set(PETPROJECT_TEST_SOURCES
    src/project/petproject.h
    src/project/petproject.cpp
    src/project/archivecommandrunner.h
    src/project/archivecommandrunner.cpp
)

register_desktop_pet_test(core_project_tests
    tests/core_project_tests.cpp
    ${PETPROJECT_TEST_SOURCES}
)
register_desktop_pet_test(asset_transaction_tests
    tests/asset_transaction_tests.cpp
    ${PETPROJECT_TEST_SOURCES}
    src/editor/proceduralmotiongenerator.h
    src/editor/proceduralmotiongenerator.cpp
)
register_desktop_pet_test(sprite_sheet_tests
    tests/sprite_sheet_tests.cpp
    src/import/spritesheetslicer.h
    src/import/spritesheetslicer.cpp
)
register_desktop_pet_test(shimeji_transaction_tests
    tests/shimeji_transaction_tests.cpp
    src/import/shimejiimportwizard.h
    src/import/shimejiimportwizard.cpp
    ${PETPROJECT_TEST_SOURCES}
)
register_desktop_pet_test(petpack_security_tests
    tests/petpack_security_tests.cpp
    ${PETPROJECT_TEST_SOURCES}
)
register_desktop_pet_test(provider_credential_tests
    tests/provider_credential_tests.cpp
    src/ai/core/aiproviderprofileservice.h
    src/ai/core/aiproviderprofileservice.cpp
    src/ai/core/aiproviderprofileregistry.h
    src/ai/core/aiproviderprofileregistry.cpp
    src/ai/core/credentialstore.h
    src/ai/core/credentialstore.cpp
)
register_desktop_pet_test(credential_store_windows_integration_tests
    tests/credential_store_windows_integration_tests.cpp
    src/ai/core/credentialstore.h
    src/ai/core/credentialstore.cpp
)
register_desktop_pet_test(multi_ai_room_tests
    tests/multi_ai_room_tests.cpp
    ${AI_CHAT_SOURCES}
    ${AI_CORE_SOURCES}
    ${PETPROJECT_TEST_SOURCES}
)
register_desktop_pet_test(multi_ai_lifecycle_tests
    tests/multi_ai_lifecycle_tests.cpp
    ${AI_CHAT_SOURCES}
    ${AI_CORE_SOURCES}
    ${PETPROJECT_TEST_SOURCES}
)
register_desktop_pet_test(formal_asset_tests
    tests/formal_asset_tests.cpp
    ${PETPROJECT_TEST_SOURCES}
)
register_desktop_pet_test(gui_widget_tests
    tests/gui_widget_tests.cpp
    src/import/spritesheetimportdialog.h
    src/import/spritesheetimportdialog.cpp
    src/import/spritesheetslicer.h
    src/import/spritesheetslicer.cpp
    ${THEME_SOURCES}
)
register_desktop_pet_test(ui_theme_tests
    tests/ui_theme_tests.cpp
    ${THEME_SOURCES}
)
register_desktop_pet_test(runtime_action_tests
    tests/runtime_action_tests.cpp
    ${RUNTIME_SOURCES}
    ${PROJECT_SOURCES}
    ${AI_CORE_SOURCES}
    src/ai/ui/aidialogwindow.h
    src/ai/ui/aidialogwindow.cpp
    ${JOURNAL_SOURCES}
    ${THEME_SOURCES}
)
target_compile_definitions(runtime_action_tests PRIVATE DESKTOP_PET_TESTING DESKTOP_PET_ENABLE_LIVE2D=0)

qt_add_executable(pro
    WIN32
    ${APP_SOURCES}
    ${DESKTOP_PET_LIBRARY_SOURCES}
)
target_include_directories(pro PRIVATE ${DESKTOP_PET_INCLUDE_DIRS})
if(WIN32)
    target_sources(pro PRIVATE desktop_pet.rc)
endif()

set_target_properties(pro PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_link_libraries(pro PRIVATE Qt6::Widgets Qt6::Network)
target_compile_definitions(pro PRIVATE DESKTOP_PET_ENABLE_LIVE2D=$<BOOL:${DESKTOP_PET_ENABLE_LIVE2D}>)

if(WIN32)
    target_link_libraries(pro PRIVATE Advapi32)
endif()
if(MSVC)
    target_compile_options(pro PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(pro PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_executable(platform_smoke_tests
    tests/platform_smoke_tests.cpp
    ${AI_CHAT_SOURCES}
    ${AI_CORE_SOURCES}
    ${INTEGRATION_SOURCES}
    src/editor/assetqualityanalyzer.h
    src/editor/assetqualityanalyzer.cpp
    src/editor/alphaboundingboxutil.h
    src/editor/alphaboundingboxutil.cpp
    src/editor/proceduralmotiongenerator.h
    src/editor/proceduralmotiongenerator.cpp
    src/editor/motionpromptlibrary.h
    src/editor/motionpromptlibrary.cpp
    src/import/shimejiimportwizard.h
    src/import/shimejiimportwizard.cpp
    src/import/spritesheetslicer.h
    src/import/spritesheetslicer.cpp
    ${PETPROJECT_TEST_SOURCES}
)
target_include_directories(platform_smoke_tests PRIVATE ${DESKTOP_PET_INCLUDE_DIRS})
set_target_properties(platform_smoke_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(platform_smoke_tests PRIVATE Qt6::Widgets Qt6::Network)
if(WIN32)
    target_link_libraries(platform_smoke_tests PRIVATE Advapi32)
endif()
if(MSVC)
    target_compile_options(platform_smoke_tests PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(platform_smoke_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME platform_smoke_tests COMMAND platform_smoke_tests)
if(WIN32)
    set_tests_properties(platform_smoke_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=windows"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
endif()

add_test(NAME check_text_encoding_self_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_text_encoding.py --self-test)
add_test(NAME check_text_encoding
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_text_encoding.py --root ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME check_cmake_source_completeness
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_cmake_source_completeness.py ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME standard_zip_self_test
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/create_standard_zip.py --self-test)
add_custom_target(update_source_manifest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/create_source_manifest.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Updating SOURCE_MANIFEST_SHA256.txt")
add_test(NAME verify_source_manifest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_source_manifest.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}
            --manifest ${CMAKE_CURRENT_SOURCE_DIR}/SOURCE_MANIFEST_SHA256.txt)
add_test(NAME manifest_gate_negative_tests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_manifest_gate.py)
add_test(NAME validate_release_pets
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_release_pets.py)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
        REQUIRED
    )
    add_custom_command(TARGET pro POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --compiler-runtime "$<TARGET_FILE:pro>"
        COMMENT "Deploying Qt runtime dependencies"
        VERBATIM
    )
endif()

qt_add_executable(platform_gui_tests
    tests/platform_gui_tests.cpp
    ${DESKTOP_PET_LIBRARY_SOURCES}
    src/app/petcontrolcenterwindow.h
    src/app/petcontrolcenterwindow.cpp
    src/app/systemtraycontroller.h
    src/app/systemtraycontroller.cpp
)
target_include_directories(platform_gui_tests PRIVATE ${DESKTOP_PET_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(platform_gui_tests PRIVATE test_support Qt6::Widgets Qt6::Network Qt6::Test)
target_compile_definitions(platform_gui_tests PRIVATE DESKTOP_PET_TESTING DESKTOP_PET_ENABLE_LIVE2D=0)
set_target_properties(platform_gui_tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(WIN32)
    target_link_libraries(platform_gui_tests PRIVATE Advapi32)
endif()
if(MSVC)
    target_compile_options(platform_gui_tests PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(platform_gui_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

foreach(gui_case IN ITEMS
        controlCenterUsesRealNavigationAndSettingsClicks
        multiAiConsoleCreatesAndRenamesRoomThroughDialogs
        actionMaterialRejectsInvalidSchemaFromRealButton
        runtimeBubbleAndRuntimeOptionsUseRealWidgets
        systemTrayMenuProvidesRecoveryActions
        uiVisualSnapshotsAndDpiChecks)
    add_test(NAME ${gui_case} COMMAND platform_gui_tests ${gui_case})
    if(WIN32)
        set_tests_properties(${gui_case} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=windows"
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    else()
        set_tests_properties(${gui_case} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endforeach()
set_tests_properties(uiVisualSnapshotsAndDpiChecks PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;DESKTOP_PET_UI_SNAPSHOT_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/ui-snapshots")

add_test(NAME multi_ai_runtime_integration_tests COMMAND platform_gui_tests multiAiRuntimeIntegrationTests)
if(WIN32)
    set_tests_properties(multi_ai_runtime_integration_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=windows"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
else()
    set_tests_properties(multi_ai_runtime_integration_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
