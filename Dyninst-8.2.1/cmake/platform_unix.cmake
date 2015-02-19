set (PLATFORM $ENV{PLATFORM})

set (VALID_PLATFORMS
    amd64-unknown-freebsd7.2 
    i386-unknown-freebsd7.2 
    i386-unknown-linux2.4 
    ppc32_linux 
    ppc64_linux 
    x86_64-unknown-linux2.4
    ppc64_bgq_ion
    )

if (NOT PLATFORM)
set (INVALID_PLATFORM true)
else()
list (FIND VALID_PLATFORMS ${PLATFORM} PLATFORM_FOUND)
  if (PLATFORM_FOUND EQUAL -1)
  set (INVALID_PLATFORM true)
  endif()
endif()


if (INVALID_PLATFORM)
# Try to set it automatically
execute_process (COMMAND ${DYNINST_ROOT}/scripts/sysname
                 OUTPUT_VARIABLE SYSNAME_OUT
                 )
execute_process (COMMAND ${DYNINST_ROOT}/scripts/dynsysname ${SYSNAME_OUT}
                 OUTPUT_VARIABLE DYNSYSNAME_OUT
                 )
string (REPLACE "\n" "" PLATFORM ${DYNSYSNAME_OUT})
message ("-- Attempting to automatically identify platform: ${PLATFORM}")
endif()

list (FIND VALID_PLATFORMS ${PLATFORM} PLATFORM_FOUND)

if (PLATFORM_FOUND EQUAL -1)
message (FATAL_ERROR "Error: unknown platform ${PLATFORM}; please set the PLATFORM environment variable to one of the following options: ${VALID_PLATFORMS}")
endif()

