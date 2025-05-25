# Control Panel

## Descrição
- Control Panel é um painel de controle de acesso interativo desenvolvido para a placa BitDogLab (RP2040) que simula o controle de fluxo de usuários em ambientes como laboratórios, bibliotecas ou refeitórios.
- Utilizando FreeRTOS, semáforos e mutexes, o sistema gerencia entradas, saídas e reset, fornecendo feedback visual por LED RGB, sonoro por buzzer e textual via display OLED SSD1306.

## Funcionalidades Principais
- Controle de usuários simultâneos com semáforo de contagem (xCountingSem).
- Reset geral via semáforo binário (xResetSem) acionado por interrupção no joystick.
- Proteção de acesso concorrente ao display usando mutex (xDisplayMutex).
- Feedback visual com LED RGB:
  - Azul: 0 usuários
  - Verde: ocupação intermediária
  - Amarelo: última vaga
  - Vermelho: capacidade máxima
- Feedback sonoro com buzzer:
  - Beep curto em tentativa inválida
  - Beep duplo no reset
- Exibição de contagem de usuários e vagas disponíveis no display OLED SSD1306.

## Componentes de Hardware
- Placa BitDogLab (RP2040)
- LED RGB: pinos GPIO 11 (G), 12 (B) e 13 (R)
- Botões:
  - Entrada (Botão A) → GPIO 5
  - Saída (Botão B) → GPIO 6
  - Reset (Joystick) → GPIO 22 (com interrupção)
- Buzzer: GPIO 21 (PWM)
- Display OLED SSD1306: I²C1 (SDA = GPIO 14, SCL = GPIO 15), endereço 0x3C

## Requisitos de Software
- Raspberry Pi Pico SDK (C/C++)
- FreeRTOS integrado no SDK
- Biblioteca SSD1306 para display OLED
- CMake e Ninja (ou Make)
- GCC ARM Embedded (arm-none-eabi-gcc)
- Picotool ou drag-and-drop para flash de UF2

## Compilação e Flash
```
# Gerar arquivos de build
cmake -B build
# Compilar o firmware
cmake --build build
# Carregar no Pico (via USB ou picotool)
picotool load -f build/control_panel.uf2
```

## Uso
- Conecte a placa BitDogLab ao computador via USB.
- Pressione o Botão A para registrar uma entrada: o sistema decrementa o semáforo de contagem e atualiza o display e LED.
- Pressione o Botão B para registrar uma saída: o sistema incrementa o semáforo liberando vaga.
- Pressione o Joystick para resetar a contagem: zera usuários, restaura todas as vagas e emite beep duplo.

## Estrutura do Código
- controle_panel.c: inicialização de periféricos, criação de semáforos, mutex e tarefas, start do scheduler.
- Tarefas (FreeRTOS):
  - vTaskEntrada(): gerencia entradas de usuários.
  - vTaskSaida(): gerencia saídas de usuários.
  - vTaskReset(): aguarda semáforo binário para reset via ISR.
- Mecanismos de sincronização:
  - xSemaphoreCreateCounting(MAX_USERS, MAX_USERS)
  - xSemaphoreCreateBinary()
  - xSemaphoreCreateMutex()
- Rotina de feedback:
  - feedback_update(): limpa e desenha no display (protegido por mutex), altera cor do LED.
  - play_note(frequency, duration): gera beeps via PWM.
