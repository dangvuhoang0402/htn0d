set(COMPONENT_SRCS "app.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
spiffs_create_partition_image(storage ../partition FLASH_IN_PROJECT)
