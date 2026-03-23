/*******************************************************************************
  GPIO PLIB

  Company:
    Microchip Technology Inc.

  File Name:
    plib_gpio.h UUUUUUUUU

  Summary:
    GPIO PLIB Header File

  Description:
    This library provides an interface to control and interact with Parallel
    Input/Output controller (GPIO) module.

*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/

#ifndef PLIB_GPIO_H
#define PLIB_GPIO_H

#include <device.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// DOM-IGNORE-BEGIN
#ifdef __cplusplus  // Provide C++ Compatibility

extern "C" {
#endif
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Data types and constants
// *****************************************************************************
// *****************************************************************************


/*** Macros for GPI_NFLT_BMS pin ***/
#define GPI_NFLT_BMS_Set()               (LATGSET = (1U << 15))
#define GPI_NFLT_BMS_Clear()             (LATGCLR = (1U << 15))
#define GPI_NFLT_BMS_Toggle()            (LATGINV = (1U << 15))
#define GPI_NFLT_BMS_OutputEnable()      (TRISGCLR = (1U << 15))
#define GPI_NFLT_BMS_InputEnable()       (TRISGSET = (1U << 15))
#define GPI_NFLT_BMS_Get()               ((PORTG >> 15) & 0x1U)
#define GPI_NFLT_BMS_GetLatch()          ((LATG >> 15) & 0x1U)
#define GPI_NFLT_BMS_PIN                  GPIO_PIN_RG15
#define GPI_NFLT_BMS_InterruptEnable()   (CNENGSET = (1U << 15))
#define GPI_NFLT_BMS_InterruptDisable()  (CNENGCLR = (1U << 15))

/*** Macros for SPI_EN pin ***/
#define SPI_EN_Get()               ((PORTA >> 7) & 0x1U)
#define SPI_EN_GetLatch()          ((LATA >> 7) & 0x1U)
#define SPI_EN_PIN                  GPIO_PIN_RA7
#define SPI_EN_InterruptEnable()   (CNENASET = (1U << 7))
#define SPI_EN_InterruptDisable()  (CNENACLR = (1U << 7))

/*** Macros for GPI_KEY_RUN pin ***/
#define GPI_KEY_RUN_Set()               (LATBSET = (1U << 15))
#define GPI_KEY_RUN_Clear()             (LATBCLR = (1U << 15))
#define GPI_KEY_RUN_Toggle()            (LATBINV = (1U << 15))
#define GPI_KEY_RUN_OutputEnable()      (TRISBCLR = (1U << 15))
#define GPI_KEY_RUN_InputEnable()       (TRISBSET = (1U << 15))
#define GPI_KEY_RUN_Get()               ((PORTB >> 15) & 0x1U)
#define GPI_KEY_RUN_GetLatch()          ((LATB >> 15) & 0x1U)
#define GPI_KEY_RUN_PIN                  GPIO_PIN_RB15
#define GPI_KEY_RUN_InterruptEnable()   (CNENBSET = (1U << 15))
#define GPI_KEY_RUN_InterruptDisable()  (CNENBCLR = (1U << 15))

/*** Macros for GPI_KEY_START pin ***/
#define GPI_KEY_START_Set()               (LATDSET = (1U << 1))
#define GPI_KEY_START_Clear()             (LATDCLR = (1U << 1))
#define GPI_KEY_START_Toggle()            (LATDINV = (1U << 1))
#define GPI_KEY_START_OutputEnable()      (TRISDCLR = (1U << 1))
#define GPI_KEY_START_InputEnable()       (TRISDSET = (1U << 1))
#define GPI_KEY_START_Get()               ((PORTD >> 1) & 0x1U)
#define GPI_KEY_START_GetLatch()          ((LATD >> 1) & 0x1U)
#define GPI_KEY_START_PIN                  GPIO_PIN_RD1
#define GPI_KEY_START_InterruptEnable()   (CNENDSET = (1U << 1))
#define GPI_KEY_START_InterruptDisable()  (CNENDCLR = (1U << 1))

/*** Macros for GPI_BRAKE_NO pin ***/
#define GPI_BRAKE_NO_Set()               (LATDSET = (1U << 2))
#define GPI_BRAKE_NO_Clear()             (LATDCLR = (1U << 2))
#define GPI_BRAKE_NO_Toggle()            (LATDINV = (1U << 2))
#define GPI_BRAKE_NO_OutputEnable()      (TRISDCLR = (1U << 2))
#define GPI_BRAKE_NO_InputEnable()       (TRISDSET = (1U << 2))
#define GPI_BRAKE_NO_Get()               ((PORTD >> 2) & 0x1U)
#define GPI_BRAKE_NO_GetLatch()          ((LATD >> 2) & 0x1U)
#define GPI_BRAKE_NO_PIN                  GPIO_PIN_RD2
#define GPI_BRAKE_NO_InterruptEnable()   (CNENDSET = (1U << 2))
#define GPI_BRAKE_NO_InterruptDisable()  (CNENDCLR = (1U << 2))

