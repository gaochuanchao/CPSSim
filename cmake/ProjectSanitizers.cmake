function(cpssim_enable_sanitizers target_name)
    if(NOT CPSSIM_ENABLE_SANITIZERS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            "${target_name}"
            PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
        )
        target_link_options(
            "${target_name}"
            PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
        )
    else()
        message(FATAL_ERROR "CPSSIM_ENABLE_SANITIZERS is unsupported by this compiler")
    endif()
endfunction()
