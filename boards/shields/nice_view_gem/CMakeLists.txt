if(CONFIG_ZMK_DISPLAY AND CONFIG_NICE_VIEW_WIDGET_STATUS)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  zephyr_library_sources(custom_status_screen.c)
  zephyr_library_sources(assets/images.c)
  zephyr_library_sources(widgets/util.c)

  if(CONFIG_TOUCAN_STATUS_SCREEN EQUAL 0)
    set(TOUCAN_STATUS_SCREEN_WIDGET_SOURCES
      widgets/battery.c
      widgets/battery_peripheral.c
      widgets/layer.c
      widgets/output.c
      widgets/profile.c
      widgets/sleep.c
    )
  elseif(CONFIG_TOUCAN_STATUS_SCREEN EQUAL 1)
    set(TOUCAN_STATUS_SCREEN_WIDGET_SOURCES
      widgets/battery.c
      widgets/battery_peripheral.c
      widgets/layer_logo.c
      widgets/output.c
      widgets/profile.c
      widgets/sleep.c
    )
  elseif(CONFIG_TOUCAN_STATUS_SCREEN EQUAL 2)
    set(TOUCAN_STATUS_SCREEN_WIDGET_SOURCES
      widgets/battery_arc.c
      widgets/battery_arc_peripheral.c
      widgets/chart.c
      widgets/layer_arc.c
      widgets/output_arc.c
      widgets/profile_arc.c
      widgets/sleep.c
    )
  elseif(CONFIG_TOUCAN_STATUS_SCREEN EQUAL 3)
    set(TOUCAN_STATUS_SCREEN_WIDGET_SOURCES
      widgets/battery_pixel.c
      widgets/battery_peripheral_pixel.c
      widgets/layer_pixel.c
      widgets/output_pixel.c
      widgets/profile_pixel.c
      widgets/sleep_pixel.c
    )
  endif()

  zephyr_library_sources(${TOUCAN_STATUS_SCREEN_WIDGET_SOURCES})

  if(NOT CONFIG_ZMK_SPLIT OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zephyr_library_sources(widgets/screen.c)
  endif()
endif()
