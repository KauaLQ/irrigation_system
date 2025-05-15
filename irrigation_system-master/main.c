#include "lwip/apps/httpd.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "pico/cyw43_arch.h"
#include "lwipopts.h"
#include "ssi.h"
#include "cgi.h"

#include <dht.h>
#include "functions/ssd1306_i2c.h"

//variáveis globais
volatile int soil_param = 80; // parâmetros que acionam a irrigação do solo (default)
volatile int temp_param = 35;
volatile int umid_param = 50;
float humidity, temperature_c, humidity_percent = 0; //variáveis para leitura dos sensores
volatile bool update_sensors = false; // Sinalizador global para atualização do super-loop
volatile bool onOff_relay = false; // Sinalizador global para ativação do relé

// configuração do tipo de sensor dht e o pino referente a ele
static const dht_model_t DHT_MODEL = DHT11;
static const uint DATA_PIN = 16;

// Credenciais do WIFI
const char WIFI_SSID[] = "SUA REDE";
const char WIFI_PASSWORD[] = "SUA SENHA";

// Definições dos pinos
#define rele_pin 17 // Pino onde o relé está conectado
#define LED_PIN_2 12 // Pino onde o LED está conectado

// função de callback para verificar os sensores
bool repeating_timer_callback(struct repeating_timer *t) {
    update_sensors = true; // Sinaliza que os sensores devem ser atualizados
    return true;
}

int main() {
    stdio_init_all();

    // inicialização dos pinos
    gpio_init(rele_pin);
    gpio_set_dir(rele_pin, GPIO_OUT);
    gpio_put(rele_pin, 1);

    gpio_init(LED_PIN_2);
    gpio_set_dir(LED_PIN_2, GPIO_OUT);
    gpio_put(LED_PIN_2, 0); // Desliga o LED inicialmente
    
    adc_init(); //iniciando o adc
    adc_gpio_init(28);  // Configura GPIO28 como entrada analógica para o sensor de umidade do SOLO
    adc_select_input(2); // Seleciona o canal ADC2

    dht_t dht; //instanciando um objeto do tipo dht
    dht_init(&dht, DHT_MODEL, pio0, DATA_PIN, true /* pull_up */); //inicializando o sensor

    // inicialização do wi-fi
    cyw43_arch_init();

    cyw43_arch_enable_sta_mode();

    // Loop para estabelecer a conexão
    while(cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000) != 0){
        printf("Estabelecendo Conexão...\n");
    }
    printf("Conectado! \n");
    
    // Inicializa o web server
    httpd_init();
    printf("Http server inicializado\n");

    // Configura o SSI e CGI handler
    ssi_init(); 
    printf("SSI Handler inicializado\n");
    cgi_init();
    printf("CGI Handler inicializado\n");

    /*Configura o temporizador repetitivo como responsável por atualizar a flag do super-loop.
    melhorando a eficiencia do projeto. Uma boa prática em projetos de sistemas embarcados!*/
    struct repeating_timer timer;
    add_repeating_timer_ms(1000, repeating_timer_callback, NULL, &timer);

    //configuração do display ssd1306
    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    SSD1306_init();

    // Área de renderização do display
    struct render_area frame_area = {
        start_col: 0,
        end_col : SSD1306_WIDTH - 1,
        start_page : 0,
        end_page : SSD1306_NUM_PAGES - 1
        };

    calc_render_area_buflen(&frame_area);

    // Buffer para o display
    uint8_t buf[SSD1306_BUF_LEN];
    memset(buf, 0, SSD1306_BUF_LEN);
    
    // Super-loop
    while(1){
        if(update_sensors){
            update_sensors = false; // Reseta o sinalizador

            // Leitura do sensor de solo
            uint16_t adc_value = adc_read();
            humidity_percent = adc_value * 100 / 4095.0; // Mapeia o valor para uma porcentagem

            // Leitura do DHT
            dht_start_measurement(&dht);
            dht_result_t result = dht_finish_measurement_blocking(&dht, &humidity, &temperature_c);

            memset(buf, 0, SSD1306_BUF_LEN); // Limpa o buffer do display

            if (onOff_relay) {
                gpio_put(rele_pin, 0); // Liga o relé
                char *rele_message[] = {"Irrigador", "Manualmente", "acionado"};
                WriteString(buf, 5, 16, rele_message[0]);
                WriteString(buf, 5, 32, rele_message[1]);
                WriteString(buf, 5, 48, rele_message[2]);
            }
            else if (humidity_percent > soil_param) {
                gpio_put(rele_pin, 0); // Liga o relé
                char *rele_message[] = {"Solo seco", "Irrigador", "acionado"};
                WriteString(buf, 5, 16, rele_message[0]);
                WriteString(buf, 5, 32, rele_message[1]);
                WriteString(buf, 5, 48, rele_message[2]);
            }
            else if (humidity < umid_param) {
                gpio_put(rele_pin, 0); // Liga o relé
                char *rele_message[] = {"Ar pouco umido", "Irrigador", "acionado"};
                WriteString(buf, 5, 16, rele_message[0]);
                WriteString(buf, 5, 32, rele_message[1]);
                WriteString(buf, 5, 48, rele_message[2]);
            }
            else if (temperature_c > temp_param) {
                gpio_put(rele_pin, 0); // Liga o relé
                char *rele_message[] = {"Temperatura","Muito alta", "Irrigador", "acionado"};
                WriteString(buf, 5, 8, rele_message[0]);
                WriteString(buf, 5, 24, rele_message[1]);
                WriteString(buf, 5, 40, rele_message[2]);
                WriteString(buf, 5, 56, rele_message[3]);
            }
            else{
                gpio_put(rele_pin, 1); // Desliga o relé
                if (result == DHT_RESULT_OK) {
                    // Converte o valor do sensor para string
                    char humidity_str[16];
                    sprintf(humidity_str, "Umid ar= %.1f%%", humidity);
                    char temp_str[16];
                    sprintf(temp_str, "Temp= %.1f C", temperature_c);
                    char soil_str[16];
                    sprintf(soil_str, "Solo= %.1f%%", humidity_percent);

                    // escrita no display oled e console dos valores
                    WriteString(buf, 5, 16, humidity_str);
                    WriteString(buf, 5, 32, temp_str);
                    WriteString(buf, 5, 48, soil_str);
                    printf("Temp: %.1f°C, Umid ar: %.1f%%, Solo: %.1f%%\n", temperature_c, humidity, humidity_percent);
                } else {
                    char *text = (result == DHT_RESULT_TIMEOUT) ? "DHT sem retorno" : "Erro checksum";
                    WriteString(buf, 5, 24, text);
                    puts(text);
                }
            }
            render(buf, &frame_area);
        }
        tight_loop_contents(); // Mantém o processador ocupado enquanto não há atualizações
    };
}
