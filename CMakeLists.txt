# Copyright (c) 2025 Crab Labs LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_ADXL36X)
  # Export public headers for applications that include the module.
  zephyr_include_directories(include)
  if(TARGET drivers__sensor)
    target_sources(drivers__sensor PRIVATE src/adxl36x.c)
    target_include_directories(drivers__sensor PRIVATE include)
  else()
    zephyr_library()
    zephyr_library_sources(src/adxl36x.c)
    zephyr_library_include_directories(include)
  endif()
endif()
