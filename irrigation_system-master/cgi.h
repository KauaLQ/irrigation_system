#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"

// variáveis globais externas capturadas do arquivo main
extern volatile int soil_param, temp_param, umid_param;
extern volatile bool onOff_relay; // Sinalizador global para ativação do relé (atrelado ao main)

// Manipulador CGI para múltiplos parâmetros
const char *cgi_control_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    for (int i = 0; i < iNumParams; i++) {
        // Manipulador CGI para o parâmetro de umidade do solo (0 a 100)
        if (strcmp(pcParam[i], "valor") == 0) {
            int soil_value = atoi(pcValue[i]);  // Converte o valor para inteiro

            if (soil_value >= 0 && soil_value <= 100) {
                soil_param = soil_value;
            }
        }
        // Manipulador CGI para o parâmetro de temperatura (0 a 40)
        else if (strcmp(pcParam[i], "temp") == 0) {
            int temp_value = atoi(pcValue[i]);  // Converte o valor para inteiro

            if (temp_value >= 0 && temp_value <= 40) {
                temp_param = temp_value;
            }
        }
        // Manipulador CGI para o parâmetro de umidade do ar (0 a 100)
        else if (strcmp(pcParam[i], "umid") == 0) {
            int umid_value = atoi(pcValue[i]);  // Converte o valor para inteiro

            if (umid_value >= 0 && umid_value <= 100) {
                umid_param = umid_value;
            }
        }
    }
    
    return "/index.shtml"; // Redireciona para a página principal
}

// Manipulador CGI que é executado quando uma solicitação para /rele.cgi é detectada
const char * cgi_rele_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    // Verifique se uma solicitação de Rele foi feita (/rele.cgi?rele=x)
    if (strcmp(pcParam[0] , "rele") == 0){
        // Veja o argumento para verificar se o Rele deve ser ligado (x=1) ou desligado (x=0)
        if(strcmp(pcValue[0], "0") == 0){
            onOff_relay = false;
        } 
        else if(strcmp(pcValue[0], "1") == 0){
            onOff_relay = true;
        }
            
    }
    
    // Envia a página de índice de volta ao usuário
    return "/index.shtml";
}

// Manipulador CGI que é executado quando uma solicitação para /led.cgi é detectada
const char * cgi_led_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    // Verifique se uma solicitação de led foi feita (/led.cgi?led=x)
    if (strcmp(pcParam[0] , "led") == 0){
        // Veja o argumento para verificar se o led deve ser ligado (x=1) ou desligado (x=0)
        if(strcmp(pcValue[0], "0") == 0){
            gpio_put(12, 0);
        }
        else if(strcmp(pcValue[0], "1") == 0){
            gpio_put(12, 1);
        }
            
    }
    
    // Envia a página de índice de volta ao usuário
    return "/index.shtml";
}

// tCGI Struct
// CAqui são colocados todos os CGI request e seus respectivos handlers
static const tCGI cgi_handlers[] = {
    {"/rele.cgi", cgi_rele_handler},
    { "/control.cgi", cgi_control_handler },
    {"/led.cgi", cgi_led_handler},
};

// registro dos handlers
void cgi_init(void)
{
    http_set_cgi_handlers(cgi_handlers, sizeof(cgi_handlers) / sizeof(tCGI));
}