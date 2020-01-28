## git-commit-id.cmake
##
## Get the current Git commit hash code and commit time.
##
## makefile simh computes them for each build, but CMake only computes them
## when generating build environments. Might need/want to change this in the
## future.

# Add the commit ID and time:
execute_process(COMMAND "git" "log" "-1" "--pretty=%H"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE HAVE_GIT_COMMIT_HASH
    OUTPUT_VARIABLE SIMH_GIT_COMMIT_HASH)

execute_process(COMMAND "git" "log" "-1" "--pretty=%aI"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE HAVE_GIT_COMMIT_TIME
    OUTPUT_VARIABLE SIMH_GIT_COMMIT_TIME)

if (NOT (HAVE_GIT_COMMIT_HASH OR HAVE_GIT_COMMIT_TIME))
    string(STRIP ${SIMH_GIT_COMMIT_HASH} SIMH_GIT_COMMIT_HASH)
    string(STRIP ${SIMH_GIT_COMMIT_TIME} SIMH_GIT_COMMIT_TIME)

    message(STATUS "SIM_GIT_COMMIT_ID:   ${SIMH_GIT_COMMIT_HASH}")
    message(STATUS "SIM_GIT_COMMIT_TIME: ${SIMH_GIT_COMMIT_TIME}")

    file(WRITE ${CMAKE_BINARY_DIR}/build-stage/include/.git-commit-id.h
        "#define SIM_GIT_COMMIT_ID ${SIMH_GIT_COMMIT_HASH}\n"
        "#define SIM_GIT_COMMIT_TIME ${SIMH_GIT_COMMIT_TIME}\n")
    add_definitions(-DSIM_NEED_GIT_COMMIT_ID)
    set(SIM_GIT_COMMIT_ID_DIRECTORY "${CMAKE_BINARY_DIR}/include/build-stage")
else ()
    message(STATUS "SIM_GIT_COMMIT_ID not set.")
    message(STATUS "SIM_GIT_COMMIT_TIME not set.")
endif()
