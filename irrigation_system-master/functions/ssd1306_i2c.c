/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "ssd1306_font.h"

#define SSD1306_HEIGHT              64
#define SSD1306_WIDTH               128

#define SSD1306_I2C_ADDR            _u(0x3C)

#define SSD1306_I2C_CLK             400
//#define SSD1306_I2C_CLK             1000


// comandos (veja o datasheet)
#define SSD1306_SET_MEM_MODE        _u(0x20)
#define SSD1306_SET_COL_ADDR        _u(0x21)
#define SSD1306_SET_PAGE_ADDR       _u(0x22)
#define SSD1306_SET_HORIZ_SCROLL    _u(0x26)
#define SSD1306_SET_SCROLL          _u(0x2E)

#define SSD1306_SET_DISP_START_LINE _u(0x40)

#define SSD1306_SET_CONTRAST        _u(0x81)
#define SSD1306_SET_CHARGE_PUMP     _u(0x8D)

#define SSD1306_SET_SEG_REMAP       _u(0xA0)
#define SSD1306_SET_ENTIRE_ON       _u(0xA4)
#define SSD1306_SET_ALL_ON          _u(0xA5)
#define SSD1306_SET_NORM_DISP       _u(0xA6)
#define SSD1306_SET_INV_DISP        _u(0xA7)
#define SSD1306_SET_MUX_RATIO       _u(0xA8)
#define SSD1306_SET_DISP            _u(0xAE)
#define SSD1306_SET_COM_OUT_DIR     _u(0xC0)
#define SSD1306_SET_COM_OUT_DIR_FLIP _u(0xC0)

#define SSD1306_SET_DISP_OFFSET     _u(0xD3)
#define SSD1306_SET_DISP_CLK_DIV    _u(0xD5)
#define SSD1306_SET_PRECHARGE       _u(0xD9)
#define SSD1306_SET_COM_PIN_CFG     _u(0xDA)
#define SSD1306_SET_VCOM_DESEL      _u(0xDB)

#define SSD1306_PAGE_HEIGHT         _u(8)
#define SSD1306_NUM_PAGES           (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
#define SSD1306_BUF_LEN             (SSD1306_NUM_PAGES * SSD1306_WIDTH)

#define SSD1306_WRITE_MODE         _u(0xFE)
#define SSD1306_READ_MODE          _u(0xFF)


struct render_area {
    uint8_t start_col;
    uint8_t end_col;
    uint8_t start_page;
    uint8_t end_page;

    int buflen;
};

void calc_render_area_buflen(struct render_area *area) {
    // calcular quanto tempo o buffer achatado terá para uma área de renderização
    area->buflen = (area->end_col - area->start_col + 1) * (area->end_page - area->start_page + 1);
}

#ifdef i2c_default

void SSD1306_send_cmd(uint8_t cmd) {
    // O processo de gravação I2C espera um byte de controle seguido por dados
    // esses "dados" podem ser um comando ou dados para acompanhar um comando
    // Co = 1, D/C = 0 => o driver espera um comando
    uint8_t buf[2] = {0x80, cmd};
    i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, buf, 2, false);
}

void SSD1306_send_cmd_list(uint8_t *buf, int num) {
    for (int i=0;i<num;i++)
        SSD1306_send_cmd(buf[i]);
}

void SSD1306_send_buf(uint8_t buf[], int buflen) {
    // no modo de endereçamento horizontal, o ponteiro de endereço da coluna aumenta automaticamente
    // e depois passa para a próxima página, para que possamos enviar o quadro inteiro
    // buffer em um gooooooo!

    // copia nosso buffer de quadro para um novo buffer porque precisamos adicionar o byte de controle
    // até o início

    uint8_t *temp_buf = malloc(buflen + 1);

    temp_buf[0] = 0x40;
    memcpy(temp_buf+1, buf, buflen);

    i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, temp_buf, buflen + 1, false);

    free(temp_buf);
}

void SSD1306_init() {
    //Alguns destes comandos não são estritamente necessários como o reset
    // o processo padroniza alguns deles, mas eles são mostrados aqui
    // para demonstrar como é a sequência de inicialização
    //Alguns valores de configuração são recomendados pelo fabricante da placa

    uint8_t cmds[] = {
        SSD1306_SET_DISP,               // desativar a exibição
        /* mapeando memória */
        SSD1306_SET_MEM_MODE,           // habilita o endereço de memória 0 = horizontal, 1 = vertical, 2 = page
        0x00,                           // horizontal addressing mode
        /* resolution and layout */
        SSD1306_SET_DISP_START_LINE,    // coloca a linha de inicio do display para 0
        SSD1306_SET_SEG_REMAP | 0x01,   // definir remapeamento do segmento, o endereço da coluna 127 é mapeado para SEG0
        SSD1306_SET_MUX_RATIO,          // definir proporção multiplex
        SSD1306_HEIGHT - 1,             // Altura do display - 1
        SSD1306_SET_COM_OUT_DIR | 0x08, // definir a direção de varredura de saída COM (comum). Digitalize de baixo para cima, COM[N-1] a COM0
        SSD1306_SET_DISP_OFFSET,        // definir deslocamento de exibição
        0x00,                           // sem deslocamento
        SSD1306_SET_COM_PIN_CFG,        // definir a configuração de hardware dos pinos COM (comuns). Número mágico específico da placa.
                                        // 0x02 Funciona para 128x32, 0x12 Possivelmente funciona para 128x64. Outras opções 0x22, 0x32
#if ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 32))
        0x02,
#elif ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 64))
        0x12,
#else
        0x02,
#endif
        /* esquema de tempo e direção */
        SSD1306_SET_DISP_CLK_DIV,       // definir a taxa de divisão do clock de exibição
        0x80,                           // proporção div de 1, frequência padrão
        SSD1306_SET_PRECHARGE,          // definir período de pré-carga
        0xF1,                           // Vcc gerado internamente em nosso quadro
        SSD1306_SET_VCOM_DESEL,         // definir o nível de desmarcação do VCOMH
        0x30,                           // 0.83xVcc
        /* display */
        SSD1306_SET_CONTRAST,           // define controle de contraste
        0xFF,
        SSD1306_SET_ENTIRE_ON,          // configure toda a exibição para seguir o conteúdo da RAM
        SSD1306_SET_NORM_DISP,           // definir exibição normal (não invertida)
        SSD1306_SET_CHARGE_PUMP,        // define charge pump
        0x14,                           // Vcc gerado internamente em nosso quadro
        SSD1306_SET_SCROLL | 0x00,      // desativar a rolagem horizontal, se definida. Isto é necessário porque as gravações na memória serão corrompidas se a rolagem estiver habilitada
        SSD1306_SET_DISP | 0x01, // liga o display
    };

    SSD1306_send_cmd_list(cmds, count_of(cmds));
}

