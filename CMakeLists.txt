cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# The bootloader is created with ExternalProject_Add while the overridden IDF
# project() command is running. Some PlatformIO IDF packages defer loading this
# standard module, so make the dependency explicit for normal configure mode.
if(NOT CMAKE_SCRIPT_MODE_FILE)
    include(ExternalProject)
endif()

# Headers live in components/<name>/include/<file.h>, while source uses
# #include "<name>/<file.h>". Generate lightweight forwarding headers instead
# of copying headers: copied headers are different physical files, so #pragma
# once cannot prevent duplicate declarations when both include forms are used.
set(NS_INC "${CMAKE_BINARY_DIR}/ns_inc")
file(MAKE_DIRECTORY "${NS_INC}")
file(GLOB _comp_dirs "${CMAKE_CURRENT_SOURCE_DIR}/components/*")
foreach(_dir ${_comp_dirs})
    if(IS_DIRECTORY "${_dir}/include")
        get_filename_component(_name "${_dir}" NAME)
        file(MAKE_DIRECTORY "${NS_INC}/${_name}")
        file(GLOB _hdrs CONFIGURE_DEPENDS "${_dir}/include/*.h" "${_dir}/include/*.hpp")
        foreach(_h ${_hdrs})
            get_filename_component(_hn "${_h}" NAME)
            file(TO_CMAKE_PATH "${_h}" _h_include)
            file(WRITE "${NS_INC}/${_name}/${_hn}"
                 "#pragma once\n#include \"${_h_include}\"\n")
        endforeach()
    endif()
endforeach()
include_directories("${NS_INC}")

project(smart_device_firmware)

idf_build_set_property(INCLUDE_DIRECTORIES "${NS_INC}" APPEND)
