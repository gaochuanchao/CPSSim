/***
 * File: src/cpssim/gui/application_state.hpp
 * Purpose: Own the GUI's optional simulation session and derive its screen.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-20
 ***/

#pragma once

#include "cpssim/gui/simulation_session.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

namespace cpssim {

enum class GuiApplicationScreen {
    Home,
    Workbench,
};

/*** Owns an optional, fully constructed GUI simulation session. ***/
class GuiApplicationState {
  public:
    GuiApplicationState() = default;

    explicit GuiApplicationState(std::unique_ptr<GuiSimulationSession> session) {
        replace_session(std::move(session));
    }

    GuiApplicationScreen screen() const noexcept {
        return session_ == nullptr ? GuiApplicationScreen::Home : GuiApplicationScreen::Workbench;
    }

    bool has_active_session() const noexcept { return session_ != nullptr; }

    GuiSimulationSession& active_session() {
        if (session_ == nullptr) {
            throw std::logic_error{"the GUI has no active simulation session"};
        }
        return *session_;
    }

    const GuiSimulationSession& active_session() const {
        if (session_ == nullptr) {
            throw std::logic_error{"the GUI has no active simulation session"};
        }
        return *session_;
    }

    // The caller constructs and validates the replacement before ownership changes.
    void replace_session(std::unique_ptr<GuiSimulationSession> replacement) {
        if (replacement == nullptr) {
            throw std::invalid_argument{"a replacement GUI session must not be empty"};
        }
        session_ = std::move(replacement);
    }

    void clear_session() noexcept { session_.reset(); }

  private:
    std::unique_ptr<GuiSimulationSession> session_;
};

} // namespace cpssim
