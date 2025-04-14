idf_component_register(SRCS "nvs_manager.c" "state_manager.c" "main.c"
  "wifi_manager.c" "status_led.c" "button_manager.c"
  "ntc_adc.c" "lcd.c" "server.c" "fatfs_manager.c"
  INCLUDE_DIRS ".")

set(image ../default_files)
fatfs_create_spiflash_image(storage ${image} FLASH_IN_PROJECT PRESERVE_TIME)