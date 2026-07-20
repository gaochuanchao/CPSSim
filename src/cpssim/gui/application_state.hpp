/***
 * File: src/cpssim/gui/application_state.hpp
 * Purpose: Own the GUI's optional standalone session or project context.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-20
 ***/

#pragma once

#include "cpssim/application/project/project.hpp"

#include <memory>
#include <stdexcept>
#include <utility>
#include <variant>

namespace cpssim {

enum class GuiApplicationScreen {
    Home,
    Workbench,
};

/*** Owns Home, a legacy standalone session, or one complete project context. ***/
class GuiApplicationState {
  public:
    GuiApplicationState() = default;

    explicit GuiApplicationState(std::unique_ptr<GuiSimulationSession> session) {
        replace_session(std::move(session));
    }

    explicit GuiApplicationState(std::unique_ptr<ProjectContext> project) {
        replace_project(std::move(project));
    }

    GuiApplicationScreen screen() const noexcept {
        return std::holds_alternative<std::monostate>(workbench_) ? GuiApplicationScreen::Home
                                                                  : GuiApplicationScreen::Workbench;
    }

    bool has_active_session() const noexcept {
        return !std::holds_alternative<std::monostate>(workbench_);
    }

    bool has_active_project() const noexcept {
        return std::holds_alternative<std::unique_ptr<ProjectContext>>(workbench_);
    }

    GuiSimulationSession& active_session() {
        if (auto* session = std::get_if<std::unique_ptr<GuiSimulationSession>>(&workbench_)) {
            return **session;
        }
        if (auto* project = std::get_if<std::unique_ptr<ProjectContext>>(&workbench_)) {
            return (*project)->session();
        }
        throw std::logic_error{"the GUI has no active simulation session"};
    }

    const GuiSimulationSession& active_session() const {
        if (const auto* session = std::get_if<std::unique_ptr<GuiSimulationSession>>(&workbench_)) {
            return **session;
        }
        if (const auto* project = std::get_if<std::unique_ptr<ProjectContext>>(&workbench_)) {
            return (*project)->session();
        }
        throw std::logic_error{"the GUI has no active simulation session"};
    }

    ProjectContext& active_project() {
        if (auto* project = std::get_if<std::unique_ptr<ProjectContext>>(&workbench_)) {
            return **project;
        }
        throw std::logic_error{"the GUI has no active project"};
    }

    const ProjectContext& active_project() const {
        if (const auto* project = std::get_if<std::unique_ptr<ProjectContext>>(&workbench_)) {
            return **project;
        }
        throw std::logic_error{"the GUI has no active project"};
    }

    // The caller constructs and validates the replacement before ownership changes.
    void replace_session(std::unique_ptr<GuiSimulationSession> replacement) {
        if (replacement == nullptr) {
            throw std::invalid_argument{"a replacement GUI session must not be empty"};
        }
        workbench_ = std::move(replacement);
    }

    // The loader constructs and validates the project before this call.
    void replace_project(std::unique_ptr<ProjectContext> replacement) {
        if (replacement == nullptr) {
            throw std::invalid_argument{"a replacement GUI project must not be empty"};
        }
        workbench_ = std::move(replacement);
    }

    void clear_session() { workbench_.emplace<std::monostate>(); }

  private:
    std::variant<std::monostate, std::unique_ptr<GuiSimulationSession>,
                 std::unique_ptr<ProjectContext>>
        workbench_;
};

} // namespace cpssim
