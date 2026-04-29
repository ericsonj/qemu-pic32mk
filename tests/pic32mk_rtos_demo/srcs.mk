####################################################
#         firmware/src/app_mk.py:AppModule         #
####################################################
CSRC += firmware/src/main.c
CSRC += firmware/src/usb_device_init_data.c
CSRC += firmware/src/usb_init.c

SRC_DIRS += firmware/src

INCS += -Ifirmware/src



####################################################
#firmware/src/third_party/rtos/FreeRTOS/Source/freertos_mk.py:FreeRTOSModule#
####################################################
CSRC += firmware/src/third_party/rtos/FreeRTOS/Source/croutine.c
CSRC += firmware/src/third_party/rtos/FreeRTOS/Source/list.c
CSRC += firmware/src/third_party/rtos/FreeRTOS/Source/queue.c
CSRC += firmware/src/third_party/rtos/FreeRTOS/Source/FreeRTOS_tasks.c
CSRC += firmware/src/third_party/rtos/FreeRTOS/Source/timers.c
CSRC += firmware/src/third_party/rtos/FreeRTOS/Source/event_groups.c
CSRC += firmware/src/third_party/rtos/FreeRTOS/Source/stream_buffer.c

SRC_DIRS += firmware/src/third_party/rtos/FreeRTOS/Source

INCS += -Ifirmware/src/third_party/rtos/FreeRTOS/Source/include



####################################################
#firmware/src/third_party/rtos/FreeRTOS/Source/portable/portable_mk.py:FreeRTOSPortModule#
####################################################
CSRC += firmware/src/third_party/rtos/FreeRTOS/Source/portable/MemMang/heap_1.c
CSRC += firmware/src/third_party/rtos/FreeRTOS/Source/portable/MPLAB/PIC32MK/port.c

SRC_DIRS += firmware/src/third_party/rtos/FreeRTOS/Source/portable/MemMang
SRC_DIRS += firmware/src/third_party/rtos/FreeRTOS/Source/portable/MPLAB/PIC32MK

INCS += -Ifirmware/src/third_party/rtos/FreeRTOS/Source/portable/MPLAB/PIC32MK



####################################################
#  firmware/src/config/config_mk.py:ConfigModule   #
####################################################
CSRC += firmware/src/config/default/libc_stubs.c
CSRC += firmware/src/config/default/driver/usb/usbfs/src/drv_usbfs.c
CSRC += firmware/src/config/default/driver/usb/usbfs/src/drv_usbfs_device.c
CSRC += firmware/src/config/default/osal/osal_freertos.c
CSRC += firmware/src/config/default/peripheral/nvm/plib_nvm.c
CSRC += firmware/src/config/default/peripheral/uart/plib_uart2.c
CSRC += firmware/src/config/default/peripheral/uart/plib_uart1.c
CSRC += firmware/src/config/default/peripheral/spi/plib_spi5_slave.c
CSRC += firmware/src/config/default/peripheral/icap/plib_icap2.c
CSRC += firmware/src/config/default/peripheral/evic/plib_evic.c
CSRC += firmware/src/config/default/peripheral/tmr/plib_tmr6.c
CSRC += firmware/src/config/default/peripheral/tmr/plib_tmr9.c
CSRC += firmware/src/config/default/peripheral/tmr/plib_tmr4.c
CSRC += firmware/src/config/default/peripheral/tmr/plib_tmr8.c
CSRC += firmware/src/config/default/peripheral/tmr/plib_tmr3.c
CSRC += firmware/src/config/default/peripheral/tmr/plib_tmr7.c
CSRC += firmware/src/config/default/peripheral/tmr/plib_tmr5.c
CSRC += firmware/src/config/default/peripheral/tmr/plib_tmr2.c
CSRC += firmware/src/config/default/peripheral/adchs/plib_adchs.c
CSRC += firmware/src/config/default/peripheral/clk/plib_clk.c
CSRC += firmware/src/config/default/peripheral/tmr1/plib_tmr1.c
CSRC += firmware/src/config/default/peripheral/ocmp/plib_ocmp12.c
CSRC += firmware/src/config/default/peripheral/ocmp/plib_ocmp2.c
CSRC += firmware/src/config/default/peripheral/ocmp/plib_ocmp16.c
CSRC += firmware/src/config/default/peripheral/ocmp/plib_ocmp3.c
CSRC += firmware/src/config/default/peripheral/ocmp/plib_ocmp11.c
CSRC += firmware/src/config/default/peripheral/ocmp/plib_ocmp6.c
CSRC += firmware/src/config/default/peripheral/ocmp/plib_ocmp7.c
CSRC += firmware/src/config/default/peripheral/ocmp/plib_ocmp1.c
CSRC += firmware/src/config/default/peripheral/ocmp/plib_ocmp5.c
CSRC += firmware/src/config/default/peripheral/ocmp/plib_ocmp13.c
CSRC += firmware/src/config/default/peripheral/ocmp/plib_ocmp4.c
CSRC += firmware/src/config/default/peripheral/eeprom/plib_eeprom.c
CSRC += firmware/src/config/default/peripheral/gpio/plib_gpio.c
CSRC += firmware/src/config/default/peripheral/wdt/plib_wdt.c
CSRC += firmware/src/config/default/peripheral/canfd/plib_canfd3.c
CSRC += firmware/src/config/default/peripheral/canfd/plib_canfd4.c
CSRC += firmware/src/config/default/peripheral/canfd/plib_canfd2.c
CSRC += firmware/src/config/default/peripheral/canfd/plib_canfd1.c
CSRC += firmware/src/config/default/usb/src/usb_device.c
CSRC += firmware/src/config/default/usb/src/usb_device_cdc_acm.c
CSRC += firmware/src/config/default/usb/src/usb_device_cdc.c
ASSRC += firmware/src/config/default/crt0.S
ASSRC += firmware/src/config/default/port_asm_patched.S

