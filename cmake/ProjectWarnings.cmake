function(cpssim_set_project_warnings target_name)
    if(MSVC)
        set(project_warnings /W4 /permissive-)

        if(CPSSIM_WARNINGS_AS_ERRORS)
            list(APPEND project_warnings /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set(
            project_warnings
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
            -Wsign-conversion
        )

        if(CPSSIM_WARNINGS_AS_ERRORS)
            list(APPEND project_warnings -Werror)
        endif()
    else()
        message(WARNING "No project warning set for ${CMAKE_CXX_COMPILER_ID}")
    endif()

    target_compile_options("${target_name}" PRIVATE ${project_warnings})
endfunction()
