function(cpssim_configure_clang_tidy)
    if(NOT CPSSIM_ENABLE_CLANG_TIDY)
        return()
    endif()

    find_program(CPSSIM_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)

    set(
        CMAKE_CXX_CLANG_TIDY
        "${CPSSIM_CLANG_TIDY_EXECUTABLE};--config-file=${PROJECT_SOURCE_DIR}/.clang-tidy"
        PARENT_SCOPE
    )
endfunction()
