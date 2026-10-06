// biblioteca para ler o DHT11 e o LDR

void sensores_iniciar(int pino_dht, int canal_ldr);
void sensores_ler(float *temperatura, float *umidade, int *luz);
