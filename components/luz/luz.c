#include "luz.h"
#include "driver/ledc.h"

int ligada = 0;
int brilho = 50;     // 0 a 100
int matiz = 0;       // 0 a 360 (cor)
int saturacao = 0;   // 0 a 100 (0 = branco)

// o app manda a cor em HSV (matiz, saturacao, brilho)
// aqui converte para RGB e manda para o PWM
void atualizar_led()
{
    float r = 0, g = 0, b = 0;

    if (ligada) {
        float h = (matiz % 360) / 60.0;
        float s = saturacao / 100.0;
        float v = brilho / 100.0;
        int i = (int)h;
        float f = h - i;
        float p = v * (1 - s);
        float q = v * (1 - s * f);
        float t = v * (1 - s * (1 - f));

        if (i == 0) { r = v; g = t; b = p; }
        if (i == 1) { r = q; g = v; b = p; }
        if (i == 2) { r = p; g = v; b = t; }
        if (i == 3) { r = p; g = q; b = v; }
        if (i == 4) { r = t; g = p; b = v; }
        if (i == 5) { r = v; g = p; b = q; }
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, 0, r * 255);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, 1, g * 255);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, 1);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, 2, b * 255);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, 2);
}

void luz_iniciar(int pino_r, int pino_g, int pino_b)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    int pinos[3] = {pino_r, pino_g, pino_b};
    for (int i = 0; i < 3; i++) {
        ledc_channel_config_t canal = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = i,
            .timer_sel = LEDC_TIMER_0,
            .gpio_num = pinos[i],
            .duty = 0
        };
        ledc_channel_config(&canal);
    }
}

void luz_ligar(int valor)       { ligada = valor; atualizar_led(); }
void luz_brilho(int valor)      { brilho = valor; atualizar_led(); }
void luz_cor(int valor)         { matiz = valor; atualizar_led(); }
void luz_saturacao(int valor)   { saturacao = valor; atualizar_led(); }
int luz_esta_ligada(void)       { return ligada; }
