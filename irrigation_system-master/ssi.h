#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"

// variáveis globais externas capturadas do arquivo main
extern volatile int soil_param, temp_param, umid_param;
extern float humidity, temperature_c, humidity_percent;

// SSI tags - tamanho da tag limitada a 8 bytes por padrão
const char * ssi_tags[] = {
  "soil","temp","umid","rele",
  "soil_val","temp_val","umid_val"
  };

u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen) {
  size_t printed;
  switch (iIndex) {
  case 0: // soil
    {
      printed = snprintf(pcInsert, iInsertLen, "%.1f", humidity_percent);
    }
    break;
  case 1: // temp
    {
      printed = snprintf(pcInsert, iInsertLen, "%.1f", temperature_c);
    }
    break;
  case 2: // umid
    {
      printed = snprintf(pcInsert, iInsertLen, "%.1f", humidity);
    }
    break;
  case 3: // rele
    {
      bool rele_status = gpio_get(12);
      if(rele_status == true){
        printed = snprintf(pcInsert, iInsertLen, "ON");
      }
      else{
        printed = snprintf(pcInsert, iInsertLen, "OFF");
      }
    }
    break;
  case 4:
    {
      printed = snprintf(pcInsert, iInsertLen, "%d", soil_param);
    }
    break;
  case 5:
    {
      printed = snprintf(pcInsert, iInsertLen, "%d", temp_param);
    }
    break;
  case 6:
    {
      printed = snprintf(pcInsert, iInsertLen, "%d", umid_param);
    }
    break;
  default:
    printed = 0;
    break;
  }

  return (u16_t)printed;
}

//  registra o manipulador SSI no servidor HTTP do LwIP
void ssi_init() {
  http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
}
