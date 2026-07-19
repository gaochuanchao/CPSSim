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

#include "cpssim/config/json_config.hpp"
#include "cpssim/functional/mock_functional_model.hpp"
#include "cpssim/gui/simulation_session.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
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
constexpr float minimum_display_scale = 1.0F;
constexpr float maximum_display_scale = 4.0F;

/*** Reports GLFW errors to the application diagnostic stream. ***/
void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

/*** Returns a safe primary-monitor content scale for initial GUI sizing. ***/
float initial_display_scale() {
    auto* primary_monitor = glfwGetPrimaryMonitor();
    if (primary_monitor == nullptr) {
        return minimum_display_scale;
    }

    const auto reported_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(primary_monitor);
    if (!std::isfinite(reported_scale) || reported_scale <= 0.0F) {
        return minimum_display_scale;
    }
    return std::clamp(reported_scale, minimum_display_scale, maximum_display_scale);
}

/*** Owns the native window and immediate-mode render loop. ***/
int run_gui(cpssim::GuiSimulationSession& session) {
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() == GLFW_FALSE) {
        throw std::runtime_error{"GLFW initialization failed"};
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    const auto display_scale = initial_display_scale();
    const auto window_width = static_cast<int>(default_window_width * display_scale);
    const auto window_height = static_cast<int>(default_window_height * display_scale);
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "CPSSim", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        throw std::runtime_error{"GLFW window creation failed"};
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.FontSizeBase = default_font_size;
    style.ScaleAllSizes(display_scale);
    style.FontScaleDpi = display_scale;
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

    cpssim::gui::GuiApplication application{session};

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        glfwPollEvents();
        session.update();
        const auto snapshot = session.snapshot();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        application.draw_frame(snapshot);

        ImGui::Render();
        int display_width = 0;
        int display_height = 0;
        glfwGetFramebufferSize(window, &display_width, &display_height);
        glViewport(0, 0, display_width, display_height);
        glClearColor(0.08F, 0.09F, 0.11F, 1.0F);
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
 * Loads an experiment and launches the optional GUI. Arguments are an optional
 * configuration path and inclusive stop tick; --help needs no display server.
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
        const std::filesystem::path config_path =
            argc > 1 ? std::filesystem::path{argv[1]}
                     : std::filesystem::path{"config/examples/basic.json"};
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
        cpssim::GuiSimulationSession session{std::move(config), stop_tick,
                                             std::move(functional_factory),
                                             std::move(signal_registry)};
        return run_gui(session);
    } catch (const std::exception& error) {
        std::cerr << "cpssim_gui: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