/*** Macros for GPI_BRAKE_NC pin ***/
#define GPI_BRAKE_NC_Set()               (LATDSET = (1U << 3))
#define GPI_BRAKE_NC_Clear()             (LATDCLR = (1U << 3))
#define GPI_BRAKE_NC_Toggle()            (LATDINV = (1U << 3))
#define GPI_BRAKE_NC_OutputEnable()      (TRISDCLR = (1U << 3))
#define GPI_BRAKE_NC_InputEnable()       (TRISDSET = (1U << 3))
#define GPI_BRAKE_NC_Get()               ((PORTD >> 3) & 0x1U)
#define GPI_BRAKE_NC_GetLatch()          ((LATD >> 3) & 0x1U)
#define GPI_BRAKE_NC_PIN                  GPIO_PIN_RD3
#define GPI_BRAKE_NC_InterruptEnable()   (CNENDSET = (1U << 3))
#define GPI_BRAKE_NC_InterruptDisable()  (CNENDCLR = (1U << 3))

/*** Macros for GPO_WUP_BMS_PIN pin ***/
#define GPO_WUP_BMS_PIN_Set()               (LATDSET = (1U << 4))
#define GPO_WUP_BMS_PIN_Clear()             (LATDCLR = (1U << 4))
#define GPO_WUP_BMS_PIN_Toggle()            (LATDINV = (1U << 4))
#define GPO_WUP_BMS_PIN_OutputEnable()      (TRISDCLR = (1U << 4))
#define GPO_WUP_BMS_PIN_InputEnable()       (TRISDSET = (1U << 4))
#define GPO_WUP_BMS_PIN_Get()               ((PORTD >> 4) & 0x1U)
#define GPO_WUP_BMS_PIN_GetLatch()          ((LATD >> 4) & 0x1U)
#define GPO_WUP_BMS_PIN                     GPIO_PIN_RD4
#define GPO_WUP_BMS_PIN_InterruptEnable()   (CNENDSET = (1U << 4))
#define GPO_WUP_BMS_PIN_InterruptDisable()  (CNENDCLR = (1U << 4))

/*** Macros for IC_TRS_1 pin ***/
#define IC_TRS_1_Get()               ((PORTG >> 6) & 0x1U)
#define IC_TRS_1_GetLatch()          ((LATG >> 6) & 0x1U)
#define IC_TRS_1_PIN                  GPIO_PIN_RG6

/*** Macros for IC_TRS_2 pin ***/
#define IC_TRS_2_Get()               ((PORTG >> 7) & 0x1U)
#define IC_TRS_2_GetLatch()          ((LATG >> 7) & 0x1U)
#define IC_TRS_2_PIN                  GPIO_PIN_RG7

/*** Macros for GPI_CRASH pin ***/
#define GPI_CRASH_Set()               (LATGSET = (1U << 8))
#define GPI_CRASH_Clear()             (LATGCLR = (1U << 8))
#define GPI_CRASH_Toggle()            (LATGINV = (1U << 8))
#define GPI_CRASH_OutputEnable()      (TRISGCLR = (1U << 8))
#define GPI_CRASH_InputEnable()       (TRISGSET = (1U << 8))
#define GPI_CRASH_Get()               ((PORTG >> 8) & 0x1U)
#define GPI_CRASH_GetLatch()          ((LATG >> 8) & 0x1U)
#define GPI_CRASH_PIN                  GPIO_PIN_RG8
#define GPI_CRASH_InterruptEnable()   (CNENGSET = (1U << 8))
#define GPI_CRASH_InterruptDisable()  (CNENGCLR = (1U << 8))

/*** Macros for GPI_HVIL_REAR pin ***/
#define GPI_HVIL_REAR_Set()               (LATGSET = (1U << 10))
#define GPI_HVIL_REAR_Clear()             (LATGCLR = (1U << 10))
#define GPI_HVIL_REAR_Toggle()            (LATGINV = (1U << 10))
#define GPI_HVIL_REAR_OutputEnable()      (TRISGCLR = (1U << 10))
#define GPI_HVIL_REAR_InputEnable()       (TRISGSET = (1U << 10))
#define GPI_HVIL_REAR_Get()               ((PORTG >> 10) & 0x1U)
#define GPI_HVIL_REAR_GetLatch()          ((LATG >> 10) & 0x1U)
#define GPI_HVIL_REAR_PIN                  GPIO_PIN_RG10
#define GPI_HVIL_REAR_InterruptEnable()   (CNENGSET = (1U << 10))
#define GPI_HVIL_REAR_InterruptDisable()  (CNENGCLR = (1U << 10))

/*** Macros for GPI_HVIL_FRONT pin ***/
#define GPI_HVIL_FRONT_Set()               (LATESET = (1U << 8))
#define GPI_HVIL_FRONT_Clear()             (LATECLR = (1U << 8))
#define GPI_HVIL_FRONT_Toggle()            (LATEINV = (1U << 8))
#define GPI_HVIL_FRONT_OutputEnable()      (TRISECLR = (1U << 8))
#define GPI_HVIL_FRONT_InputEnable()       (TRISESET = (1U << 8))
#define GPI_HVIL_FRONT_Get()               ((PORTE >> 8) & 0x1U)
#define GPI_HVIL_FRONT_GetLatch()          ((LATE >> 8) & 0x1U)
#define GPI_HVIL_FRONT_PIN                  GPIO_PIN_RE8
#define GPI_HVIL_FRONT_InterruptEnable()   (CNENESET = (1U << 8))
#define GPI_HVIL_FRONT_InterruptDisable()  (CNENECLR = (1U << 8))

