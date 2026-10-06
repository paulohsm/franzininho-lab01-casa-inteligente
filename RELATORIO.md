# Relatorio Tecnico - Casa Inteligente

## O que foi usado do curso

- **ESP RainMaker:** cria o no "Casa Inteligente" com tres dispositivos: Luz (lampada), Ambiente (sensor de temperatura com umidade) e Ar-condicionado (interruptor com modo automatico).
- **Provisionamento:** o ESP32-S2 nao tem Bluetooth, entao o WiFi e configurado pelo app via SoftAP, lendo o QR code mostrado no terminal.
- **Agendamentos:** servico de schedule do RainMaker, com o servico de fuso horario para os horarios ficarem certos.
- **Alexa:** a skill do ESP RainMaker reconhece a Luz como lampada (liga, brilho e cor).
- **PWM (LEDC):** tres canais para o LED RGB.
- **DHT11:** leitura de temperatura e umidade.
- **FreeRTOS:** duas tarefas, uma para os sensores e outra para a presenca.

## Como o programa funciona

O app_main inicia o hardware, cria os dispositivos no RainMaker e liga o WiFi. Quando chega um comando do app, da Alexa ou de um agendamento, o RainMaker chama `comando_luz` ou `comando_ar`, que mudam o LED.

A tarefa dos sensores le o DHT11 a cada 30 s, manda os valores para o app e faz o ar automatico. A tarefa de presenca olha o botao a cada 1 s.

## Escolhas

- **Cor em HSV:** o app e a Alexa mandam a cor como matiz, saturacao e brilho. O componente `luz` converte para RGB.
- **Ar automatico com folga de 1 grau:** liga acima de 28 e desliga abaixo de 27, para nao ficar ligando e desligando.
- **Botao no lugar do sensor de movimento:** a LAB01 nao tem sensor PIR, entao um botao simula a presenca. Um PIR poderia ser ligado no mesmo pino sem mudar a logica.
- **Leitura a cada 30 s:** a temperatura muda devagar e assim nao envia dados demais para a nuvem.

## Limitacoes

- A temperatura do ar automatico (28 graus) esta fixa no codigo.
- Se o ar for ligado pelo app com o automatico ligado, a automacao pode desligar na proxima leitura.
