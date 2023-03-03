if(CONFIG_MAX17048)
  # Add driver header files to the set of global include paths.
  zephyr_include_directories(include)

  zephyr_library()
  zephyr_library_sources(
          src/max17048.c
    )
endif()