/*** Macros for GPI_HVIL_IDU pin ***/
#define GPI_HVIL_IDU_Set()               (LATESET = (1U << 9))
#define GPI_HVIL_IDU_Clear()             (LATECLR = (1U << 9))
#define GPI_HVIL_IDU_Toggle()            (LATEINV = (1U << 9))
#define GPI_HVIL_IDU_OutputEnable()      (TRISECLR = (1U << 9))
#define GPI_HVIL_IDU_InputEnable()       (TRISESET = (1U << 9))
#define GPI_HVIL_IDU_Get()               ((PORTE >> 9) & 0x1U)
#define GPI_HVIL_IDU_GetLatch()          ((LATE >> 9) & 0x1U)
#define GPI_HVIL_IDU_PIN                  GPIO_PIN_RE9
#define GPI_HVIL_IDU_InterruptEnable()   (CNENESET = (1U << 9))
#define GPI_HVIL_IDU_InterruptDisable()  (CNENECLR = (1U << 9))

/*** Macros for SPI_SDO pin ***/
#define SPI_SDO_Get()               ((PORTA >> 12) & 0x1U)
#define SPI_SDO_GetLatch()          ((LATA >> 12) & 0x1U)
#define SPI_SDO_PIN                  GPIO_PIN_RA12
#define SPI_SDO_InterruptEnable()   (CNENASET = (1U << 12))
#define SPI_SDO_InterruptDisable()  (CNENACLR = (1U << 12))

/*** Macros for SPI_SDI pin ***/
#define SPI_SDI_Get()               ((PORTA >> 11) & 0x1U)
#define SPI_SDI_GetLatch()          ((LATA >> 11) & 0x1U)
#define SPI_SDI_PIN                  GPIO_PIN_RA11
#define SPI_SDI_InterruptEnable()   (CNENASET = (1U << 11))
#define SPI_SDI_InterruptDisable()  (CNENACLR = (1U << 11))

/*** Macros for GPO_WDO_MB pin ***/
#define GPO_WDO_MB_Set()               (LATASET = (1U << 0))
#define GPO_WDO_MB_Clear()             (LATACLR = (1U << 0))
#define GPO_WDO_MB_Toggle()            (LATAINV = (1U << 0))
#define GPO_WDO_MB_OutputEnable()      (TRISACLR = (1U << 0))
#define GPO_WDO_MB_InputEnable()       (TRISASET = (1U << 0))
#define GPO_WDO_MB_Get()               ((PORTA >> 0) & 0x1U)
#define GPO_WDO_MB_GetLatch()          ((LATA >> 0) & 0x1U)
#define GPO_WDO_MB_PIN                  GPIO_PIN_RA0

/*** Macros for GPO_WDI_MB pin ***/
#define GPO_WDI_MB_Set()               (LATASET = (1U << 1))
#define GPO_WDI_MB_Clear()             (LATACLR = (1U << 1))
#define GPO_WDI_MB_Toggle()            (LATAINV = (1U << 1))
#define GPO_WDI_MB_OutputEnable()      (TRISACLR = (1U << 1))
#define GPO_WDI_MB_InputEnable()       (TRISASET = (1U << 1))
#define GPO_WDI_MB_Get()               ((PORTA >> 1) & 0x1U)
#define GPO_WDI_MB_GetLatch()          ((LATA >> 1) & 0x1U)
#define GPO_WDI_MB_PIN                  GPIO_PIN_RA1
#define GPO_WDI_MB_InterruptEnable()   (CNENASET = (1U << 1))
#define GPO_WDI_MB_InterruptDisable()  (CNENACLR = (1U << 1))

/*** Macros for UART_TX_HB_MB pin ***/
#define UART_TX_HB_MB_Get()               ((PORTB >> 0) & 0x1U)
#define UART_TX_HB_MB_GetLatch()          ((LATB >> 0) & 0x1U)
#define UART_TX_HB_MB_PIN                  GPIO_PIN_RB0

/*** Macros for UART_RX_HB_MB pin ***/
#define UART_RX_HB_MB_Get()               ((PORTB >> 1) & 0x1U)
#define UART_RX_HB_MB_GetLatch()          ((LATB >> 1) & 0x1U)
#define UART_RX_HB_MB_PIN                  GPIO_PIN_RB1

/*** Macros for ADC_NET_AAT pin ***/
#define ADC_NET_AAT_Get()               ((PORTF >> 9) & 0x1U)
#define ADC_NET_AAT_GetLatch()          ((LATF >> 9) & 0x1U)
#define ADC_NET_AAT_PIN                  GPIO_PIN_RF9

/*** Macros for ADC_PRESSURE pin ***/
#define ADC_PRESSURE_Get()               ((PORTF >> 10) & 0x1U)
#define ADC_PRESSURE_GetLatch()          ((LATF >> 10) & 0x1U)
#define ADC_PRESSURE_PIN                  GPIO_PIN_RF10

