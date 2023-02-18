if(CONFIG_WIBBLE)
  # Add driver header files to the set of global include paths.
  zephyr_include_directories(.)

  zephyr_library()
  zephyr_library_sources(
    px1122r.c
    )
endif()
