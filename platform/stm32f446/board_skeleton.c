#include "platform.h"

#if defined(SENSOR_NODE_STM32F446)
#include "stm32f446xx.h"
#include "FreeRTOS.h"
#include "task.h"

/*
 * Reference register-level primitives. Sensor coefficient parsing is board
 * specific and remains behind the platform_* read functions.
 */
static TaskHandle_t i2c_waiter;
static TaskHandle_t uart_waiter;

static void gpio_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;
    /* PA2: USART2 AF7; PB8/9: I2C1 AF4 open drain. */
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 4)) | (2u << 4);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFu << 8)) | (7u << 8);
    GPIOB->MODER = (GPIOB->MODER & ~((3u << 16) | (3u << 18))) |
                   (2u << 16) | (2u << 18);
    GPIOB->OTYPER |= (1u << 8) | (1u << 9);
    GPIOB->PUPDR = (GPIOB->PUPDR & ~((3u << 16) | (3u << 18))) |
                   (1u << 16) | (1u << 18);
    GPIOB->AFR[1] = (GPIOB->AFR[1] & ~0xFFu) | 0x44u;
}

static void uart2_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    USART2->BRR = 0x31u; /* 45 MHz PCLK1 / 921600, nearest integer divider. */
    USART2->CR3 = USART_CR3_DMAT;
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

void platform_init(void) {
    SCB->CPACR |= (0xFu << 20);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gpio_init();
    uart2_init();
    /* I2C1, DMA and NVIC setup follow the table in README.md. */
}

uint32_t platform_time_us(void) {
    return DWT->CYCCNT / (SystemCoreClock / 1000000u);
}

uint32_t platform_reset_cause(void) { return RCC->CSR; }

bool platform_uart_write_dma(const uint8_t *data, size_t length,
                             uint32_t timeout_ms) {
    configASSERT(length > 0u && length <= 0xFFFFu);
    uart_waiter = xTaskGetCurrentTaskHandle();
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    while ((DMA1_Stream6->CR & DMA_SxCR_EN) != 0u) { }
    DMA1->HIFCR = DMA_HIFCR_CFEIF6 | DMA_HIFCR_CDMEIF6 |
                  DMA_HIFCR_CTEIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTCIF6;
    DMA1_Stream6->PAR = (uint32_t)&USART2->DR;
    DMA1_Stream6->M0AR = (uint32_t)data;
    DMA1_Stream6->NDTR = (uint32_t)length;
    DMA1_Stream6->CR = (4u << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC |
                       DMA_SxCR_DIR_0 | DMA_SxCR_TCIE | DMA_SxCR_TEIE;
    DMA1_Stream6->CR |= DMA_SxCR_EN;
    return ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) != 0u;
}

void DMA1_Stream6_IRQHandler(void) {
    BaseType_t wake = pdFALSE;
    const uint32_t flags = DMA1->HISR;
    DMA1->HIFCR = DMA_HIFCR_CFEIF6 | DMA_HIFCR_CDMEIF6 |
                  DMA_HIFCR_CTEIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTCIF6;
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    if ((flags & DMA_HISR_TCIF6) != 0u && uart_waiter != NULL) {
        vTaskNotifyGiveFromISR(uart_waiter, &wake);
    }
    portYIELD_FROM_ISR(wake);
}

void platform_watchdog_start(uint32_t timeout_ms) {
    (void)timeout_ms; /* Configure prescaler/reload from measured LSI frequency. */
    IWDG->KR = 0x5555u;
    IWDG->PR = 3u;
    IWDG->RLR = 1000u; /* Approximately 1 s for nominal 32 kHz LSI / 32. */
    IWDG->KR = 0xAAAAu;
    IWDG->KR = 0xCCCCu;
}

void platform_watchdog_feed(void) { IWDG->KR = 0xAAAAu; }

void platform_enter_power_mode(platform_power_mode_t mode) {
    if (mode == PLATFORM_POWER_IDLE) {
        __DSB();
        __WFI();
    } else if (mode == PLATFORM_POWER_STOP) {
        SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
        PWR->CR &= ~PWR_CR_PDDS;
        __DSB();
        __WFI();
        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
        /* Restore PLL/system clocks here before returning to a task. */
    }
}

/* These are deliberately hard failures until real sensor drivers are bound. */
bool platform_imu_read_dma(imu_sample_t *sample, uint32_t timeout_ms) {
    (void)sample; (void)timeout_ms; (void)i2c_waiter; return false;
}
bool platform_baro_read_dma(baro_sample_t *sample, uint32_t timeout_ms) {
    (void)sample; (void)timeout_ms; (void)i2c_waiter; return false;
}
bool platform_temp_read_dma(temp_sample_t *sample, uint32_t timeout_ms) {
    (void)sample; (void)timeout_ms; return false;
}
void platform_sensor_bus_recover(void) { }

#endif
