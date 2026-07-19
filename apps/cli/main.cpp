/***
 * File: apps/cli/main.cpp
 * Purpose: Provide the minimal command-line entry point for the current
 *          headless CPSSim application.
 * Creator: Chuanchao Gao
 * Documentation date: 2026-07-18
 * Notes: The CLI currently reports only the compiled project version. Later
 *        tasks may add experiment commands without moving simulator behavior
 *        into this application layer.
 ***/

#include "cpssim/core/version.hpp"

#include <iostream>

/***
 * Starts the command-line application.
 * Returns zero after printing the CPSSim version to standard output.
 ***/
int main() {
    std::cout << "CPSSim " << cpssim::version() << '\n';
    return 0;
}
