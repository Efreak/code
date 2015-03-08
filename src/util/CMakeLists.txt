# UTIL Module ##############

declare_module(util .)

add_library(inexor_util STATIC ${UTIL_MODULE_SOURCES})

function(config_util targ)
  message(STATUS "Configuring ${targ} with util")

  target_link_libraries_wrapper(${targ} inexor_util)
endfunction()
