declare_module(ui .)

add_library(inexor_ui STATIC ${UI_MODULE_SOURCES})
config_cef(inexor_ui NOLINK)

function(config_ui targ)
  message(STATUS "Configuring ${targ} with ui")
  config_cef(${targ})
  target_link_libraries_wrapper(${targ} inexor_ui ${CEF_LIBRARIES})
endfunction()
