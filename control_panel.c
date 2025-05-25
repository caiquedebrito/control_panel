#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "semphr.h"
#include "pico/bootrom.h"

#define LED_G 11
#define LED_B 12
#define LED_R 13

#define BUTTON_A 5
#define BUTTON_B 6
#define JOYSTICK_BUTTON 22

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define address 0x3C

#define BUZZER_PIN 21

#define BUZZER_FREQUENCY 200

const uint8_t MAX_USERS = 8;

SemaphoreHandle_t xCountingSem;
SemaphoreHandle_t xResetSem;
SemaphoreHandle_t xDisplayMutex;

static uint8_t ulCurrentUsers = 0;

ssd1306_t ssd;

void vTaskEntrada(void *pvParameters);
void vTaskSaida(void *pvParameters);
void vTaskReset(void *pvParameters);
void feedback_update(void);
void buzzer_beep_short(void);
void buzzer_beep_double(void);
void pwm_init_buzzer(uint pin);
void play_note(int frequency, int duration);
void gpio_callback(uint gpio, uint32_t events);
void gpio_irq_hanlder(uint gpio, uint32_t events);

int main()
{
    stdio_init_all();

    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_init(LED_B);
    gpio_set_dir(LED_B, GPIO_OUT);
    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);

    pwm_init_buzzer(BUZZER_PIN);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, address, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    
    // Semáforo de contagem para controlar o número de usuários
    xCountingSem = xSemaphoreCreateCounting(MAX_USERS, MAX_USERS);
    // Semáforo binário para resetar o contador
    xResetSem = xSemaphoreCreateBinary();
    // Mutex para proteger o acesso ao display
    xDisplayMutex = xSemaphoreCreateMutex();

    xTaskCreate(vTaskEntrada, "Entrada", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vTaskSaida, "Saida", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vTaskReset, "Reset", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}

// Task para gerenciar a entrada de usuários, incrementando o contador com o botão A
void vTaskEntrada(void *pvParameters)
{
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    feedback_update();
    
    while (true) {
        if (gpio_get(BUTTON_A) == 0) {
            if (xSemaphoreTake(xCountingSem, 0) == pdTRUE) {
                ulCurrentUsers++;
                printf("Current Users: %d\n", ulCurrentUsers);
            } else {
                buzzer_beep_short();
                printf("Max users reached!\n");
            }

            feedback_update();
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task para gerenciar a saída de usuários, decrementando o contador com o botão B
void vTaskSaida(void *pvParameters)
{
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    while (true) {
        if (gpio_get(BUTTON_B) == 0) {
            if (ulCurrentUsers > 0) {
                ulCurrentUsers--;
                xSemaphoreGive(xCountingSem);
            } else {
                buzzer_beep_short();
            }

            feedback_update();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task para gerenciar o reset do contador de usuários com o botão do joystick
void vTaskReset(void *pvParameters)
{
    gpio_init(JOYSTICK_BUTTON);
    gpio_set_dir(JOYSTICK_BUTTON, GPIO_IN);
    gpio_pull_up(JOYSTICK_BUTTON);
    gpio_set_irq_enabled_with_callback(JOYSTICK_BUTTON, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_hanlder);

    while (true) {
        if (xSemaphoreTake(xResetSem, portMAX_DELAY) == pdTRUE) {
            for (int i = ulCurrentUsers; i > 0; i--) {
                xSemaphoreGive(xCountingSem);
            }

            ulCurrentUsers = 0;
            buzzer_beep_double();
            feedback_update();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Função para atualizar o display com o número de usuários e vagas disponíveis
void feedback_update(void) {
    xSemaphoreTake(xDisplayMutex, portMAX_DELAY);

    uint32_t remaining_vacancies = uxSemaphoreGetCount(xCountingSem);
    
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    ssd1306_rect(&ssd, 0, 0, WIDTH, HEIGHT, true, false);
    char user[15];
    sprintf(user, "Usuarios: %d", ulCurrentUsers);
    ssd1306_draw_string(&ssd, user, 13, 2);
    ssd1306_hline(&ssd, 0, WIDTH, 13, true);

    char vacancies[15];
    sprintf(vacancies, "Vagas: %d", remaining_vacancies);
    ssd1306_draw_string(&ssd, vacancies, 13, 20);
    ssd1306_hline(&ssd, 0, WIDTH, 30, true);

    if (remaining_vacancies == 1) {
        ssd1306_draw_string(&ssd, "Ultima Vaga", 13, 40);
    } else if (remaining_vacancies == 0) {
        ssd1306_draw_string(&ssd, "Sem Vagas", 13, 40);
    } else {
        ssd1306_draw_string(&ssd, "Vagas", 35, 40);
        ssd1306_draw_string(&ssd, "Disponiveis", 15, 50);
    }

    ssd1306_send_data(&ssd);

    xSemaphoreGive(xDisplayMutex);

    if (ulCurrentUsers == MAX_USERS) {
        gpio_put(LED_R, true);
        gpio_put(LED_G, false);
        gpio_put(LED_B, false);
    } else if (ulCurrentUsers == MAX_USERS - 1) {
        gpio_put(LED_R, true);
        gpio_put(LED_G, true);
        gpio_put(LED_B, false);
    } else if (ulCurrentUsers > 0) {
        gpio_put(LED_G, true);
        gpio_put(LED_R, false);
        gpio_put(LED_B, false);
    } else {
        gpio_put(LED_B, true);
        gpio_put(LED_R, false);
        gpio_put(LED_G, false);
    }
}

void buzzer_beep_short(void) {
    play_note(1000, 100);
}

void buzzer_beep_double(void) {
    play_note(2100, 100);
    vTaskDelay(pdMS_TO_TICKS(100));
    play_note(2100, 100);
}

void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(pin);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096));
    pwm_init(slice_num, &config, true);

    pwm_set_gpio_level(pin, 0);
}

void play_note(int frequency, int duration) {
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    uint32_t wrap = 4095;
    float divider = (float) clock_get_hz(clk_sys) / (frequency * (wrap + 1));
    pwm_set_clkdiv(slice_num, divider);
    uint16_t level = (uint16_t)(((wrap + 1) * 50) / 100); // 50% duty cycle
    pwm_set_gpio_level(BUZZER_PIN, level);
    pwm_set_enabled(slice_num, true);

    sleep_ms(duration);

    pwm_set_enabled(slice_num, false);
}

void gpio_callback(uint gpio, uint32_t events)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xResetSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void gpio_irq_hanlder(uint gpio, uint32_t events)
{
    // if (gpio == BUTTON_B) {
    //     reset_usb_boot(0, 0);
    // } else 
    if (gpio == JOYSTICK_BUTTON) {
        gpio_callback(gpio, events);
    }
}