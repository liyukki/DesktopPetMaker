cmake_minimum_required(VERSION 3.22)
project(pro VERSION 1.0.0 LANGUAGES CXX)

option(DESKTOP_PET_ENABLE_LIVE2D "Enable the optional Live2D/Cubism adapter interface" OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Test)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)
include(CTest)

add_library(test_support STATIC
    tests/test_support.h
    tests/test_support.cpp
)
target_include_directories(test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(test_support PUBLIC Qt6::Widgets Qt6::Test)
set_target_properties(test_support PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

function(register_desktop_pet_test target)
    qt_add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
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

set(PETPROJECT_TEST_SOURCES petproject.h petproject.cpp archivecommandrunner.h archivecommandrunner.cpp)
register_desktop_pet_test(core_project_tests tests/core_project_tests.cpp ${PETPROJECT_TEST_SOURCES})
register_desktop_pet_test(asset_transaction_tests tests/asset_transaction_tests.cpp ${PETPROJECT_TEST_SOURCES}
    proceduralmotiongenerator.h proceduralmotiongenerator.cpp)
register_desktop_pet_test(sprite_sheet_tests tests/sprite_sheet_tests.cpp spritesheetslicer.h spritesheetslicer.cpp)
register_desktop_pet_test(shimeji_transaction_tests tests/shimeji_transaction_tests.cpp
    shimejiimportwizard.h shimejiimportwizard.cpp ${PETPROJECT_TEST_SOURCES})
register_desktop_pet_test(petpack_security_tests tests/petpack_security_tests.cpp ${PETPROJECT_TEST_SOURCES})
register_desktop_pet_test(provider_credential_tests tests/provider_credential_tests.cpp
    aiproviderprofileservice.h aiproviderprofileservice.cpp aiproviderprofileregistry.h aiproviderprofileregistry.cpp
    credentialstore.h credentialstore.cpp)
register_desktop_pet_test(credential_store_windows_integration_tests
    tests/credential_store_windows_integration_tests.cpp credentialstore.h credentialstore.cpp)
register_desktop_pet_test(multi_ai_room_tests tests/multi_ai_room_tests.cpp
    aiconversationroom.h aiconversationroom.cpp
    aiconversationroomrepository.h aiconversationroomrepository.cpp
    aiconversationroommanager.h aiconversationroommanager.cpp
    aiprovider.h aiprovider.cpp aiproviderprofileregistry.h aiproviderprofileregistry.cpp
    credentialstore.h credentialstore.cpp petairequestcoordinator.h petairequestcoordinator.cpp
    aiactiondescriptor.h aiactionvalidator.h aiactionvalidator.cpp chathistoryentry.h runtimeactionresult.h
    petproject.h petproject.cpp archivecommandrunner.h archivecommandrunner.cpp)
register_desktop_pet_test(multi_ai_lifecycle_tests tests/multi_ai_lifecycle_tests.cpp
    aiconversationroom.h aiconversationroom.cpp
    aiconversationroomrepository.h aiconversationroomrepository.cpp
    aiconversationroommanager.h aiconversationroommanager.cpp
    petairequestcoordinator.h petairequestcoordinator.cpp aiprovider.h aiprovider.cpp
    aiproviderprofileregistry.h aiproviderprofileregistry.cpp
    petproject.h petproject.cpp archivecommandrunner.h archivecommandrunner.cpp
    aiactiondescriptor.h aiactionvalidator.h aiactionvalidator.cpp chathistoryentry.h runtimeactionresult.h
    credentialstore.h credentialstore.cpp)
register_desktop_pet_test(formal_asset_tests tests/formal_asset_tests.cpp ${PETPROJECT_TEST_SOURCES})
register_desktop_pet_test(gui_widget_tests tests/gui_widget_tests.cpp
    spritesheetimportdialog.h spritesheetimportdialog.cpp spritesheetslicer.h spritesheetslicer.cpp
    ui/theme/apptheme.h ui/theme/apptheme.cpp ui/theme/themeconstants.h
    ui/theme/iconprovider.h ui/theme/iconprovider.cpp)
register_desktop_pet_test(ui_theme_tests tests/ui_theme_tests.cpp
    ui/theme/apptheme.h ui/theme/apptheme.cpp ui/theme/themeconstants.h
    ui/theme/iconprovider.h ui/theme/iconprovider.cpp)
register_desktop_pet_test(runtime_action_tests tests/runtime_action_tests.cpp
    runtimepetwindow.h runtimepetwindow.cpp runtimeactionresult.h
    screenplacementutil.h screenplacementutil.cpp
    renderbackend.h renderbackend.cpp
    runtimepetmanager.h runtimepetmanager.cpp petruntimeinstance.h petruntimeinstance.cpp
    petprojectregistry.h petprojectregistry.cpp
    petproject.h petproject.cpp archivecommandrunner.h archivecommandrunner.cpp
    petspeechbubblewindow.h petspeechbubblewindow.cpp petconversationcontext.h
    aidialogwindow.h aidialogwindow.cpp journalwindow.h journalwindow.cpp moodjournal.h moodjournal.cpp
    aiprovider.h aiprovider.cpp aiactiondescriptor.h aiactionvalidator.h aiactionvalidator.cpp chathistoryentry.h
    aiproviderprofileregistry.h aiproviderprofileregistry.cpp
    aiproviderprofileservice.h aiproviderprofileservice.cpp
    credentialstore.h credentialstore.cpp petairequestcoordinator.h petairequestcoordinator.cpp
    ui/theme/apptheme.h ui/theme/apptheme.cpp ui/theme/themeconstants.h
    ui/theme/iconprovider.h ui/theme/iconprovider.cpp)
target_compile_definitions(runtime_action_tests PRIVATE DESKTOP_PET_TESTING)
target_compile_definitions(runtime_action_tests PRIVATE DESKTOP_PET_ENABLE_LIVE2D=0)

qt_add_executable(pro
    WIN32
    main.cpp
    editorwindow.h
    editorwindow.cpp
    petproject.h
    petproject.cpp
    archivecommandrunner.h
    archivecommandrunner.cpp
    previewcanvas.h
    previewcanvas.cpp
    runtimepetwindow.h
    runtimepetwindow.cpp
    runtimeactionresult.h
    screenplacementutil.h
    screenplacementutil.cpp
    renderbackend.h
    renderbackend.cpp
    runtimepetmanager.h
    runtimepetmanager.cpp
    petruntimeinstance.h
    petruntimeinstance.cpp
    petprojectregistry.h
    petprojectregistry.cpp
    credentialstore.h
    credentialstore.cpp
    petairequestcoordinator.h
    petairequestcoordinator.cpp
    aiactiondescriptor.h
    aiactionvalidator.h
    aiactionvalidator.cpp
    chathistoryentry.h
    aiproviderprofileregistry.h
    aiproviderprofileregistry.cpp
    aiproviderprofileservice.h
    aiproviderprofileservice.cpp
    actionmaterialwindow.h
    actionmaterialwindow.cpp
    alphaboundingboxutil.h
    alphaboundingboxutil.cpp
    assetqualityanalyzer.h
    assetqualityanalyzer.cpp
    spritesheetslicer.h
    spritesheetslicer.cpp
    spritesheetimportdialog.h
    spritesheetimportdialog.cpp
    proceduralmotiongenerator.h
    proceduralmotiongenerator.cpp
    motionpromptlibrary.h
    motionpromptlibrary.cpp
    aiassetpromptwizard.h
    aiassetpromptwizard.cpp
    petbehaviorrule.h
    shimejiimportwizard.h
    shimejiimportwizard.cpp
    toolintegrationmanager.h
    toolintegrationmanager.cpp
    aiconversationroom.h
    aiconversationroom.cpp
    aiconversationroomrepository.h
    aiconversationroomrepository.cpp
    aiconversationroommanager.h
    aiconversationroommanager.cpp
    aiconversationconsolewindow.h
    aiconversationconsolewindow.cpp
    petcontrolcenterwindow.h
    petcontrolcenterwindow.cpp
    systemtraycontroller.h
    systemtraycontroller.cpp
    resources.qrc
    petspeechbubblewindow.h
    petspeechbubblewindow.cpp
    petconversationcontext.h
    aidialogwindow.h
    aidialogwindow.cpp
    moodjournal.h
    moodjournal.cpp
    journalwindow.h
    journalwindow.cpp
    aiprovider.h
    aiprovider.cpp
    ui/theme/apptheme.h
    ui/theme/apptheme.cpp
    ui/theme/themeconstants.h
    ui/theme/iconprovider.h
    ui/theme/iconprovider.cpp
)
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
    platform_smoke_tests.cpp
    aiactiondescriptor.h
    aiactionvalidator.h
    aiactionvalidator.cpp
    chathistoryentry.h
    aiprovider.h
    aiprovider.cpp
    aiproviderprofileregistry.h
    aiproviderprofileregistry.cpp
    aiproviderprofileservice.h
    aiproviderprofileservice.cpp
    credentialstore.h
    credentialstore.cpp
    petairequestcoordinator.h
    petairequestcoordinator.cpp
    aiconversationroom.h
    aiconversationroom.cpp
    aiconversationroomrepository.h
    aiconversationroomrepository.cpp
    aiconversationroommanager.h
    aiconversationroommanager.cpp
    toolintegrationmanager.h
    toolintegrationmanager.cpp
    assetqualityanalyzer.h
    assetqualityanalyzer.cpp
    alphaboundingboxutil.h
    alphaboundingboxutil.cpp
    shimejiimportwizard.h
    shimejiimportwizard.cpp
    petproject.h
    petproject.cpp
    archivecommandrunner.h
    archivecommandrunner.cpp
    spritesheetslicer.h
    spritesheetslicer.cpp
    proceduralmotiongenerator.h
    proceduralmotiongenerator.cpp
    motionpromptlibrary.h
    motionpromptlibrary.cpp
)

set_target_properties(platform_smoke_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(platform_smoke_tests PRIVATE Qt6::Widgets Qt6::Network)

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
    target_link_libraries(platform_smoke_tests PRIVATE Advapi32)
endif()

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

get_target_property(DESKTOP_PET_APP_SOURCES pro SOURCES)
list(FILTER DESKTOP_PET_APP_SOURCES EXCLUDE REGEX "(^|/)main\\.cpp$")
qt_add_executable(platform_gui_tests
    tests/platform_gui_tests.cpp
    ${DESKTOP_PET_APP_SOURCES}
)
target_include_directories(platform_gui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
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
