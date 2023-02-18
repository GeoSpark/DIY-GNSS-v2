if(CONFIG_PX1122R_DRIVER)
  # Add driver header files to the set of global include paths.
  zephyr_include_directories(include)

  zephyr_library()
  zephyr_library_sources(
    src/px1122r.c
    )
endif()
