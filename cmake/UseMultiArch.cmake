if(UNIX AND NOT APPLE)
  include(GNUInstallDirs)
endif()

if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR lib CACHE STRING "Specify the output directory for libraries (default is lib)")
endif()

if(NOT CMAKE_INSTALL_ARCH_LIBDIR)
  if(NOT SYSTEM_NAME)
    if(MSVC OR DEFINED ENV{VSCMD_ARG_TGT_ARCH})
      set(HOST_SYSTEM_NAME $ENV{VSCMD_ARG_HOST_ARCH})
      set(SYSTEM_NAME $ENV{VSCMD_ARG_TGT_ARCH})
    else(MSVC OR DEFINED ENV{VSCMD_ARG_TGT_ARCH})
      execute_process(COMMAND cc -dumpmachine OUTPUT_VARIABLE HOST_SYSTEM_NAME OUTPUT_STRIP_TRAILING_WHITESPACE)
      execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpmachine OUTPUT_VARIABLE SYSTEM_NAME OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif(MSVC OR DEFINED ENV{VSCMD_ARG_TGT_ARCH})
  endif(NOT SYSTEM_NAME)

  if(CMAKE_CROSSCOMPILING)
    if(NOT "${HOST_SYSTEM_NAME}" STREQUAL "${SYSTEM_NAME}")
      string(REGEX REPLACE i686 i386 CMAKE_CROSS_ARCH "${SYSTEM_NAME}")
    endif()

    set(CMAKE_CROSS_ARCH "${CMAKE_CROSS_ARCH}" CACHE STRING "Cross compiling target")
  endif()

  if(SYSTEM_NAME AND NOT "${SYSTEM_NAME}" STREQUAL "")
    set(CMAKE_INSTALL_LIBDIR lib/${SYSTEM_NAME})
    set(CMAKE_INSTALL_ARCH_LIBDIR lib/${SYSTEM_NAME} CACHE STRING "Architecture specific libraries")
  endif(SYSTEM_NAME AND NOT "${SYSTEM_NAME}" STREQUAL "")
endif(NOT CMAKE_INSTALL_ARCH_LIBDIR)

#message("${CMAKE_C_COMPILER}: ${CMAKE_C_COMPILER}")
#message("Architecture-specific library directory: ${CMAKE_INSTALL_ARCH_LIBDIR}")
