cmake_minimum_required(VERSION 3.24)

project(
    CPSSim
    VERSION 0.1.0
    DESCRIPTION "Portable deterministic cyber-physical-system simulator"
    LANGUAGES C CXX
)

option(CPSSIM_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)
option(CPSSIM_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(CPSSIM_ENABLE_CLANG_TIDY "Run clang-tidy while compiling project targets" OFF)
option(CPSSIM_BUILD_GUI "Build the optional Dear ImGui visualization executable" OFF)

set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ClangTidy)
include(ProjectSanitizers)
include(ProjectWarnings)

cpssim_configure_clang_tidy()

find_package(nlohmann_json 3.11 QUIET)

if(NOT nlohmann_json_FOUND)
    include(FetchContent)

    string(MAKE_C_IDENTIFIER "${CMAKE_GENERATOR}" CPSSIM_DEPENDENCY_GENERATOR)
    set(
        FETCHCONTENT_BASE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/${CPSSIM_DEPENDENCY_GENERATOR}"
    )
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

if(NOT TARGET xlsxwriter)
    include(FetchContent)
    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG 925af44f3cde53c6b076611c297850091b5dc7bb
        GIT_SHALLOW FALSE
    )
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(zlib)
    FetchContent_Declare(
        libxlsxwriter
        GIT_REPOSITORY https://github.com/jmcnamara/libxlsxwriter.git
        GIT_TAG 2894634d65cee6021901a165bfc2bb0fad6da193
        GIT_SHALLOW FALSE
        SOURCE_SUBDIR dependency-populated-without-upstream-build
    )
    FetchContent_MakeAvailable(libxlsxwriter)
    file(GLOB CPSSIM_XLSXWRITER_SOURCES "${libxlsxwriter_SOURCE_DIR}/src/*.c")
    add_library(
        xlsxwriter STATIC
        ${CPSSIM_XLSXWRITER_SOURCES}
        "${libxlsxwriter_SOURCE_DIR}/third_party/minizip/ioapi.c"
        "${libxlsxwriter_SOURCE_DIR}/third_party/minizip/zip.c"
        "${libxlsxwriter_SOURCE_DIR}/third_party/tmpfileplus/tmpfileplus.c"
        "${libxlsxwriter_SOURCE_DIR}/third_party/md5/md5.c"
    )
    target_include_directories(
        xlsxwriter
        SYSTEM PUBLIC
            "${libxlsxwriter_SOURCE_DIR}/include"
            "${libxlsxwriter_SOURCE_DIR}/include/xlsxwriter"
        PRIVATE
            "${libxlsxwriter_SOURCE_DIR}/third_party/minizip"
            "${libxlsxwriter_SOURCE_DIR}/third_party/tmpfileplus"
            "${libxlsxwriter_SOURCE_DIR}/third_party/md5"
    )
    target_compile_definitions(xlsxwriter PRIVATE NOCRYPT NOUNCRYPT)
    target_link_libraries(xlsxwriter PRIVATE zlibstatic)
endif()

add_library(
    cpssim_core STATIC
    src/cpssim/config/json_config.cpp
    src/cpssim/config/json_run_plan.cpp
    src/cpssim/core/version.cpp
    src/cpssim/functional/functional_runtime.cpp
    src/cpssim/functional/mock_functional_model.cpp
    src/cpssim/kernel/event_queue.cpp
    src/cpssim/kernel/periodic_release.cpp
    src/cpssim/kernel/scheduler.cpp
    src/cpssim/kernel/simulation_engine.cpp
    src/cpssim/model/event.cpp
    src/cpssim/model/experiment_config.cpp
    src/cpssim/model/message.cpp
    src/cpssim/model/run_plan.cpp
    src/cpssim/model/runtime_state.cpp
    src/cpssim/model/specifications.cpp
    src/cpssim/model/time.cpp
    src/cpssim/network/fixed_delay_network.cpp
    src/cpssim/policy/fixed_priority.cpp
    src/cpssim/policy/resource_allocator.cpp
    src/cpssim/trace/event_json.cpp
)
add_library(cpssim::core ALIAS cpssim_core)