/*** Macros for GPI_BCM_WUP pin ***/
#define GPI_BCM_WUP_Set()               (LATCSET = (1U << 0))
#define GPI_BCM_WUP_Clear()             (LATCCLR = (1U << 0))
#define GPI_BCM_WUP_Toggle()            (LATCINV = (1U << 0))
#define GPI_BCM_WUP_OutputEnable()      (TRISCCLR = (1U << 0))
#define GPI_BCM_WUP_InputEnable()       (TRISCSET = (1U << 0))
#define GPI_BCM_WUP_Get()               ((PORTC >> 0) & 0x1U)
#define GPI_BCM_WUP_GetLatch()          ((LATC >> 0) & 0x1U)
#define GPI_BCM_WUP_PIN                  GPIO_PIN_RC0
#define GPI_BCM_WUP_InterruptEnable()   (CNENCSET = (1U << 0))
#define GPI_BCM_WUP_InterruptDisable()  (CNENCCLR = (1U << 0))

/*** Macros for ADC_THROTTLE_1 pin ***/
#define ADC_THROTTLE_1_Get()               ((PORTE >> 14) & 0x1U)
#define ADC_THROTTLE_1_GetLatch()          ((LATE >> 14) & 0x1U)
#define ADC_THROTTLE_1_PIN                  GPIO_PIN_RE14

/*** Macros for ADC_THROTTLE_2 pin ***/
#define ADC_THROTTLE_2_Get()               ((PORTE >> 15) & 0x1U)
#define ADC_THROTTLE_2_GetLatch()          ((LATE >> 15) & 0x1U)
#define ADC_THROTTLE_2_PIN                  GPIO_PIN_RE15

/*** Macros for OSC pin ***/
#define OSC_Get()               ((PORTC >> 12) & 0x1U)
#define OSC_GetLatch()          ((LATC >> 12) & 0x1U)
#define OSC_PIN                  GPIO_PIN_RC12

/*** Macros for GPIO_GND_CVH pin ***/
#define GPIO_GND_CVH_Set()               (LATASET = (1U << 14))
#define GPIO_GND_CVH_Clear()             (LATACLR = (1U << 14))
#define GPIO_GND_CVH_Toggle()            (LATAINV = (1U << 14))
#define GPIO_GND_CVH_OutputEnable()      (TRISACLR = (1U << 14))
#define GPIO_GND_CVH_InputEnable()       (TRISASET = (1U << 14))
#define GPIO_GND_CVH_Get()               ((PORTA >> 14) & 0x1U)
#define GPIO_GND_CVH_GetLatch()          ((LATA >> 14) & 0x1U)
#define GPIO_GND_CVH_PIN                  GPIO_PIN_RA14

/*** Macros for GPO_GND_DIFF_LOCK pin ***/
#define GPO_GND_DIFF_LOCK_Set()               (LATASET = (1U << 15))
#define GPO_GND_DIFF_LOCK_Clear()             (LATACLR = (1U << 15))
#define GPO_GND_DIFF_LOCK_Toggle()            (LATAINV = (1U << 15))
#define GPO_GND_DIFF_LOCK_OutputEnable()      (TRISACLR = (1U << 15))
#define GPO_GND_DIFF_LOCK_InputEnable()       (TRISASET = (1U << 15))
#define GPO_GND_DIFF_LOCK_Get()               ((PORTA >> 15) & 0x1U)
#define GPO_GND_DIFF_LOCK_GetLatch()          ((LATA >> 15) & 0x1U)
#define GPO_GND_DIFF_LOCK_PIN                  GPIO_PIN_RA15

/*** Macros for GPI_BTN_UP pin ***/
#define GPI_BTN_UP_Set()               (LATBSET = (1U << 6))
#define GPI_BTN_UP_Clear()             (LATBCLR = (1U << 6))
#define GPI_BTN_UP_Toggle()            (LATBINV = (1U << 6))
#define GPI_BTN_UP_OutputEnable()      (TRISBCLR = (1U << 6))
#define GPI_BTN_UP_InputEnable()       (TRISBSET = (1U << 6))
#define GPI_BTN_UP_Get()               ((PORTB >> 6) & 0x1U)
#define GPI_BTN_UP_GetLatch()          ((LATB >> 6) & 0x1U)
#define GPI_BTN_UP_PIN                  GPIO_PIN_RB6
#define GPI_BTN_UP_InterruptEnable()   (CNENBSET = (1U << 6))
#define GPI_BTN_UP_InterruptDisable()  (CNENBCLR = (1U << 6))

/*** Macros for GPI_BTN_DOWN pin ***/
#define GPI_BTN_DOWN_Set()               (LATCSET = (1U << 10))
#define GPI_BTN_DOWN_Clear()             (LATCCLR = (1U << 10))
#define GPI_BTN_DOWN_Toggle()            (LATCINV = (1U << 10))
#define GPI_BTN_DOWN_OutputEnable()      (TRISCCLR = (1U << 10))
#define GPI_BTN_DOWN_InputEnable()       (TRISCSET = (1U << 10))
#define GPI_BTN_DOWN_Get()               ((PORTC >> 10) & 0x1U)
#define GPI_BTN_DOWN_GetLatch()          ((LATC >> 10) & 0x1U)
#define GPI_BTN_DOWN_PIN                  GPIO_PIN_RC10
#define GPI_BTN_DOWN_InterruptEnable()   (CNENCSET = (1U << 10))
#define GPI_BTN_DOWN_InterruptDisable()  (CNENCCLR = (1U << 10))

