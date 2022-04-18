cmake_minimum_required(VERSION 3.0)

###################################################################

if (DEFINED ENV{OPENORBIS})
    set(OPENORBIS $ENV{OPENORBIS})
    set(OO_PS4_TOOLCHAIN ${OPENORBIS})
elseif (DEFINED ENV{OO_PS4_TOOLCHAIN})
    set(OPENORBIS $ENV{OO_PS4_TOOLCHAIN})
    set(OO_PS4_TOOLCHAIN ${OPENORBIS})
else ()
    message(FATAL_ERROR "Please define either OPENORBIS or OO_PS4_TOOLCHAIN to point to your sdk path")
endif ()

list(APPEND CMAKE_MODULE_PATH "${OPENORBIS}/cmake")

set(PS4 TRUE)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(TARGET x86_64-pc-freebsd-elf)
set(CMAKE_SYSTEM_VERSION 12)
set(CMAKE_CROSSCOMPILING 1)

if (DEFINED ENV{PS4_TOOLCHAIN_BIN})
    set(TOOLCHAIN_BIN $ENV{PS4_TOOLCHAIN_BIN}/bin/)
elseif (EXISTS "${OPENORBIS}/bin/clang")
    set(TOOLCHAIN_BIN ${OPENORBIS}/bin/)
endif ()

set(CMAKE_ASM_COMPILER ${TOOLCHAIN_BIN}clang CACHE PATH "")
set(CMAKE_C_COMPILER ${TOOLCHAIN_BIN}clang CACHE PATH "")
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_BIN}clang++ CACHE PATH "")
set(CMAKE_LINKER ${TOOLCHAIN_BIN}ld.lld CACHE PATH "")
set(CMAKE_AR ${TOOLCHAIN_BIN}llvm-ar CACHE PATH "")
set(CMAKE_RANLIB ${TOOLCHAIN_BIN}llvm-ranlib CACHE PATH "")
set(CMAKE_STRIP ${TOOLCHAIN_BIN}llvm-strip CACHE PATH "")

set(CMAKE_LIBRARY_ARCHITECTURE x86_64 CACHE INTERNAL "abi")

set(CMAKE_FIND_ROOT_PATH ${OPENORBIS} ${OPENORBIS}/usr)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Shared libs not available")

###################################################################

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(PS4_COMMON_INCLUDES "-isysroot ${OPENORBIS} -isystem ${OPENORBIS}/include -I${OPENORBIS}/usr/include")
set(PS4_COMMON_FLAGS "--target=x86_64-pc-freebsd12-elf -D__PS4__ -D__OPENORBIS__ -D__ORBIS__ -fPIC -funwind-tables ${PS4_COMMON_INCLUDES}")
set(PS4_COMMON_LIBS "-L${OPENORBIS}/lib -L${OPENORBIS}/usr/lib -lc -lkernel")

set(CMAKE_C_FLAGS_INIT "${PS4_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${PS4_COMMON_FLAGS} -I${OPENORBIS}/include/c++/v1")
set(CMAKE_ASM_FLAGS_INIT "${PS4_COMMON_FLAGS}")

set(PS4_LINKER_FLAGS "-m elf_x86_64 -pie --eh-frame-hdr --script ${OPENORBIS}/link.x ${OPENORBIS}/lib/crt1.o")
# crt1.o may be already added to LDFLAGS from "ps4vars.sh", so remove LDFLAGS env (todo: find a better way...)
set(ENV{LDFLAGS} "" CACHE STRING FORCE)

set(CMAKE_EXE_LINKER_FLAGS_INIT "${PS4_LINKER_FLAGS} ${PS4_COMMON_LIBS}")
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> -o <TARGET> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> <LINK_LIBRARIES>") # <FLAGS>
set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_LINKER> -o <TARGET> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  <LINK_LIBRARIES>") # <FLAGS>

# Start find_package in config mode
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

# Set pkg-config for the same
find_program(PKG_CONFIG_EXECUTABLE NAMES openorbis-pkg-config HINTS "${OPENORBIS}/usr/bin")
if (NOT PKG_CONFIG_EXECUTABLE)
    message(WARNING "Could not find openorbis-pkg-config: try installing ps4-openorbis-pkg-config")
endif ()

function(add_self project)
    set(AUTH_INFO "000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000")
    add_custom_command(
            OUTPUT "${project}.self"
            COMMAND ${CMAKE_COMMAND} -E env "OO_PS4_TOOLCHAIN=${OPENORBIS}" "${TOOLCHAIN_BIN}create-fself" "-in=${project}" "-out=${project}.oelf" "--eboot" "eboot.bin" "--paid" "0x3800000000000035" "--authinfo" "${AUTH_INFO}"
            VERBATIM
            DEPENDS "${project}"
    )
    add_custom_target(
            "${project}_self" ALL
            DEPENDS "${project}.self"
    )
endfunction()