SRC_DIRS += firmware/src/config/default/peripheral/canfd
SRC_DIRS += firmware/src/config/default
SRC_DIRS += firmware/src/config/default/driver/usb/usbfs/src
SRC_DIRS += firmware/src/config/default/peripheral/uart
SRC_DIRS += firmware/src/config/default/peripheral/evic
SRC_DIRS += firmware/src/config/default/peripheral/adchs
SRC_DIRS += firmware/src/config/default/peripheral/ocmp
SRC_DIRS += firmware/src/config/default/peripheral/eeprom
SRC_DIRS += firmware/src/config/default/peripheral/tmr1
SRC_DIRS += firmware/src/config/default/peripheral/clk
SRC_DIRS += firmware/src/config/default/peripheral/tmr
SRC_DIRS += firmware/src/config/default/osal
SRC_DIRS += firmware/src/config/default/peripheral/nvm
SRC_DIRS += firmware/src/config/default/peripheral/spi
SRC_DIRS += firmware/src/config/default/peripheral/gpio
SRC_DIRS += firmware/src/config/default/peripheral/icap
SRC_DIRS += firmware/src/config/default/peripheral/wdt
SRC_DIRS += firmware/src/config/default/usb/src

INCS += -Ifirmware/src/config/default
INCS += -Ifirmware/src/config/default/driver
INCS += -Ifirmware/src/config/default/driver/usb
INCS += -Ifirmware/src/config/default/driver/usb/usbfs
INCS += -Ifirmware/src/config/default/driver/usb/usbfs/src
INCS += -Ifirmware/src/config/default/driver/usb/usbfs/src/templates
INCS += -Ifirmware/src/config/default/osal
INCS += -Ifirmware/src/config/default/sys
INCS += -Ifirmware/src/config/default/gnu
INCS += -Ifirmware/src/config/default/system
INCS += -Ifirmware/src/config/default/system/debug
INCS += -Ifirmware/src/config/default/system/int
INCS += -Ifirmware/src/config/default/system/reset
INCS += -Ifirmware/src/config/default/peripheral/nvm
INCS += -Ifirmware/src/config/default/peripheral/uart
INCS += -Ifirmware/src/config/default/peripheral/spi
INCS += -Ifirmware/src/config/default/peripheral/icap
INCS += -Ifirmware/src/config/default/peripheral/evic
INCS += -Ifirmware/src/config/default/peripheral/tmr
INCS += -Ifirmware/src/config/default/peripheral/adchs
INCS += -Ifirmware/src/config/default/peripheral/clk
INCS += -Ifirmware/src/config/default/peripheral/tmr1
INCS += -Ifirmware/src/config/default/peripheral/ocmp
INCS += -Ifirmware/src/config/default/peripheral/eeprom
INCS += -Ifirmware/src/config/default/peripheral/gpio
INCS += -Ifirmware/src/config/default/peripheral/wdt
INCS += -Ifirmware/src/config/default/peripheral/canfd
INCS += -Ifirmware/src/config/default/usb
INCS += -Ifirmware/src/config/default/usb/src



