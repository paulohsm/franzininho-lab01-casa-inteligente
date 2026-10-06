#include "sensores.h"
#include "dht.h"
#include "esp_adc/adc_oneshot.h"

int pino_do_dht;
int canal_do_ldr;
adc_oneshot_unit_handle_t adc;

// configura o ADC1 para ler o LDR
void sensores_iniciar(int pino_dht, int canal_ldr)
{
    pino_do_dht = pino_dht;
    canal_do_ldr = canal_ldr;

    adc_oneshot_unit_init_cfg_t config_adc = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&config_adc, &adc);

    adc_oneshot_chan_cfg_t config_canal = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    adc_oneshot_config_channel(adc, canal_do_ldr, &config_canal);
}

// le temperatura e umidade do DHT11 e o valor do LDR (0 a 8191)
void sensores_ler(float *temperatura, float *umidade, int *luz)
{
    float t, u;
    // se der erro fica o valor antigo
    if (dht_read_float_data(DHT_TYPE_DHT11, pino_do_dht, &u, &t) == ESP_OK) {
        *temperatura = t;
        *umidade = u;
    }

    int valor = 0;
    adc_oneshot_read(adc, canal_do_ldr, &valor);
    *luz = valor;
}
