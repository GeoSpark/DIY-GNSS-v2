if(CONFIG_RFM9x)
  # Add driver header files to the set of global include paths.
  zephyr_include_directories(include)

  zephyr_library()
  zephyr_library_sources(
          src/rfm9x.c
    )
endif()
