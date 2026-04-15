.syntax unified
.cpu cortex-m3
.thumb

.extern SystemInit
.extern main
.extern __libc_init_array

.global g_pfnVectors
.global Reset_Handler
.global Default_Handler

.section .isr_vector, "a", %progbits
.align 2
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler

    .word WWDG_IRQHandler
    .word PVD_IRQHandler
    .word TAMPER_IRQHandler
    .word RTC_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Channel1_IRQHandler
    .word DMA1_Channel2_IRQHandler
    .word DMA1_Channel3_IRQHandler
    .word DMA1_Channel4_IRQHandler
    .word DMA1_Channel5_IRQHandler
    .word DMA1_Channel6_IRQHandler
    .word DMA1_Channel7_IRQHandler
    .word ADC1_2_IRQHandler
    .word USB_HP_CAN1_TX_IRQHandler
    .word USB_LP_CAN1_RX0_IRQHandler
    .word CAN1_RX1_IRQHandler
    .word CAN1_SCE_IRQHandler
    .word EXTI9_5_IRQHandler
    .word TIM1_BRK_IRQHandler
    .word TIM1_UP_IRQHandler
    .word TIM1_TRG_COM_IRQHandler
    .word TIM1_CC_IRQHandler
    .word TIM2_IRQHandler
    .word TIM3_IRQHandler
    .word TIM4_IRQHandler
    .word I2C1_EV_IRQHandler
    .word I2C1_ER_IRQHandler
    .word I2C2_EV_IRQHandler
    .word I2C2_ER_IRQHandler
    .word SPI1_IRQHandler
    .word SPI2_IRQHandler
    .word USART1_IRQHandler
    .word USART2_IRQHandler
    .word USART3_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTCAlarm_IRQHandler
    .word USBWakeUp_IRQHandler
    .word TIM8_BRK_IRQHandler
    .word TIM8_UP_IRQHandler
    .word TIM8_TRG_COM_IRQHandler
    .word TIM8_CC_IRQHandler
    .word ADC3_IRQHandler
    .word FSMC_IRQHandler
    .word SDIO_IRQHandler
    .word TIM5_IRQHandler
    .word SPI3_IRQHandler
    .word UART4_IRQHandler
    .word UART5_IRQHandler
    .word TIM6_IRQHandler
    .word TIM7_IRQHandler
    .word DMA2_Channel1_IRQHandler
    .word DMA2_Channel2_IRQHandler
    .word DMA2_Channel3_IRQHandler
    .word DMA2_Channel4_5_IRQHandler

.section .text.Reset_Handler, "ax", %progbits
.align 2
.type Reset_Handler, %function
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    bcc 2f
    b 3f
2:
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b

3:
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
4:
    cmp r0, r1
    bcc 5f
    b 6f
5:
    str r2, [r0], #4
    b 4b

6:
    bl SystemInit
    bl __libc_init_array
    bl main
7:
    b 7b

.type Default_Handler, %function
Default_Handler:
8:
    b 8b

.macro IRQ_HANDLER name
    .weak \name
    .thumb_set \name, Default_Handler
.endm

IRQ_HANDLER NMI_Handler
IRQ_HANDLER HardFault_Handler
IRQ_HANDLER MemManage_Handler
IRQ_HANDLER BusFault_Handler
IRQ_HANDLER UsageFault_Handler
IRQ_HANDLER SVC_Handler
IRQ_HANDLER DebugMon_Handler
IRQ_HANDLER PendSV_Handler
IRQ_HANDLER SysTick_Handler
IRQ_HANDLER WWDG_IRQHandler
IRQ_HANDLER PVD_IRQHandler
IRQ_HANDLER TAMPER_IRQHandler
IRQ_HANDLER RTC_IRQHandler
IRQ_HANDLER FLASH_IRQHandler
IRQ_HANDLER RCC_IRQHandler
IRQ_HANDLER EXTI0_IRQHandler
IRQ_HANDLER EXTI1_IRQHandler
IRQ_HANDLER EXTI2_IRQHandler
IRQ_HANDLER EXTI3_IRQHandler
IRQ_HANDLER EXTI4_IRQHandler
IRQ_HANDLER DMA1_Channel1_IRQHandler
IRQ_HANDLER DMA1_Channel2_IRQHandler
IRQ_HANDLER DMA1_Channel3_IRQHandler
IRQ_HANDLER DMA1_Channel4_IRQHandler
IRQ_HANDLER DMA1_Channel5_IRQHandler
IRQ_HANDLER DMA1_Channel6_IRQHandler
IRQ_HANDLER DMA1_Channel7_IRQHandler
IRQ_HANDLER ADC1_2_IRQHandler
IRQ_HANDLER USB_HP_CAN1_TX_IRQHandler
IRQ_HANDLER USB_LP_CAN1_RX0_IRQHandler
IRQ_HANDLER CAN1_RX1_IRQHandler
IRQ_HANDLER CAN1_SCE_IRQHandler
IRQ_HANDLER EXTI9_5_IRQHandler
IRQ_HANDLER TIM1_BRK_IRQHandler
IRQ_HANDLER TIM1_UP_IRQHandler
IRQ_HANDLER TIM1_TRG_COM_IRQHandler
IRQ_HANDLER TIM1_CC_IRQHandler
IRQ_HANDLER TIM2_IRQHandler
IRQ_HANDLER TIM3_IRQHandler
IRQ_HANDLER TIM4_IRQHandler
IRQ_HANDLER I2C1_EV_IRQHandler
IRQ_HANDLER I2C1_ER_IRQHandler
IRQ_HANDLER I2C2_EV_IRQHandler
IRQ_HANDLER I2C2_ER_IRQHandler
IRQ_HANDLER SPI1_IRQHandler
IRQ_HANDLER SPI2_IRQHandler
IRQ_HANDLER USART1_IRQHandler
IRQ_HANDLER USART2_IRQHandler
IRQ_HANDLER USART3_IRQHandler
IRQ_HANDLER EXTI15_10_IRQHandler
IRQ_HANDLER RTCAlarm_IRQHandler
IRQ_HANDLER USBWakeUp_IRQHandler
IRQ_HANDLER TIM8_BRK_IRQHandler
IRQ_HANDLER TIM8_UP_IRQHandler
IRQ_HANDLER TIM8_TRG_COM_IRQHandler
IRQ_HANDLER TIM8_CC_IRQHandler
IRQ_HANDLER ADC3_IRQHandler
IRQ_HANDLER FSMC_IRQHandler
IRQ_HANDLER SDIO_IRQHandler
IRQ_HANDLER TIM5_IRQHandler
IRQ_HANDLER SPI3_IRQHandler
IRQ_HANDLER UART4_IRQHandler
IRQ_HANDLER UART5_IRQHandler
IRQ_HANDLER TIM6_IRQHandler
IRQ_HANDLER TIM7_IRQHandler
IRQ_HANDLER DMA2_Channel1_IRQHandler
IRQ_HANDLER DMA2_Channel2_IRQHandler
IRQ_HANDLER DMA2_Channel3_IRQHandler
IRQ_HANDLER DMA2_Channel4_5_IRQHandler
