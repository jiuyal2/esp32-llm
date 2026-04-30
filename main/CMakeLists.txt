idf_component_register(SRCS "main.c" "llm.c"
                    INCLUDE_DIRS ""  LDFRAGMENTS "../linker.lf")

# https://github.com/espressif/esp-idf/issues/11696#issuecomment-1596208414
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-if-conversion)

# SPIFFS image: tokenizer only (model lives in the raw 'model' partition)
spiffs_create_partition_image(data ../spiffs_data FLASH_IN_PROJECT)

# Flash model binary directly to raw 'model' partition (no RAM needed at runtime)
esptool_py_flash_to_partition(flash "model" "${CMAKE_CURRENT_SOURCE_DIR}/../data/stories260K.bin")