/*** Macros for GPI_BTN_MAN pin ***/
#define GPI_BTN_MAN_Set()               (LATBSET = (1U << 7))
#define GPI_BTN_MAN_Clear()             (LATBCLR = (1U << 7))
#define GPI_BTN_MAN_Toggle()            (LATBINV = (1U << 7))
#define GPI_BTN_MAN_OutputEnable()      (TRISBCLR = (1U << 7))
#define GPI_BTN_MAN_InputEnable()       (TRISBSET = (1U << 7))
#define GPI_BTN_MAN_Get()               ((PORTB >> 7) & 0x1U)
#define GPI_BTN_MAN_GetLatch()          ((LATB >> 7) & 0x1U)
#define GPI_BTN_MAN_PIN                  GPIO_PIN_RB7
#define GPI_BTN_MAN_InterruptEnable()   (CNENBSET = (1U << 7))
#define GPI_BTN_MAN_InterruptDisable()  (CNENBCLR = (1U << 7))

/*** Macros for CAN_RX_IPU_REAR pin ***/
#define CAN_RX_IPU_REAR_Get()               ((PORTC >> 6) & 0x1U)
#define CAN_RX_IPU_REAR_GetLatch()          ((LATC >> 6) & 0x1U)
#define CAN_RX_IPU_REAR_PIN                  GPIO_PIN_RC6

/*** Macros for CAN_TX_IPU_REAR pin ***/
#define CAN_TX_IPU_REAR_Get()               ((PORTC >> 7) & 0x1U)
#define CAN_TX_IPU_REAR_GetLatch()          ((LATC >> 7) & 0x1U)
#define CAN_TX_IPU_REAR_PIN                  GPIO_PIN_RC7

/*** Macros for CAN_RX_IPU_FRONT pin ***/
#define CAN_RX_IPU_FRONT_Get()               ((PORTD >> 6) & 0x1U)
#define CAN_RX_IPU_FRONT_GetLatch()          ((LATD >> 6) & 0x1U)
#define CAN_RX_IPU_FRONT_PIN                  GPIO_PIN_RD6

/*** Macros for CAN_TX_IPU_FRONT pin ***/
#define CAN_TX_IPU_FRONT_Get()               ((PORTC >> 9) & 0x1U)
#define CAN_TX_IPU_FRONT_GetLatch()          ((LATC >> 9) & 0x1U)
#define CAN_TX_IPU_FRONT_PIN                  GPIO_PIN_RC9

/*** Macros for CAN_TX_GW pin ***/
#define CAN_TX_GW_Get()               ((PORTF >> 0) & 0x1U)
#define CAN_TX_GW_GetLatch()          ((LATF >> 0) & 0x1U)
#define CAN_TX_GW_PIN                  GPIO_PIN_RF0

/*** Macros for CAN_RX_GW pin ***/
#define CAN_RX_GW_Get()               ((PORTF >> 1) & 0x1U)
#define CAN_RX_GW_GetLatch()          ((LATF >> 1) & 0x1U)
#define CAN_RX_GW_PIN                  GPIO_PIN_RF1

/*** Macros for CAN_RX_EXBRD pin ***/
#define CAN_RX_EXBRD_Get()               ((PORTG >> 1) & 0x1U)
#define CAN_RX_EXBRD_GetLatch()          ((LATG >> 1) & 0x1U)
#define CAN_RX_EXBRD_PIN                  GPIO_PIN_RG1

/*** Macros for CAN_TX_EXBRD pin ***/
#define CAN_TX_EXBRD_Get()               ((PORTG >> 0) & 0x1U)
#define CAN_TX_EXBRD_GetLatch()          ((LATG >> 0) & 0x1U)
#define CAN_TX_EXBRD_PIN                  GPIO_PIN_RG0

/*** Macros for GPO_GND_AC_CLUTCH pin ***/
#define GPO_GND_AC_CLUTCH_Set()               (LATBSET = (1U << 10))
#define GPO_GND_AC_CLUTCH_Clear()             (LATBCLR = (1U << 10))
#define GPO_GND_AC_CLUTCH_Toggle()            (LATBINV = (1U << 10))
#define GPO_GND_AC_CLUTCH_OutputEnable()      (TRISBCLR = (1U << 10))
#define GPO_GND_AC_CLUTCH_InputEnable()       (TRISBSET = (1U << 10))
#define GPO_GND_AC_CLUTCH_Get()               ((PORTB >> 10) & 0x1U)
#define GPO_GND_AC_CLUTCH_GetLatch()          ((LATB >> 10) & 0x1U)
#define GPO_GND_AC_CLUTCH_PIN                  GPIO_PIN_RB10

