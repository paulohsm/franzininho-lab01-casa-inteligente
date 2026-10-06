// Projeto final modulo IV - Casa Inteligente com ESP RainMaker
// Franzininho WiFi LAB01

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_schedule.h>
#include <app_network.h>
#include <app_reset.h>

#include "luz.h"
#include "sensores.h"

// pinos da LAB01
#define LED_R        14
#define LED_G        13
#define LED_B        12
#define PINO_DHT     15
#define CANAL_LDR    0
#define LED_AR       21   // LED que faz papel do ar-condicionado
#define BT_PRESENCA  2    // botao que simula o sensor de movimento
#define BT_BOOT      0    // botao BOOT (reset do WiFi)

// temperatura para ligar o ar automatico
#define TEMP_AR      28

// tempo que a luz fica acesa depois da ultima presenca (segundos)
#define TEMPO_PRESENCA 30

esp_rmaker_device_t *dispositivo_luz;
esp_rmaker_device_t *dispositivo_ambiente;
esp_rmaker_device_t *dispositivo_ar;

int ar_ligado = 0;
int ar_automatico = 1;

// ---------------- comandos que chegam do app / Alexa / agendamentos ----------------

esp_err_t comando_luz(const esp_rmaker_device_t *dispositivo, const esp_rmaker_param_write_req_t pedidos[],
                      uint8_t quantidade, void *priv, esp_rmaker_write_ctx_t *ctx)
{
    for (int i = 0; i < quantidade; i++) {
        const char *nome = esp_rmaker_param_get_name(pedidos[i].param);
        esp_rmaker_param_val_t valor = pedidos[i].val;

        if (strcmp(nome, ESP_RMAKER_DEF_POWER_NAME) == 0) luz_ligar(valor.val.b);
        if (strcmp(nome, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0) luz_brilho(valor.val.i);
        if (strcmp(nome, ESP_RMAKER_DEF_HUE_NAME) == 0) luz_cor(valor.val.i);
        if (strcmp(nome, ESP_RMAKER_DEF_SATURATION_NAME) == 0) luz_saturacao(valor.val.i);

        printf("Luz: %s\n", nome);
        esp_rmaker_param_update(pedidos[i].param, valor);
    }
    return ESP_OK;
}

void mudar_ar(int ligar)
{
    ar_ligado = ligar;
    gpio_set_level(LED_AR, ligar);
    printf("Ar-condicionado %s\n", ligar ? "LIGADO" : "DESLIGADO");
}

esp_err_t comando_ar(const esp_rmaker_device_t *dispositivo, const esp_rmaker_param_write_req_t pedidos[],
                     uint8_t quantidade, void *priv, esp_rmaker_write_ctx_t *ctx)
{
    for (int i = 0; i < quantidade; i++) {
        const char *nome = esp_rmaker_param_get_name(pedidos[i].param);
        esp_rmaker_param_val_t valor = pedidos[i].val;

        if (strcmp(nome, ESP_RMAKER_DEF_POWER_NAME) == 0) mudar_ar(valor.val.b);
        if (strcmp(nome, "Automatico") == 0) ar_automatico = valor.val.b;

        esp_rmaker_param_update(pedidos[i].param, valor);
    }
    return ESP_OK;
}

// ---------------- tarefas ----------------

// a cada 30 s le temperatura e umidade, manda para o app e faz o ar automatico
void tarefa_sensores(void *parametro)
{
    float temperatura = 0, umidade = 0;
    int luz_ambiente = 0;

    while (1) {
        sensores_ler(&temperatura, &umidade, &luz_ambiente);
        printf("Temperatura: %.1f C  Umidade: %.1f %%\n", temperatura, umidade);

        esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(dispositivo_ambiente, ESP_RMAKER_PARAM_TEMPERATURE),
            esp_rmaker_float(temperatura));
        esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_name(dispositivo_ambiente, "Umidade"),
            esp_rmaker_float(umidade));

        // ar automatico: liga acima de TEMP_AR e desliga 1 grau abaixo
        if (ar_automatico && umidade > 0) {
            int antes = ar_ligado;
            if (temperatura > TEMP_AR) mudar_ar(1);
            if (temperatura < TEMP_AR - 1) mudar_ar(0);
            if (ar_ligado != antes) {
                esp_rmaker_param_update_and_report(
                    esp_rmaker_device_get_param_by_name(dispositivo_ar, ESP_RMAKER_DEF_POWER_NAME),
                    esp_rmaker_bool(ar_ligado));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

// botao de presenca: acende a luz e apaga depois de TEMPO_PRESENCA segundos sem presenca
void tarefa_presenca(void *parametro)
{
    int segundos_sem_presenca = 0;
    int acesa_pela_presenca = 0;

    while (1) {
        if (gpio_get_level(BT_PRESENCA) == 0) {
            // botao apertado = tem alguem
            segundos_sem_presenca = 0;
            if (luz_esta_ligada() == 0) {
                printf("Presenca detectada, acendendo a luz\n");
                luz_ligar(1);
                acesa_pela_presenca = 1;
                esp_rmaker_param_update_and_report(
                    esp_rmaker_device_get_param_by_name(dispositivo_luz, ESP_RMAKER_DEF_POWER_NAME),
                    esp_rmaker_bool(true));
            }
        } else if (acesa_pela_presenca) {
            segundos_sem_presenca++;
            if (segundos_sem_presenca >= TEMPO_PRESENCA) {
                printf("Sem presenca, apagando a luz\n");
                luz_ligar(0);
                acesa_pela_presenca = 0;
                esp_rmaker_param_update_and_report(
                    esp_rmaker_device_get_param_by_name(dispositivo_luz, ESP_RMAKER_DEF_POWER_NAME),
                    esp_rmaker_bool(false));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------------- dispositivos no RainMaker ----------------

void criar_dispositivos(esp_rmaker_node_t *no)
{
    // Luz: liga/desliga, brilho e cor
    dispositivo_luz = esp_rmaker_lightbulb_device_create("Luz", NULL, false);
    esp_rmaker_device_add_bulk_cb(dispositivo_luz, comando_luz, NULL);
    esp_rmaker_device_add_param(dispositivo_luz, esp_rmaker_brightness_param_create(ESP_RMAKER_DEF_BRIGHTNESS_NAME, 50));
    esp_rmaker_device_add_param(dispositivo_luz, esp_rmaker_hue_param_create(ESP_RMAKER_DEF_HUE_NAME, 0));
    esp_rmaker_device_add_param(dispositivo_luz, esp_rmaker_saturation_param_create(ESP_RMAKER_DEF_SATURATION_NAME, 0));
    esp_rmaker_node_add_device(no, dispositivo_luz);

    // Ambiente: temperatura e umidade
    dispositivo_ambiente = esp_rmaker_temp_sensor_device_create("Ambiente", NULL, 0);
    esp_rmaker_param_t *umidade = esp_rmaker_param_create("Umidade", NULL, esp_rmaker_float(0), PROP_FLAG_READ);
    esp_rmaker_param_add_ui_type(umidade, ESP_RMAKER_UI_TEXT);
    esp_rmaker_device_add_param(dispositivo_ambiente, umidade);
    esp_rmaker_node_add_device(no, dispositivo_ambiente);

    // Ar-condicionado: liga/desliga e modo automatico
    dispositivo_ar = esp_rmaker_switch_device_create("Ar-condicionado", NULL, false);
    esp_rmaker_device_add_bulk_cb(dispositivo_ar, comando_ar, NULL);
    esp_rmaker_param_t *automatico = esp_rmaker_param_create("Automatico", NULL, esp_rmaker_bool(true), PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_param_add_ui_type(automatico, ESP_RMAKER_UI_TOGGLE);
    esp_rmaker_device_add_param(dispositivo_ar, automatico);
    esp_rmaker_node_add_device(no, dispositivo_ar);
}

// ---------------- programa principal ----------------

void app_main(void)
{
    // hardware
    luz_iniciar(LED_R, LED_G, LED_B);
    sensores_iniciar(PINO_DHT, CANAL_LDR);

    gpio_reset_pin(LED_AR);
    gpio_set_direction(LED_AR, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_AR, 0);

    gpio_reset_pin(BT_PRESENCA);
    gpio_set_direction(BT_PRESENCA, GPIO_MODE_INPUT);
    gpio_pullup_en(BT_PRESENCA);

    // segurar BOOT 3 s apaga o WiFi, 10 s volta ao padrao de fabrica
    app_reset_button_register(app_reset_button_create(BT_BOOT, 0), 3, 10);

    // NVS (guarda o WiFi e os dados do RainMaker)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    app_network_init();

    // cria o no do RainMaker com os 3 dispositivos
    esp_rmaker_config_t config = { .enable_time_sync = false };
    esp_rmaker_node_t *no = esp_rmaker_node_init(&config, "Casa Inteligente", "Smart Home");
    criar_dispositivos(no);

    // fuso horario e agendamentos (timers das luzes)
    esp_rmaker_timezone_service_enable();
    esp_rmaker_schedule_enable();

    esp_rmaker_start();

    // se ainda nao tem WiFi salvo, mostra um QR code no terminal para configurar pelo app
    app_network_start(POP_TYPE_RANDOM);

    xTaskCreate(tarefa_sensores, "sensores", 4096, NULL, 5, NULL);
    xTaskCreate(tarefa_presenca, "presenca", 4096, NULL, 4, NULL);
}
