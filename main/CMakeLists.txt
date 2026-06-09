idf_component_register(SRCS "barkeeperguide_main.c" "lcd1602.c" "Button.c" "Leds.c" "Scale.c"
                       INCLUDE_DIRS "." 
                       PRIV_REQUIRES freertos driver led_strip)