/*** Macros for GPO_REL_VACUUM pin ***/
#define GPO_REL_VACUUM_Set()               (LATBSET = (1U << 11))
#define GPO_REL_VACUUM_Clear()             (LATBCLR = (1U << 11))
#define GPO_REL_VACUUM_Toggle()            (LATBINV = (1U << 11))
#define GPO_REL_VACUUM_OutputEnable()      (TRISBCLR = (1U << 11))
#define GPO_REL_VACUUM_InputEnable()       (TRISBSET = (1U << 11))
#define GPO_REL_VACUUM_Get()               ((PORTB >> 11) & 0x1U)
#define GPO_REL_VACUUM_GetLatch()          ((LATB >> 11) & 0x1U)
#define GPO_REL_VACUUM_PIN                  GPIO_PIN_RB11

/*** Macros for GPO_GND_VALVE pin ***/
#define GPO_GND_VALVE_Set()               (LATGSET = (1U << 14))
#define GPO_GND_VALVE_Clear()             (LATGCLR = (1U << 14))
#define GPO_GND_VALVE_Toggle()            (LATGINV = (1U << 14))
#define GPO_GND_VALVE_OutputEnable()      (TRISGCLR = (1U << 14))
#define GPO_GND_VALVE_InputEnable()       (TRISGSET = (1U << 14))
#define GPO_GND_VALVE_Get()               ((PORTG >> 14) & 0x1U)
#define GPO_GND_VALVE_GetLatch()          ((LATG >> 14) & 0x1U)
#define GPO_GND_VALVE_PIN                  GPIO_PIN_RG14

/*** Macros for GPO_GND_PUMP pin ***/
#define GPO_GND_PUMP_Set()               (LATGSET = (1U << 12))
#define GPO_GND_PUMP_Clear()             (LATGCLR = (1U << 12))
#define GPO_GND_PUMP_Toggle()            (LATGINV = (1U << 12))
#define GPO_GND_PUMP_OutputEnable()      (TRISGCLR = (1U << 12))
#define GPO_GND_PUMP_InputEnable()       (TRISGSET = (1U << 12))
#define GPO_GND_PUMP_Get()               ((PORTG >> 12) & 0x1U)
#define GPO_GND_PUMP_GetLatch()          ((LATG >> 12) & 0x1U)
#define GPO_GND_PUMP_PIN                  GPIO_PIN_RG12

/*** Macros for GPO_REL_PUMP_IEP pin ***/
#define GPO_REL_PUMP_IEP_Set()               (LATGSET = (1U << 13))
#define GPO_REL_PUMP_IEP_Clear()             (LATGCLR = (1U << 13))
#define GPO_REL_PUMP_IEP_Toggle()            (LATGINV = (1U << 13))
#define GPO_REL_PUMP_IEP_OutputEnable()      (TRISGCLR = (1U << 13))
#define GPO_REL_PUMP_IEP_InputEnable()       (TRISGSET = (1U << 13))
#define GPO_REL_PUMP_IEP_Get()               ((PORTG >> 13) & 0x1U)
#define GPO_REL_PUMP_IEP_GetLatch()          ((LATG >> 13) & 0x1U)
#define GPO_REL_PUMP_IEP_PIN                  GPIO_PIN_RG13

/*** Macros for GPO_REL_PUMP_STEERING pin ***/
#define GPO_REL_PUMP_STEERING_Set()               (LATBSET = (1U << 12))
#define GPO_REL_PUMP_STEERING_Clear()             (LATBCLR = (1U << 12))
#define GPO_REL_PUMP_STEERING_Toggle()            (LATBINV = (1U << 12))
#define GPO_REL_PUMP_STEERING_OutputEnable()      (TRISBCLR = (1U << 12))
#define GPO_REL_PUMP_STEERING_InputEnable()       (TRISBSET = (1U << 12))
#define GPO_REL_PUMP_STEERING_Get()               ((PORTB >> 12) & 0x1U)
#define GPO_REL_PUMP_STEERING_GetLatch()          ((LATB >> 12) & 0x1U)
#define GPO_REL_PUMP_STEERING_PIN                  GPIO_PIN_RB12

/*** Macros for GPO_REL_FAN pin ***/
#define GPO_REL_FAN_Set()               (LATBSET = (1U << 13))
#define GPO_REL_FAN_Clear()             (LATBCLR = (1U << 13))
#define GPO_REL_FAN_Toggle()            (LATBINV = (1U << 13))
#define GPO_REL_FAN_OutputEnable()      (TRISBCLR = (1U << 13))
#define GPO_REL_FAN_InputEnable()       (TRISBSET = (1U << 13))
#define GPO_REL_FAN_Get()               ((PORTB >> 13) & 0x1U)
#define GPO_REL_FAN_GetLatch()          ((LATB >> 13) & 0x1U)
#define GPO_REL_FAN_PIN                  GPIO_PIN_RB13

/*** Macros for GPO_REL_PUMP_IPU pin ***/
#define GPO_REL_PUMP_IPU_Set()               (LATASET = (1U << 10))
#define GPO_REL_PUMP_IPU_Clear()             (LATACLR = (1U << 10))
#define GPO_REL_PUMP_IPU_Toggle()            (LATAINV = (1U << 10))
#define GPO_REL_PUMP_IPU_OutputEnable()      (TRISACLR = (1U << 10))
#define GPO_REL_PUMP_IPU_InputEnable()       (TRISASET = (1U << 10))
#define GPO_REL_PUMP_IPU_Get()               ((PORTA >> 10) & 0x1U)
#define GPO_REL_PUMP_IPU_GetLatch()          ((LATA >> 10) & 0x1U)
#define GPO_REL_PUMP_IPU_PIN                  GPIO_PIN_RA10


