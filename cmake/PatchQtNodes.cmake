if(NOT DEFINED CPSSIM_QTNODES_SOURCE_DIR OR NOT DEFINED CPSSIM_QTNODES_PATCH)
    message(FATAL_ERROR "QtNodes patch source and patch file are required")
endif()

execute_process(
    COMMAND git apply --check --ignore-space-change --whitespace=nowarn
            "${CPSSIM_QTNODES_PATCH}"
    WORKING_DIRECTORY "${CPSSIM_QTNODES_SOURCE_DIR}"
    RESULT_VARIABLE apply_check
    OUTPUT_QUIET
    ERROR_QUIET
)
if(apply_check EQUAL 0)
    execute_process(
        COMMAND git apply --ignore-space-change --whitespace=nowarn
                "${CPSSIM_QTNODES_PATCH}"
        WORKING_DIRECTORY "${CPSSIM_QTNODES_SOURCE_DIR}"
        COMMAND_ERROR_IS_FATAL ANY
    )
    return()
endif()

execute_process(
    COMMAND git apply --reverse --check --ignore-space-change --whitespace=nowarn
            "${CPSSIM_QTNODES_PATCH}"
    WORKING_DIRECTORY "${CPSSIM_QTNODES_SOURCE_DIR}"
    RESULT_VARIABLE reverse_check
    OUTPUT_QUIET
    ERROR_QUIET
)
if(NOT reverse_check EQUAL 0)
    message(FATAL_ERROR "Pinned QtNodes patch does not apply to the selected source")
endif()
