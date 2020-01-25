#~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
# Manage the ZLIB dependency
#
# (a) Try to locate the system's installed zlib.
# (b) If system zlib isn't available, build it as an external project.
#~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=

include (FindZLIB)
if (NOT ZLIB_FOUND AND PKG_CONFIG_FOUND)
    pkg_check_modules(ZLIB IMPORTED_TARGET zlib)
endif (NOT ZLIB_FOUND AND PKG_CONFIG_FOUND)

add_library(zlib_lib INTERFACE)

if (ZLIB_FOUND)
    if (TARGET ZLIB::ZLIB)
        target_link_libraries(zlib_lib INTERFACE ZLIB::ZLIB)
    elseif (TARGET PkgConfig::ZLIB)
        target_link_libraries(zlib_lib INTERFACE PkgConfig::ZLIB)
    else (TARGET ZLIB::ZLIB)
        target_compile_definitions(zlib_lib INTERFACE ${ZLIB_INCLUDE_DIRS})
        target_link_libraries(zlib_lib INTERFACE ${ZLIB_LIBRARIES})
    endif (TARGET ZLIB::ZLIB)

    set(ZLIB_PKG_STATUS "installed ZLIB")
else (ZLIB_FOUND)
    include (ExternalProject)

    ExternalProject_Add(zlib-dep
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.2.11
        CMAKE_ARGS 
            ${DEP_CMAKE_ARGS}
            -DCMAKE_INSTALL_PREFIX=${SIMH_DEP_TOPDIR}
            -DCMAKE_PREFIX_PATH=${SIMH_PREFIX_PATH_LIST}
            -DCMAKE_INCLUDE_PATH=${SIMH_INCLUDE_PATH_LIST}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    )

    list(APPEND SIMH_BUILD_DEPS zlib)
    list(APPEND SIMH_DEP_TARGETS zlib-dep)
    message(STATUS "Building ZLIB from github repository.")
    set(ZLIB_PKG_STATUS "ZLIB source build")
endif (ZLIB_FOUND)