// *****************************************************************************
/* GPIO Port

  Summary:
    Identifies the available GPIO Ports.

  Description:
    This enumeration identifies the available GPIO Ports.

  Remarks:
    The caller should not rely on the specific numbers assigned to any of
    these values as they may change from one processor to the next.

    Not all ports are available on all devices.  Refer to the specific
    device data sheet to determine which ports are supported.
*/


#define    GPIO_PORT_A  (0)
#define    GPIO_PORT_B  (1)
#define    GPIO_PORT_C  (2)
#define    GPIO_PORT_D  (3)
#define    GPIO_PORT_E  (4)
#define    GPIO_PORT_F  (5)
#define    GPIO_PORT_G  (6)
typedef uint32_t GPIO_PORT;

typedef enum
{
    GPIO_INTERRUPT_ON_MISMATCH,
    GPIO_INTERRUPT_ON_RISING_EDGE,
    GPIO_INTERRUPT_ON_FALLING_EDGE,
    GPIO_INTERRUPT_ON_BOTH_EDGES,
} GPIO_INTERRUPT_STYLE;

// *****************************************************************************
/* GPIO Port Pins

  Summary:
    Identifies the available GPIO port pins.

  Description:
    This enumeration identifies the available GPIO port pins.

  Remarks:
    The caller should not rely on the specific numbers assigned to any of
    these values as they may change from one processor to the next.

    Not all pins are available on all devices.  Refer to the specific
    device data sheet to determine which pins are supported.
*/


#define     GPIO_PIN_RA0  (0U)
#define     GPIO_PIN_RA1  (1U)
#define     GPIO_PIN_RA4  (4U)
#define     GPIO_PIN_RA7  (7U)
#define     GPIO_PIN_RA8  (8U)
#define     GPIO_PIN_RA10  (10U)
#define     GPIO_PIN_RA11  (11U)
#define     GPIO_PIN_RA12  (12U)
#define     GPIO_PIN_RA14  (14U)
#define     GPIO_PIN_RA15  (15U)
#define     GPIO_PIN_RB0  (16U)
#define     GPIO_PIN_RB1  (17U)
#define     GPIO_PIN_RB2  (18U)
#define     GPIO_PIN_RB3  (19U)
#define     GPIO_PIN_RB4  (20U)
#define     GPIO_PIN_RB5  (21U)
#define     GPIO_PIN_RB6  (22U)
#define     GPIO_PIN_RB7  (23U)
#define     GPIO_PIN_RB8  (24U)
#define     GPIO_PIN_RB9  (25U)
#define     GPIO_PIN_RB10  (26U)
#define     GPIO_PIN_RB11  (27U)
#define     GPIO_PIN_RB12  (28U)
#define     GPIO_PIN_RB13  (29U)
#define     GPIO_PIN_RB14  (30U)
#define     GPIO_PIN_RB15  (31U)
#define     GPIO_PIN_RC0  (32U)
#define     GPIO_PIN_RC1  (33U)
#define     GPIO_PIN_RC2  (34U)
#define     GPIO_PIN_RC6  (38U)
#define     GPIO_PIN_RC7  (39U)
#define     GPIO_PIN_RC8  (40U)
#define     GPIO_PIN_RC9  (41U)
#define     GPIO_PIN_RC10  (42U)
#define     GPIO_PIN_RC11  (43U)
#define     GPIO_PIN_RC12  (44U)
#define     GPIO_PIN_RC13  (45U)
#define     GPIO_PIN_RC15  (47U)
#define     GPIO_PIN_RD1  (49U)
#define     GPIO_PIN_RD2  (50U)
#define     GPIO_PIN_RD3  (51U)
#define     GPIO_PIN_RD4  (52U)
#define     GPIO_PIN_RD5  (53U)
#define     GPIO_PIN_RD6  (54U)
#define     GPIO_PIN_RD8  (56U)
#define     GPIO_PIN_RD12  (60U)
#define     GPIO_PIN_RD13  (61U)
#define     GPIO_PIN_RD14  (62U)
#define     GPIO_PIN_RD15  (63U)
#define     GPIO_PIN_RE0  (64U)
#define     GPIO_PIN_RE1  (65U)
#define     GPIO_PIN_RE8  (72U)
#define     GPIO_PIN_RE9  (73U)
#define     GPIO_PIN_RE12  (76U)
#define     GPIO_PIN_RE13  (77U)
#define     GPIO_PIN_RE14  (78U)
#define     GPIO_PIN_RE15  (79U)
#define     GPIO_PIN_RF0  (80U)
#define     GPIO_PIN_RF1  (81U)
#define     GPIO_PIN_RF5  (85U)
#define     GPIO_PIN_RF6  (86U)
#define     GPIO_PIN_RF7  (87U)
#define     GPIO_PIN_RF9  (89U)
#define     GPIO_PIN_RF10  (90U)
#define     GPIO_PIN_RF12  (92U)
#define     GPIO_PIN_RF13  (93U)
#define     GPIO_PIN_RG0  (96U)
#define     GPIO_PIN_RG1  (97U)
#define     GPIO_PIN_RG6  (102U)
#define     GPIO_PIN_RG7  (103U)
#define     GPIO_PIN_RG8  (104U)
#define     GPIO_PIN_RG9  (105U)
#define     GPIO_PIN_RG10  (106U)
#define     GPIO_PIN_RG11  (107U)
#define     GPIO_PIN_RG12  (108U)
#define     GPIO_PIN_RG13  (109U)
#define     GPIO_PIN_RG14  (110U)
#define     GPIO_PIN_RG15  (111U)

