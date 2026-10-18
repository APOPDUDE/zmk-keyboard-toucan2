# The swipe-keys input processor. Built into every half; it only runs where the devicetree has an instance
# (the left half's trackpad listener, boards/shields/toucan/toucan.dtsi).
zephyr_library_named(toucan_swipe_keys)
zephyr_library_include_directories(${APPLICATION_SOURCE_DIR}/include)
zephyr_library_sources(src/input_processor_swipe_keys.c)