target_compile_features(cpssim_core PUBLIC cxx_std_20)
target_compile_definitions(
    cpssim_core
    PRIVATE CPSSIM_VERSION="${PROJECT_VERSION}"
)
target_include_directories(
    cpssim_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
get_target_property(
    CPSSIM_NLOHMANN_INCLUDE_DIRECTORIES
    nlohmann_json::nlohmann_json
    INTERFACE_INCLUDE_DIRECTORIES
)
target_include_directories(
    cpssim_core
    SYSTEM PRIVATE ${CPSSIM_NLOHMANN_INCLUDE_DIRECTORIES}
)
target_link_libraries(cpssim_core PRIVATE nlohmann_json::nlohmann_json)

cpssim_set_project_warnings(cpssim_core)
cpssim_enable_sanitizers(cpssim_core)

add_library(
    cpssim_bosch_adapter STATIC
    src/cpssim/bosch/trigger_encoder.cpp
)
target_link_libraries(cpssim_bosch_adapter PUBLIC cpssim::core)

cpssim_set_project_warnings(cpssim_bosch_adapter)
cpssim_enable_sanitizers(cpssim_bosch_adapter)

add_library(
    cpssim_fmi2_adapter STATIC
    src/cpssim/fmi/fmi2_importer.cpp
)
add_library(cpssim::fmi2_adapter ALIAS cpssim_fmi2_adapter)
target_compile_features(cpssim_fmi2_adapter PUBLIC cxx_std_20)
target_include_directories(
    cpssim_fmi2_adapter
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
)
target_link_libraries(cpssim_fmi2_adapter PRIVATE ${CMAKE_DL_LIBS})

cpssim_set_project_warnings(cpssim_fmi2_adapter)
cpssim_enable_sanitizers(cpssim_fmi2_adapter)

add_library(
    cpssim_bosch_fmi2_adapter STATIC
    src/cpssim/bosch/bosch_fmi2_functional_model.cpp
    src/cpssim/bosch/example_data.cpp
)
target_link_libraries(
    cpssim_bosch_fmi2_adapter
    PUBLIC
        cpssim::core
        cpssim_bosch_adapter
        cpssim_fmi2_adapter
)

cpssim_set_project_warnings(cpssim_bosch_fmi2_adapter)
cpssim_enable_sanitizers(cpssim_bosch_fmi2_adapter)

add_library(
    cpssim_bosch_reference STATIC
    src/cpssim/conformance/bosch_reference.cpp
)
target_link_libraries(cpssim_bosch_reference PUBLIC cpssim_bosch_adapter)

cpssim_set_project_warnings(cpssim_bosch_reference)
cpssim_enable_sanitizers(cpssim_bosch_reference)

add_library(
    cpssim_bosch_functional_reference STATIC
    src/cpssim/conformance/bosch_functional_reference.cpp
)
target_link_libraries(
    cpssim_bosch_functional_reference
    PUBLIC
        cpssim_bosch_fmi2_adapter
        cpssim_bosch_reference
)

cpssim_set_project_warnings(cpssim_bosch_functional_reference)
cpssim_enable_sanitizers(cpssim_bosch_functional_reference)

add_library(
    cpssim_bosch_application STATIC
    src/cpssim/application/bosch_run_service.cpp
)
target_link_libraries(
    cpssim_bosch_application
    PUBLIC
        cpssim_bosch_fmi2_adapter
        cpssim_bosch_reference
)

cpssim_set_project_warnings(cpssim_bosch_application)
cpssim_enable_sanitizers(cpssim_bosch_application)

add_library(
    cpssim_cli_support STATIC
    apps/cli/cli_application.cpp
    apps/cli/command_parser.cpp
    apps/cli/command_registry.cpp
    apps/cli/commands/bosch_run_command.cpp
    apps/cli/commands/exit_command.cpp
    apps/cli/commands/help_command.cpp
    apps/cli/commands/list_command.cpp
    apps/cli/commands/version_command.cpp
)
target_include_directories(
    cpssim_cli_support
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)
target_link_libraries(
    cpssim_cli_support
    PUBLIC
        cpssim::core
        cpssim_bosch_application
)

cpssim_set_project_warnings(cpssim_cli_support)
cpssim_enable_sanitizers(cpssim_cli_support)

add_executable(cpssim_cli apps/cli/main.cpp)
target_link_libraries(cpssim_cli PRIVATE cpssim_cli_support)

cpssim_set_project_warnings(cpssim_cli)
cpssim_enable_sanitizers(cpssim_cli)

add_library(
    cpssim_gui_support STATIC
    src/cpssim/analysis/completed_run_result.cpp
    src/cpssim/analysis/run_result.cpp
    src/cpssim/application/gui_layout_store.cpp
    src/cpssim/application/result_export.cpp
    src/cpssim/application/results_workbook.cpp
    src/cpssim/application/project/project.cpp
    src/cpssim/application/project/project_template.cpp
    src/cpssim/application/project/project_workflow.cpp
    src/cpssim/application/project/system_builder_workflow.cpp
    src/cpssim/application/recent_projects.cpp
    src/cpssim/gui/architecture_graph.cpp
    src/cpssim/gui/display_scale.cpp
    src/cpssim/gui/event_table_model.cpp
    src/cpssim/gui/draft_run_plan.cpp
    src/cpssim/gui/editable_system_draft.cpp
    src/cpssim/gui/system_builder_interaction.cpp
    src/cpssim/gui/presentation_model.cpp
    src/cpssim/gui/plot_visualizer_model.cpp
    src/cpssim/gui/resource_presentation.cpp
    src/cpssim/gui/selection_model.cpp
    src/cpssim/gui/signal_series.cpp
    src/cpssim/gui/simulation_controller.cpp
    src/cpssim/gui/simulation_session.cpp
    src/cpssim/gui/timeline_model.cpp
    src/cpssim/gui/workspace_state.cpp
)
target_link_libraries(cpssim_gui_support PUBLIC cpssim::core)
target_include_directories(
    cpssim_gui_support
    SYSTEM PRIVATE ${CPSSIM_NLOHMANN_INCLUDE_DIRECTORIES}
)
target_link_libraries(cpssim_gui_support PRIVATE nlohmann_json::nlohmann_json xlsxwriter)

cpssim_set_project_warnings(cpssim_gui_support)
cpssim_enable_sanitizers(cpssim_gui_support)

add_library(
    cpssim_bosch_gui_application STATIC
    src/cpssim/application/bosch_project_factory.cpp
    src/cpssim/application/bosch_result_analysis.cpp
)
target_link_libraries(
    cpssim_bosch_gui_application
    PUBLIC
        cpssim_bosch_fmi2_adapter
        cpssim_bosch_reference
        cpssim_gui_support
)
target_include_directories(
    cpssim_bosch_gui_application
    SYSTEM PRIVATE ${CPSSIM_NLOHMANN_INCLUDE_DIRECTORIES}
)
target_link_libraries(cpssim_bosch_gui_application PRIVATE nlohmann_json::nlohmann_json)
cpssim_set_project_warnings(cpssim_bosch_gui_application)
cpssim_enable_sanitizers(cpssim_bosch_gui_application)

if(CPSSIM_BUILD_GUI)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 3.3 CONFIG REQUIRED)

    include(FetchContent)
    string(MAKE_C_IDENTIFIER "${CMAKE_GENERATOR}" CPSSIM_GUI_DEPENDENCY_GENERATOR)
    set(
        FETCHCONTENT_BASE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/build/_deps/${CPSSIM_GUI_DEPENDENCY_GENERATOR}"
    )
    FetchContent_Declare(
        imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.8.tar.gz
        URL_HASH SHA256=fecb33d33930e12ff53a34064e9d3a06c8f7c3e04408f14cd36c80e3faac863b
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(imgui)

    FetchContent_Declare(
        implot
        GIT_REPOSITORY https://github.com/epezent/implot.git
        GIT_TAG 524f9fcd48d76c13fdf94c5ffbba8787a1ff7e39
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(implot)

    FetchContent_Declare(
        portable_file_dialogs
        GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
        GIT_TAG c12ea8c9a727f5320a2b4570aee863bbede2a204
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(portable_file_dialogs)

    add_library(
        cpssim_imgui STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    )
    target_include_directories(
        cpssim_imgui
        SYSTEM PUBLIC
            "${imgui_SOURCE_DIR}"
            "${imgui_SOURCE_DIR}/backends"
    )
    target_link_libraries(cpssim_imgui PUBLIC glfw OpenGL::GL)

    add_library(
        cpssim_implot STATIC
        "${implot_SOURCE_DIR}/implot.cpp"
        "${implot_SOURCE_DIR}/implot_items.cpp"
    )
    target_include_directories(cpssim_implot SYSTEM PUBLIC "${implot_SOURCE_DIR}")
    target_link_libraries(cpssim_implot PUBLIC cpssim_imgui)

    add_executable(
        cpssim_gui
        apps/gui/gui_application.cpp
        apps/gui/main.cpp
        apps/gui/native_file_dialog.cpp
        apps/gui/views/architecture_view.cpp
        apps/gui/views/event_view.cpp
        apps/gui/views/experiment_explorer.cpp
        apps/gui/views/inspector_view.cpp
        apps/gui/views/resource_view.cpp
        apps/gui/views/results_view.cpp
        apps/gui/views/plot_visualizer.cpp
        apps/gui/views/run_plan_editor.cpp
        apps/gui/views/signal_view.cpp
        apps/gui/views/system_builder.cpp
        apps/gui/views/timeline_view.cpp
        apps/gui/views/toolbar_view.cpp
    )
    target_include_directories(cpssim_gui SYSTEM PRIVATE "${portable_file_dialogs_SOURCE_DIR}")
    target_link_libraries(
        cpssim_gui
        PRIVATE cpssim_gui_support cpssim_bosch_gui_application cpssim_imgui cpssim_implot
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/gui/imgui.ini"
        "${CMAKE_CURRENT_BINARY_DIR}/imgui.ini"
        COPYONLY
    )

    cpssim_set_project_warnings(cpssim_gui)
    cpssim_enable_sanitizers(cpssim_gui)
endif()

add_executable(cpssim_bosch_conformance apps/conformance/main.cpp)
target_link_libraries(cpssim_bosch_conformance PRIVATE cpssim_bosch_reference)

cpssim_set_project_warnings(cpssim_bosch_conformance)
cpssim_enable_sanitizers(cpssim_bosch_conformance)

add_executable(cpssim_bosch_example apps/bosch_example/main.cpp)
target_link_libraries(cpssim_bosch_example PRIVATE cpssim_bosch_application)

cpssim_set_project_warnings(cpssim_bosch_example)
cpssim_enable_sanitizers(cpssim_bosch_example)

include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)

    include(Catch)

    function(cpssim_add_catch_tests target)
        cmake_parse_arguments(ARG "" "LABEL" "SOURCES;LIBRARIES" ${ARGN})
        add_executable(${target} ${ARG_SOURCES})
        # Catch2's decomposer intentionally rewrites assertions into chained
        # comparisons that trigger clang-tidy false positives. Test targets
        # still compile with project warnings and execute in the tidy preset.
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "")
        target_link_libraries(${target} PRIVATE ${ARG_LIBRARIES} Catch2::Catch2WithMain)
        cpssim_set_project_warnings(${target})
        cpssim_enable_sanitizers(${target})
        catch_discover_tests(
            ${target}
            TEST_PREFIX "${target}: "
            DISCOVERY_MODE PRE_TEST
            PROPERTIES LABELS "${ARG_LABEL}"
        )
    endfunction()

    add_library(
        cpssim_bosch_fmu_linux SHARED
        LateralMotionControl/sources/LateralMotionControl.c
    )
    target_include_directories(
        cpssim_bosch_fmu_linux
        PRIVATE LateralMotionControl/sources
    )
    if(NOT WIN32)
        target_link_libraries(cpssim_bosch_fmu_linux PRIVATE m)
    endif()
    set_target_properties(
        cpssim_bosch_fmu_linux
        PROPERTIES
            PREFIX ""
            OUTPUT_NAME "LateralMotionControl"
    )

    cpssim_add_catch_tests(
        cpssim_core_tests
        SOURCES
            tests/model/categories_test.cpp
            tests/model/event_test.cpp
            tests/model/identifiers_test.cpp
            tests/model/message_test.cpp
            tests/model/run_plan_test.cpp
            tests/model/runtime_state_test.cpp
            tests/model/specifications_test.cpp
            tests/model/time_test.cpp
            tests/smoke_test.cpp
            tests/trace/event_json_test.cpp
        LIBRARIES cpssim::core
        LABEL core
    )

    cpssim_add_catch_tests(
        cpssim_config_tests
        SOURCES
            tests/config/json_config_test.cpp
            tests/config/json_run_plan_test.cpp
        LIBRARIES cpssim::core
        LABEL config
    )

    cpssim_add_catch_tests(
        cpssim_kernel_tests
        SOURCES
            tests/kernel/event_queue_test.cpp
            tests/kernel/periodic_release_test.cpp
            tests/kernel/simulation_engine_test.cpp
        LIBRARIES cpssim::core
        LABEL kernel
    )

    cpssim_add_catch_tests(
        cpssim_scheduler_tests
        SOURCES
            tests/kernel/multi_resource_simulation_test.cpp
            tests/kernel/scheduler_test.cpp
            tests/policy/fixed_priority_test.cpp
            tests/policy/resource_allocator_test.cpp
        LIBRARIES cpssim::core
        LABEL scheduler
    )

    cpssim_add_catch_tests(
        cpssim_network_tests
        SOURCES
            tests/kernel/network_simulation_test.cpp
            tests/network/fixed_delay_network_test.cpp
        LIBRARIES cpssim::core
        LABEL network
    )

    cpssim_add_catch_tests(
        cpssim_functional_runtime_tests
        SOURCES tests/functional/functional_runtime_test.cpp
        LIBRARIES cpssim::core
        LABEL functional
    )

    cpssim_add_catch_tests(
        cpssim_bosch_tests
        SOURCES tests/bosch/trigger_encoder_test.cpp
        LIBRARIES cpssim_bosch_reference
        LABEL bosch
    )

    cpssim_add_catch_tests(
        cpssim_bosch_reference_tests
        SOURCES tests/conformance/bosch_reference_test.cpp
        LIBRARIES cpssim_bosch_reference
        LABEL conformance
    )

    cpssim_add_catch_tests(
        cpssim_gui_tests
        SOURCES
            tests/gui/application_state_test.cpp
            tests/gui/architecture_graph_test.cpp
            tests/gui/display_scale_test.cpp
            tests/gui/draft_run_plan_test.cpp
            tests/gui/editable_system_draft_test.cpp
            tests/gui/event_table_model_test.cpp
            tests/gui/presentation_model_test.cpp
            tests/gui/resource_presentation_test.cpp
            tests/gui/selection_model_test.cpp
            tests/gui/system_builder_interaction_test.cpp
            tests/gui/signal_series_test.cpp
            tests/gui/simulation_controller_test.cpp
            tests/gui/timeline_model_test.cpp
            tests/gui/workspace_state_test.cpp
            tests/analysis/run_result_test.cpp
        LIBRARIES cpssim_gui_support
        LABEL gui
    )

    cpssim_add_catch_tests(
        cpssim_project_tests
        SOURCES
            tests/application/gui_layout_store_test.cpp
            tests/application/project/project_persistence_test.cpp
            tests/application/project/system_builder_workflow_test.cpp
            tests/application/recent_projects_test.cpp
            tests/application/result_export_test.cpp
        LIBRARIES cpssim_gui_support
        LABEL project
    )

    add_executable(
        cpssim_bosch_project_tests
        tests/application/bosch_project_factory_test.cpp
        tests/application/bosch_result_analysis_test.cpp
    )
    set_target_properties(cpssim_bosch_project_tests PROPERTIES CXX_CLANG_TIDY "")
    target_link_libraries(
        cpssim_bosch_project_tests
        PRIVATE cpssim_bosch_gui_application Catch2::Catch2WithMain
    )
    add_dependencies(cpssim_bosch_project_tests cpssim_bosch_fmu_linux)
    cpssim_set_project_warnings(cpssim_bosch_project_tests)
    cpssim_enable_sanitizers(cpssim_bosch_project_tests)
    catch_discover_tests(
        cpssim_bosch_project_tests
        TEST_PREFIX "project-bosch: "
        DISCOVERY_MODE PRE_TEST
        PROPERTIES
            ENVIRONMENT
                "CPSSIM_G1_BOSCH_FMU_LIBRARY=$<TARGET_FILE:cpssim_bosch_fmu_linux>"
            LABELS project
    )

    cpssim_add_catch_tests(
        cpssim_cli_tests
        SOURCES tests/cli/cli_application_test.cpp
        LIBRARIES cpssim_cli_support
        LABEL cli
    )

    add_executable(cpssim_fmi2_tests tests/fmi/fmi2_importer_test.cpp)
    set_target_properties(cpssim_fmi2_tests PROPERTIES CXX_CLANG_TIDY "")
    target_link_libraries(
        cpssim_fmi2_tests
        PRIVATE
            cpssim_fmi2_adapter
            Catch2::Catch2WithMain
    )
    add_dependencies(cpssim_fmi2_tests cpssim_bosch_fmu_linux)

    cpssim_set_project_warnings(cpssim_fmi2_tests)
    cpssim_enable_sanitizers(cpssim_fmi2_tests)

    catch_discover_tests(
        cpssim_fmi2_tests
        TEST_PREFIX "fmi2: "
        DISCOVERY_MODE PRE_TEST
        PROPERTIES
            ENVIRONMENT
                "CPSSIM_T15_BOSCH_FMU_LIBRARY=$<TARGET_FILE:cpssim_bosch_fmu_linux>"
            LABELS fmi
    )

    add_executable(
        cpssim_functional_tests
        tests/bosch/bosch_fmi2_functional_model_test.cpp
        tests/bosch/example_data_test.cpp
        tests/conformance/bosch_functional_reference_test.cpp
    )
    set_target_properties(cpssim_functional_tests PROPERTIES CXX_CLANG_TIDY "")
    target_link_libraries(
        cpssim_functional_tests
        PRIVATE
            cpssim_bosch_application
            cpssim_bosch_functional_reference
            Catch2::Catch2WithMain
    )
    add_dependencies(cpssim_functional_tests cpssim_bosch_fmu_linux)

    cpssim_set_project_warnings(cpssim_functional_tests)
    cpssim_enable_sanitizers(cpssim_functional_tests)

    catch_discover_tests(
        cpssim_functional_tests
        TEST_PREFIX "functional: "
        DISCOVERY_MODE PRE_TEST
        PROPERTIES
            ENVIRONMENT
                "CPSSIM_T16_BOSCH_FMU_LIBRARY=$<TARGET_FILE:cpssim_bosch_fmu_linux>"
            LABELS functional
    )

    add_test(
        NAME cpssim_bosch_conformance_dedicated
        COMMAND cpssim_bosch_conformance
                "${CMAKE_CURRENT_SOURCE_DIR}/experiments/bosch_v10_reference"
                dedicated
    )
    add_test(
        NAME cpssim_bosch_conformance_shared_cloud
        COMMAND cpssim_bosch_conformance
                "${CMAKE_CURRENT_SOURCE_DIR}/experiments/bosch_v10_reference"
                shared_cloud
    )
    set_tests_properties(
        cpssim_bosch_conformance_dedicated
        cpssim_bosch_conformance_shared_cloud
        PROPERTIES LABELS conformance
    )
endif()