/* This element should not be used in any of the GPIO APIs.
       It will be used by other modules or application to denote that none of the GPIO Pin is used */
#define    GPIO_PIN_NONE   (-1)

typedef uint32_t GPIO_PIN;

typedef  void (*GPIO_PIN_CALLBACK) (GPIO_PIN pin, uintptr_t context);

void GPIO_Initialize(void);

// *****************************************************************************
// *****************************************************************************
// Section: GPIO Functions which operates on multiple pins of a port
// *****************************************************************************
// *****************************************************************************

uint32_t GPIO_PortRead(GPIO_PORT port);

void GPIO_PortWrite(GPIO_PORT port, uint32_t mask, uint32_t value);

uint32_t GPIO_PortLatchRead(GPIO_PORT port);

void GPIO_PortSet(GPIO_PORT port, uint32_t mask);

void GPIO_PortClear(GPIO_PORT port, uint32_t mask);

void GPIO_PortToggle(GPIO_PORT port, uint32_t mask);

void GPIO_PortInputEnable(GPIO_PORT port, uint32_t mask);

void GPIO_PortOutputEnable(GPIO_PORT port, uint32_t mask);

void GPIO_PortInterruptEnable(GPIO_PORT port, uint32_t mask);

void GPIO_PortInterruptDisable(GPIO_PORT port, uint32_t mask);

// *****************************************************************************
// *****************************************************************************
// Section: Local Data types and Prototypes
// *****************************************************************************
// *****************************************************************************

typedef struct {
    /* target pin */
    GPIO_PIN pin;

    /* Callback for event on target pin*/
    GPIO_PIN_CALLBACK callback;

    /* Callback Context */
    uintptr_t context;
} GPIO_PIN_CALLBACK_OBJ;

// *****************************************************************************
// *****************************************************************************
// Section: GPIO Functions which operates on one pin at a time
// *****************************************************************************
// *****************************************************************************

static inline void GPIO_PinWrite(GPIO_PIN pin, bool value) {
    uint32_t xvalue = (uint32_t)value;
    GPIO_PortWrite((pin >> 4U), (uint32_t)(0x1U) << (pin & 0xFU), (xvalue) << (pin & 0xFU));
}

static inline bool GPIO_PinRead(GPIO_PIN pin) {
    return ((((GPIO_PortRead((GPIO_PORT)(pin >> 4U))) >> (pin & 0xFU)) & 0x1U) != 0U);
}

static inline bool GPIO_PinLatchRead(GPIO_PIN pin) {
    return (((GPIO_PortLatchRead((GPIO_PORT)(pin >> 4U)) >> (pin & 0xFU)) & 0x1U) != 0U);
}

static inline void GPIO_PinToggle(GPIO_PIN pin) {
    GPIO_PortToggle((pin >> 4U), (uint32_t)0x1U << (pin & 0xFU));
}

static inline void GPIO_PinSet(GPIO_PIN pin) {
    GPIO_PortSet((pin >> 4U), (uint32_t)0x1U << (pin & 0xFU));
}

static inline void GPIO_PinClear(GPIO_PIN pin) {
    GPIO_PortClear((pin >> 4U), (uint32_t)0x1U << (pin & 0xFU));
}

static inline void GPIO_PinInputEnable(GPIO_PIN pin) {
    GPIO_PortInputEnable((pin >> 4U), (uint32_t)0x1U << (pin & 0xFU));
}

static inline void GPIO_PinOutputEnable(GPIO_PIN pin) {
    GPIO_PortOutputEnable((pin >> 4U), (uint32_t)0x1U << (pin & 0xFU));
}

#define GPIO_PinInterruptEnable(pin)       GPIO_PinIntEnable(pin, GPIO_INTERRUPT_ON_MISMATCH)
#define GPIO_PinInterruptDisable(pin)      GPIO_PinIntDisable(pin)

void GPIO_PinIntEnable(GPIO_PIN pin, GPIO_INTERRUPT_STYLE style);

void GPIO_PinIntDisable(GPIO_PIN pin);

bool GPIO_PinInterruptCallbackRegister(
    GPIO_PIN pin,
    const GPIO_PIN_CALLBACK callback,
    uintptr_t context
    );

// DOM-IGNORE-BEGIN
#ifdef __cplusplus  // Provide C++ Compatibility
}

#endif
// DOM-IGNORE-END
#endif // PLIB_GPIO_H
