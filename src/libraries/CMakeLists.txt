include(ExternalProject)
set(pwd "${CMAKE_CURRENT_SOURCE_DIR}")

#### MUPARSER

set(muparser_dir "${pwd}/muparser")

file(GLOB_RECURSE headers ${muparser_dir}/include/*.h)
set(MUPARSER_HEADERS ${headers} CACHE INTERNAL "")

prepend(sources ${muparser_dir}/src/ muParser muParserBase
  muParserBytecode muParserCallback muParserError
  muParserInt muParserTokenReader)
set(MUPARSER_SOURCES ${sources} CACHE INTERNAL "")

source_group(muparser FILES ${headers} ${sources})
set(MUPARSER_INCLUDE_DIRS "${muparser_dir}/include" CACHE INTERNAL "")

include_directories(${MUPARSER_INCLUDE_DIRS})
add_library(inexor_muparser STATIC ${MUPARSER_SOURCES})

function(config_muparser targ)
  message(STATUS "Configuring ${targ} with muparser")

  include_directories(${MUPARSER_INCLUDE_DIRS})
  target_link_libraries_wrapper(${targ} inexor_muparser)
endfunction()

#### Googletest

set(GTEST_DIR "${pwd}/googletest" CACHE INTERNAL "")
set(GTEST_INCLUDE_DIRS "${GTEST_DIR}/include"
  CACHE INTERNAL "")

set(gtest_args
      -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      -Dgtest_disable_pthreads=true)
if (CMAKE_TOOLCHAIN_FILE)
  list(APPEND gtest_args
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

ExternalProject_Add(build_gtest
  SOURCE_DIR "${GTEST_DIR}"
  INSTALL_COMMAND ""
  CMAKE_ARGS ${gtest_args})

# This is a it of a hack to find the actual location of the
# built library
ExternalProject_Get_Property(build_gtest BINARY_DIR)
set(GTEST_LIB_DIR ${BINARY_DIR} CACHE INTERN "")

function(config_gtest targ)
  message(STATUS "Configuring ${targ} with google test")

  add_dependencies(${targ} build_gtest)
  include_directories(${GTEST_INCLUDE_DIRS})
  target_link_libraries_wrapper(${targ} gtest)
endfunction()

#### CEF

set(CEF_INCLUDE_DIRS "${SOURCE_DIR}/libraries/cef3" CACHE INTERN "")
include_directories(${CEF_INCLUDE_DIRS})

if(NOT DEFINED LIBCEF_DLL_LIBRARIES AND NOT DEFINED LOCAL_LIBCEF_DLL)
  set(LOCAL_LIBCEF_DLL 1 CACHE INTERNAL "Use local libcef_dll library")
elseif(DEFINED LOCAL_LIBCEF_DLL)
  set(LOCAL_LIBCEF_DLL ${LOCAL_LIBCEF_DLL} CACHE INTERNAL "Use local libcef_dll library")
else()
  set(LOCAL_LIBCEF_DLL 0 CACHE INTERNAL "Use local libcef_dll library")
endif()

if(${LOCAL_LIBCEF_DLL})
    message(STATUS "Building local libcef_dll")
    add_subdirectory(${SOURCE_DIR}/libraries/cef3/libcef_dll)
endif()

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CEF_DLL_LIBRARY "libcef_dll_wrapper")
set(CEF_BASE_DIR "${PROJECT_SOURCE_DIR}/src/libraries/cef3")
set(ICUDTL_DAT "${CEF_BASE_DIR}/Resources/icudtl.dat")
if(OS_LINUX)
  set(CEF_LINUX_DIR "${CEF_BASE_DIR}/cef_bin/linux")
  if(X64)
    set(CEF_LINUX_X86_64_DIR "${CEF_LINUX_DIR}/bin_x86_64")
    set(UDEV_LIBRARY "${CEF_LINUX_X86_64_DIR}/libudev.so")
    set(CEF_LIBRARY "${CEF_LINUX_X86_64_DIR}/libcef.so")
    set(FFMPEGSUMO_LIBRARY "${CEF_LINUX_X86_64_DIR}/libffmpegsumo.so")
    set(CHROME_SANDBOX_BIN "${CEF_LINUX_X86_64_DIR}/chrome-sandbox")
  else()
    set(CEF_LINUX_X86_DIR "${CEF_LINUX_DIR}/bin_x86")
    set(UDEV_LIBRARY "${CEF_LINUX_X86_DIR}/libudev.so")
    set(CEF_LIBRARY "${CEF_LINUX_X86_DIR}/libcef.so")
    set(FFMPEGSUMO_LIBRARY "${CEF_LINUX_X86_DIR}/libffmpegsumo.so")
    set(CHROME_SANDBOX_BIN "${CEF_LINUX_X86_DIR}/chrome-sandbox")
  endif()
elseif(OS_WINDOWS)
  if(X64)
    set(CEF_LIBRARY "${CEF_BASE_DIR}/cef_bin/windows/lib64/libcef.lib")
  else()
    set(CEF_LIBRARY "${CEF_BASE_DIR}/cef_bin/windows/lib32/libcef.lib")
  endif()
endif()

set(CEF_LIBRARIES "${CEF_DLL_LIBRARY};-Wl,-rpath,. ${CEF_LIBRARY};-Wl,-rpath,. ${UDEV_LIBRARY}" CACHE INTERNAL "")

function(config_cef targ)
  message(STATUS "Configuring ${targ} with CEF")

  include_directories(${CEF_INCLUDE_DIRS})

  if (NOT ";${ARGN};" MATCHES ";NOLINK;")
    target_link_libraries_wrapper(${targ} ${CEF_LIBRARIES})

    install(TARGETS ${UDEV_LIBRARY} ${CEF_LIBRARY}
      ${FFMPEGSUMO_LIBRARY} ${CHROME_SANDBOX_BIN}
      ${ICUDTL_DAT} DESTINATION ${EXE_DIR})
  endif()
endfunction()