void SSD1306_scroll(bool on) {
    // configura a rolagem horizontal
    uint8_t cmds[] = {
        SSD1306_SET_HORIZ_SCROLL | 0x00,
        0x00, // dummy byte
        0x00, // start page 0
        0x00, // time interval
        0x03, // end page 3 SSD1306_NUM_PAGES ??
        0x00, // dummy byte
        0xFF, // dummy byte
        SSD1306_SET_SCROLL | (on ? 0x01 : 0) // Inicia/para rolagem
    };

    SSD1306_send_cmd_list(cmds, count_of(cmds));
}

void render(uint8_t *buf, struct render_area *area) {
    // atualizar uma parte da exibição com uma área de renderização
    uint8_t cmds[] = {
        SSD1306_SET_COL_ADDR,
        area->start_col,
        area->end_col,
        SSD1306_SET_PAGE_ADDR,
        area->start_page,
        area->end_page
    };

    SSD1306_send_cmd_list(cmds, count_of(cmds));
    SSD1306_send_buf(buf, area->buflen);
}

static void SetPixel(uint8_t *buf, int x,int y, bool on) {
    assert(x >= 0 && x < SSD1306_WIDTH && y >=0 && y < SSD1306_HEIGHT);

    // O cálculo para determinar o bit correto a ser definido depende de qual endereço
    // modo em que estamos. Este código assume horizontal

    // A memória RAM de vídeo no SSD1306 é dividida em 8 linhas, um bit por pixel.
    // Cada linha tem 128 pixels de comprimento por 8 pixels de altura, cada byte organizado verticalmente, então o byte 0 é x=0, y=0->7,
    // byte 1 é x = 1, y=0->7 etc.

    // Este código poderia ser otimizado, mas é assim para maior clareza. O compilador
    // deveria fazer um trabalho decente otimizando-o de qualquer maneira.

    const int BytesPerRow = SSD1306_WIDTH ; // x pixels, 1bpp, mas cada linha tem 8 pixels de altura, então (x / 8) * 8

    int byte_idx = (y / 8) * BytesPerRow + x;
    uint8_t byte = buf[byte_idx];

    if (on)
        byte |=  1 << (y % 8);
    else
        byte &= ~(1 << (y % 8));

    buf[byte_idx] = byte;
}
// Bresenhams básicos.
static void DrawLine(uint8_t *buf, int x0, int y0, int x1, int y1, bool on) {

    int dx =  abs(x1-x0);
    int sx = x0<x1 ? 1 : -1;
    int dy = -abs(y1-y0);
    int sy = y0<y1 ? 1 : -1;
    int err = dx+dy;
    int e2;

    while (true) {
        SetPixel(buf, x0, y0, on);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2*err;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static inline int GetFontIndex(uint8_t ch) {
    if (ch >= 'A' && ch <='Z') {
        return  ch - 'A' + 1;
    }
    else if (ch >= '0' && ch <='9') {
        return  ch - '0' + 27;
    }
    else if (ch == '=') {
        return 37; // '=' no índice 37
    } 
    else if (ch == ',') {
        return 38; // ',' no índice 38
    } 
    else if (ch == '%') {
        return 39; // '%' no índice 39
    }
    else if (ch == '.') {
        return 40; // '.' no índice 40
    } 
    else return  0; // Não tenho aquele caractere, então espaço.
}

static void WriteChar(uint8_t *buf, int16_t x, int16_t y, uint8_t ch) {
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
        return;

    // No momento, escreva apenas nos limites da linha Y (a cada 8 pixels verticais)
    y = y/8;

    ch = toupper(ch);
    int idx = GetFontIndex(ch);
    int fb_idx = y * 128 + x;

    for (int i=0;i<8;i++) {
        buf[fb_idx++] = font[idx * 8 + i];
    }
}

void WriteString(uint8_t *buf, int16_t x, int16_t y, char *str) {
    // Retire qualquer string da tela
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
        return;

    while (*str) {
        WriteChar(buf, x, y, *str++);
        x+=8;
    }
}



#endif