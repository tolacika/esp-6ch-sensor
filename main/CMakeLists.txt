idf_component_register(SRCS "nvs_manager.c" "state_manager.c" "main.c" "wifi_manager.c" "status_led.c" "button_manager.c" "ntc_adc.c" "lcd.c" "server.c" "fatfs_manager.c" 
    INCLUDE_DIRS ".")

set(image_src ../default_files)

if(EXISTS ${image_src})
    fatfs_create_spiflash_image(storage ${image_src} FLASH_IN_PROJECT PRESERVE_TIME)
else()
    message(FATAL_ERROR "Image source directory does not exist.\n\nPossible solutions:\n1. Check the direcotry at ${image_src}.\n2. If you are using a build system to build the frontend application, make sure to run the build system before building the firmware.")
endif()

