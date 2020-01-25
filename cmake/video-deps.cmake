#~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=
# Manage the freetype, SDL2, SDL2_ttf dependencies
#
# (a) Try to locate the system's installed libraries.
# (b) Build dependent libraries, if not found.
#~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=~=

add_library(simh_video INTERFACE)
add_library(png_lib INTERFACE)

if (WITH_VIDEO)
    find_package (PNG)
    if (NOT PNG_FOUND AND PKG_CONFIG_FOUND)
        pkg_check_modules(PNG IMPORTED_TARGET libpng)
    endif (NOT PNG_FOUND AND PKG_CONFIG_FOUND)

    if (PNG_FOUND)
        if (TARGET PkgConfig::PNG)
            target_link_libraries(png_lib INTERFACE PkgConfig::PNG)
        else (TARGET PkgConfig::PNG)
            target_include_directories(png_lib INTERFACE ${PNG_INCLUDE_DIRS})
            target_link_libraries(png_lib INTERFACE ${PNG_LIBRARIES})
        endif (TARGET PkgConfig::PNG)

        target_compile_definitions(png_lib INTERFACE ${PNG_DEFINITIONS} HAVE_LIBPNG)
        target_link_libraries(simh_video INTERFACE png_lib)

        set(VIDEO_PKG_STATUS "installed PNG")
    else ()
        set(PNG_DEPS)
        if (NOT ZLIB_FOUND)
            list(APPEND PNG_DEPS zlib-dep)
        endif (NOT ZLIB_FOUND)

        set(PNG_SOURCE_URL "https://sourceforge.net/projects/libpng/files/libpng16/1.6.37/libpng-1.6.37.tar.xz/download")
        set(PNG_CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE})
        if (CMAKE_C_COMPILER_ID STREQUAL "GNU" AND
            CMAKE_C_COMPILER_VERSION VERSION_EQUAL "8.1" AND
            NOT CMAKE_BUILD_VERSION)
            message(STATUS "PNG: Build using MinSizeRel CMAKE_BUILD_TYPE with GCC 8.1")
            set(PNG_CMAKE_BUILD_TYPE "MinSizeRel")
        endif()

        ExternalProject_Add(png-dep
            URL ${PNG_SOURCE_URL}
            CMAKE_ARGS 
                ${DEP_CMAKE_ARGS}
                -DCMAKE_INSTALL_PREFIX=${SIMH_DEP_TOPDIR}
                -DCMAKE_PREFIX_PATH=${SIMH_PREFIX_PATH_LIST}
                -DCMAKE_INCLUDE_PATH=${SIMH_INCLUDE_PATH_LIST}
                -DCMAKE_BUILD_TYPE=${PNG_CMAKE_BUILD_TYPE}
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            DEPENDS
                ${PNG_DEPS}
        )

        list(APPEND SIMH_BUILD_DEPS "png")
        list(APPEND SIMH_DEP_TARGETS "png-dep")
        message(STATUS "Building PNG from ${PNG_SOURCE_URL}")
        set(VIDEO_PKG_STATUS "PNG source build")
    endif ()

    find_package(Freetype)
    if (NOT FREETYPE_FOUND AND PKG_CONFIG_FOUND)
        pkg_check_modules(FREETYPE IMPORTED_TARGET freetype2)
    endif (NOT FREETYPE_FOUND AND PKG_CONFIG_FOUND)

    if (FREETYPE_FOUND)
        if (TARGET Freetype::Freetype)
            target_link_libraries(simh_video INTERFACE Freetype::Freetype)
        elseif (TARGET PkgConfig::FREETYPE)
            target_link_libraries(simh_video INTERFACE PkgConfig::FREETYPE)
        endif (TARGET Freetype::Freetype)

        set(VIDEO_PKG_STATUS "${VIDEO_PKG_STATUS}, installed Freetype")
    else (FREETYPE_FOUND)
        if (NOT PNG_FOUND)
            list(APPEND FREETYPE_DEPS png-dep)
        endif (NOT PNG_FOUND)

        ExternalProject_Add(freetype-dep
            GIT_REPOSITORY https://git.sv.nongnu.org/r/freetype/freetype2.git
            GIT_TAG VER-2-10-1
            CMAKE_ARGS 
                ${DEP_CMAKE_ARGS}
                -DCMAKE_INSTALL_PREFIX=${SIMH_DEP_TOPDIR}
                -DCMAKE_PREFIX_PATH=${SIMH_PREFIX_PATH_LIST}
                -DCMAKE_INCLUDE_PATH=${SIMH_INCLUDE_PATH_LIST}
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            DEPENDS
                ${FREETYPE_DEPS}
        )

        list(APPEND SIMH_BUILD_DEPS "freetype")
        list(APPEND SIMH_DEP_TARGETS "freetype-dep")
        message(STATUS "Building Freetype from github repository.")
        set(VIDEO_PKG_STATUS "${VIDEO_PKG_STATUS}, Freetype source build")
    endif (FREETYPE_FOUND)

    find_package(SDL2 CONFIG)
    if (NOT SDL2_FOUND AND PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 IMPORTED_TARGET SDL2)
    endif (NOT SDL2_FOUND AND PKG_CONFIG_FOUND)

    find_package(SDL2_ttf CONFIG)
    IF (NOT SDL2_ttf_FOUND AND PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2_ttf IMPORTED_TARGET SDL2_ttf)
    ENDIF (NOT SDL2_ttf_FOUND AND PKG_CONFIG_FOUND)
    
    IF (SDL2_FOUND AND SDL2_ttf_FOUND)
        target_compile_definitions(simh_video INTERFACE USE_SIM_VIDEO HAVE_LIBSDL)

        IF (TARGET SDL2_ttf::SDL2_ttf)
            target_link_libraries(simh_video INTERFACE SDL2_ttf::SDL2_ttf)
        ELSEIF (TARGET PkgConfig::SDL2_ttf)
            target_link_libraries(simh_video INTERFACE PkgConfig::SDL2_ttf)
        ELSEIF (DEFINED SDL2_ttf_LIBRARIES AND DEFINED SDL2_ttf_INCLUDE_DIRS)
            ## target_link_directories(simh_video INTERFACE ${SDL2_ttf_LIBDIR})
            target_link_libraries(simh_video INTERFACE ${SDL2_ttf_LIBRARIES})
            target_include_directories(simh_video INTERFACE ${SDL2_ttf_INCLUDE_DIRS})
        ELSE ()
            message(FATAL_ERROR "SDL2_ttf_FOUND set but no SDL2_ttf::SDL2_ttf import library or SDL2_ttf_LIBRARIES/SDL2_ttf_INCLUDE_DIRS? ")
        ENDIF ()

        IF (TARGET SDL2::SDL2)
            target_link_libraries(simh_video INTERFACE SDL2::SDL2)
        ELSEIF (TARGET PkgConfig::SDL2)
            target_link_libraries(simh_video INTERFACE PkgConfig::SDL2)
        ELSEIF (DEFINED SDL2_LIBRARIES AND DEFINED SDL2_INCLUDE_DIRS)
            ## target_link_directories(simh_video INTERFACE ${SDL2_LIBDIR})
            target_link_libraries(simh_video INTERFACE ${SDL2_LIBRARIES})
            target_include_directories(simh_video INTERFACE ${SDL2_INCLUDE_DIRS})
        ELSE ()
            message(FATAL_ERROR "SDL2_FOUND set but no SDL2::SDL2 import library or SDL2_LIBRARIES/SDL2_INCLUDE_DIRS? ")
        ENDIF ()

        set(VIDEO_PKG_STATUS "${VIDEO_PKG_STATUS}, installed SDL2 and SDL2_ttf")
    ELSE (SDL2_FOUND AND SDL2_ttf_FOUND)
        IF (NOT SDL2_FOUND)
            ExternalProject_Add(sdl2-dep
                URL https://www.libsdl.org/release/SDL2-2.0.10.zip
                CMAKE_ARGS 
                    ${DEP_CMAKE_ARGS}
                    -DCMAKE_INSTALL_PREFIX=${SIMH_DEP_TOPDIR}
                    -DCMAKE_PREFIX_PATH=${SIMH_PREFIX_PATH_LIST}
                    -DCMAKE_INCLUDE_PATH=${SIMH_INCLUDE_PATH_LIST}
                    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            )

            list(APPEND SIMH_BUILD_DEPS "SDL2")
            list(APPEND SIMH_DEP_TARGETS "sdl2-dep")
            message(STATUS "Building SDL2 from https://www.libsdl.org/release/SDL2-2.0.10.zip.")
            set(VIDEO_PKG_STATUS "${VIDEO_PKG_STATUS}, SDL2 source build")
        ELSE (NOT SDL2_FOUND)
            set(VIDEO_PKG_STATUS "${VIDEO_PKG_STATUS}, installed SDL2")
        ENDIF (NOT SDL2_FOUND)

        IF (NOT SDL2_ttf_FOUND)
            set(SDL2_ttf_depdir ${CMAKE_BINARY_DIR}/sdl2-ttf-dep-prefix/src/sdl2-ttf-dep/)
            set(SDL2_ttf_DEPS)

            if (NOT SDL2_FOUND)
                list(APPEND SDL2_ttf_DEPS sdl2-dep)
            endif (NOT SDL2_FOUND)

            if (NOT FREETYPE_FOUND)
                list(APPEND SDL2_ttf_DEPS freetype-dep)
            endif (NOT FREETYPE_FOUND)

            ExternalProject_Add(sdl2-ttf-dep
                URL https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-2.0.15.zip
                CMAKE_ARGS 
                    ${DEP_CMAKE_ARGS}
                    -DCMAKE_INSTALL_PREFIX=${SIMH_DEP_TOPDIR}
                    -DCMAKE_PREFIX_PATH=${SIMH_PREFIX_PATH_LIST}
                    -DCMAKE_INCLUDE_PATH=${SIMH_INCLUDE_PATH_LIST}
                    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                UPDATE_COMMAND
                    ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/cmake/dep-patches/SDL2_ttf/SDL2_ttfConfig.cmake ${SDL2_ttf_depdir}
                DEPENDS
                    ${SDL2_ttf_DEPS}
            )

            list(APPEND SIMH_BUILD_DEPS "SDL2_ttf")
            list(APPEND SIMH_DEP_TARGETS "sdl2-ttf-dep")
            message(STATUS "Building SDL2_ttf from https://www.libsdl.org/release/SDL2_ttf-2.0.15.zip.")
            set(VIDEO_PKG_STATUS "${VIDEO_PKG_STATUS}, SDL2_ttf source build")
        ELSE (NOT SDL2_ttf_FOUND)
            set(VIDEO_PKG_STATUS "${VIDEO_PKG_STATUS}, installed SDL2_ttf")
        ENDIF (NOT SDL2_ttf_FOUND)
    ENDIF (SDL2_FOUND AND SDL2_ttf_FOUND)
endif ()
