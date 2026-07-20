/***
 * File: apps/gui/main.cpp
 * Purpose: Own native GUI startup, graphics lifecycle, and the top-level frame
 *          loop for the CPSSim workbench.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-19
 * Notes: The application reads detached SimulationSnapshot values and sends
 *        GuiCommand values. It never accesses mutable kernel containers.
 ***/

#include "gui_application.hpp"
#include "native_file_dialog.hpp"

#include "cpssim/application/bosch_project_factory.hpp"
#include "cpssim/application/recent_projects.hpp"
#include "cpssim/config/json_config.hpp"
#include "cpssim/functional/mock_functional_model.hpp"
#include "cpssim/gui/display_scale.hpp"
#include "cpssim/gui/simulation_session.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int default_window_width = 1200;
constexpr int default_window_height = 760;
constexpr float default_font_size = 16.0F;

/*** Reports GLFW errors to the application diagnostic stream. ***/
void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

/*** Returns a safe primary-monitor content scale for initial GUI sizing. ***/
float initial_display_scale() {
    auto* primary_monitor = glfwGetPrimaryMonitor();
    if (primary_monitor == nullptr) {
        return cpssim::sanitize_gui_display_scale(0.0F);
    }

    return cpssim::sanitize_gui_display_scale(
        ImGui_ImplGlfw_GetContentScaleForMonitor(primary_monitor));
}

/*** Rebuilds layout metrics from an unscaled style for the current monitor. ***/
void apply_display_scale(const ImGuiStyle& base_style, float display_scale) {
    const auto user_text_scale = ImGui::GetStyle().FontScaleMain;
    ImGui::GetStyle() = base_style;
    ImGui::GetStyle().FontScaleMain = user_text_scale;
    ImGui::GetStyle().ScaleAllSizes(display_scale);
    ImGui::GetStyle().FontScaleDpi = display_scale;
}

ImGuiStyle make_base_style(cpssim::GuiTheme theme) {
    ImGuiStyle base_style;
    if (theme == cpssim::GuiTheme::Light) {
        ImGui::StyleColorsLight(&base_style);
    } else {
        ImGui::StyleColorsDark(&base_style);
    }
    base_style.FontSizeBase = default_font_size;
    return base_style;
}

/*** Owns the native window and immediate-mode render loop. ***/
int run_gui(std::unique_ptr<cpssim::GuiSimulationSession> session,
            const std::filesystem::path& executable_path,
            const std::filesystem::path& repository_root) {
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() == GLFW_FALSE) {
        throw std::runtime_error{"GLFW initialization failed"};
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    const auto startup_display_scale = initial_display_scale();
    GLFWwindow* window =
        glfwCreateWindow(default_window_width, default_window_height, "CPSSim", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        throw std::runtime_error{"GLFW window creation failed"};
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto applied_theme = cpssim::GuiTheme::Dark;
    auto base_style = make_base_style(applied_theme);
    auto display_scale = startup_display_scale;
    apply_display_scale(base_style, display_scale);
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        throw std::runtime_error{"Dear ImGui GLFW backend initialization failed"};
    }
    if (!ImGui_ImplOpenGL3_Init("#version 150")) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        throw std::runtime_error{"Dear ImGui OpenGL3 backend initialization failed"};
    }
    display_scale = cpssim::sanitize_gui_display_scale(
        ImGui_ImplGlfw_GetContentScaleForWindow(window), display_scale);
    apply_display_scale(base_style, display_scale);

    auto dialogs = std::make_unique<cpssim::gui::NativeFileDialog>();
    cpssim::gui::GuiApplication application{
        std::move(session),
        std::move(dialogs),
        {.projects_directory = repository_root / "projects",
         .preferences_file = cpssim::default_gui_preferences_file(),
         .examples_directory = repository_root / "examples",
         .bosch_reference_directory = repository_root / "experiments/bosch_v10_reference",
         .bosch_fmu_library = cpssim::resolve_bundled_bosch_fmu(executable_path)}};
    applied_theme = application.theme();
    base_style = make_base_style(applied_theme);
    apply_display_scale(base_style, display_scale);
    ImVec2 last_framebuffer_scale{1.0F, 1.0F};

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        glfwPollEvents();
        application.update_active_session();

        const auto reported_display_scale = ImGui_ImplGlfw_GetContentScaleForWindow(window);
        if (cpssim::gui_presentation_style_changed(applied_theme, application.theme(),
                                                   display_scale, reported_display_scale)) {
            if (applied_theme != application.theme()) {
                applied_theme = application.theme();
                base_style = make_base_style(applied_theme);
            }
            display_scale =
                cpssim::sanitize_gui_display_scale(reported_display_scale, display_scale);
            apply_display_scale(base_style, display_scale);
        }

        int display_width = 0;
        int display_height = 0;
        glfwGetFramebufferSize(window, &display_width, &display_height);
        if (display_width <= 0 || display_height <= 0) {
            glfwWaitEventsTimeout(0.05);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        auto& io = ImGui::GetIO();
        io.DisplayFramebufferScale.x = cpssim::sanitize_gui_framebuffer_scale(
            io.DisplayFramebufferScale.x, last_framebuffer_scale.x);
        io.DisplayFramebufferScale.y = cpssim::sanitize_gui_framebuffer_scale(
            io.DisplayFramebufferScale.y, last_framebuffer_scale.y);
        last_framebuffer_scale = io.DisplayFramebufferScale;
        ImGui::NewFrame();

        application.draw_frame();

        ImGui::Render();
        glViewport(0, 0, display_width, display_height);
        const auto clear_color = cpssim::gui_theme_clear_color(application.theme());
        glClearColor(clear_color.red, clear_color.green, clear_color.blue, clear_color.alpha);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}

} // namespace

/***
 * Launches Home by default or loads a supplied experiment. Arguments are an
 * optional configuration path and inclusive stop tick; --help needs no display.
 ***/
int main(int argc, char* argv[]) {
    try {
        if (argc > 1 && std::string{argv[1]} == "--help") {
            std::cout << "Usage: cpssim_gui [config.json] [stop_tick] [--mock-functional]\n";
            return EXIT_SUCCESS;
        }
        if (argc > 4) {
            throw std::invalid_argument{"too many arguments; use --help for usage"};
        }
        std::unique_ptr<cpssim::GuiSimulationSession> session;
        if (argc > 1) {
            const std::filesystem::path config_path{argv[1]};
            const cpssim::Tick stop_tick = argc > 2 ? std::stoll(argv[2]) : 300;
            const auto use_mock_functional = argc > 3;
            if (use_mock_functional && std::string{argv[3]} != "--mock-functional") {
                throw std::invalid_argument{"third argument must be --mock-functional"};
            }

            auto config = cpssim::load_experiment_config(config_path);
            cpssim::GuiFunctionalModelFactory functional_factory;
            std::vector<cpssim::GuiSignalDescriptor> signal_registry;
            if (use_mock_functional) {
                functional_factory = [] { return std::make_unique<cpssim::MockFunctionalModel>(); };
                signal_registry = {{.id = {cpssim::GuiSignalScalarType::Real, "mock_state"},
                                    .path = "Functional/Mock/State",
                                    .display_name = "Mock state",
                                    .unit = "",
                                    .source = "mock"}};
            }
            session = std::make_unique<cpssim::GuiSimulationSession>(std::move(config), stop_tick,
                                                                     std::move(functional_factory),
                                                                     std::move(signal_registry));
        }
        return run_gui(std::move(session), std::filesystem::absolute(argv[0]),
                       std::filesystem::current_path());
    } catch (const std::exception& error) {
        std::cerr << "cpssim_gui: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
