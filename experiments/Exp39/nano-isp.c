#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* Pure-C STK500v1 AVR ISP programmer for ATmega328P (Nano/Uno clone).
   Drop-in equivalent of the ArduinoISP sketch, no Arduino core needed.
   Compile: avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL -Os

   Pin map (ArduinoISP-compatible, Nano pins):
     RESET target : D10 = PB2
     MOSI         : D11 = PB3 (hardware SPI)
     MISO         : D12 = PB4
     SCK          : D13 = PB5
     LED_PMODE    : D7  = PD7
     LED_ERR      : D8  = PB0
   Serial 19200 8N1 on UART0 (PD0 RX / PD1 TX) via CH340. */

#define LED_PMODE PD7
#define LED_ERR   PB0
#define RESET     PB2

#define STK_OK      0x10
#define STK_FAILED  0x11
#define STK_UNKNOWN 0x12
#define STK_INSYNC  0x14
#define STK_NOSYNC  0x15
#define CRC_EOP     0x20

#define HWVER 2
#define SWMAJ 1
#define SWMIN 18

static void delay_ms(uint16_t ms) {
    while (ms--) _delay_ms(1);
}

/* ---------------- UART ---------------- */

static void uart_init(void) {
    UBRR0 = 51;                                  /* 19200 @ 16 MHz */
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);      /* 8N1 */
}

static uint8_t getch_tmo(uint8_t *ok) {
    uint16_t n = 0;
    while (!(UCSR0A & (1 << RXC0))) {
        if (++n > 50000) {          /* ~500 ms timeout */
            if (ok) *ok = 0;
            return 0;
        }
        _delay_us(10);
    }
    if (ok) *ok = 1;
    return UDR0;
}

static void send(uint8_t c) {
    while (!(UCSR0A & (1 << UDRE0))) ;
    UDR0 = c;
}

static void send_str(const char *s) {
    while (*s) send((uint8_t)*s++);
}

static uint8_t serial_available(void) {
    return (UCSR0A & (1 << RXC0)) ? 1 : 0;
}

/* ---------------- SPI ---------------- */

static void spi_init(void) {
    DDRB |= (1 << PB2) | (1 << PB3) | (1 << PB5); /* RESET, MOSI, SCK out */
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0); /* /128 = 125 kHz */
}

static void spi_end(void) {
    SPCR &= ~(1 << SPE);
    DDRB &= ~((1 << PB3) | (1 << PB5));
}

static uint8_t spi_transfer(uint8_t b) {
    SPDR = b;
    while (!(SPSR & (1 << SPIF))) ;
    return SPDR;
}

static uint8_t spi_transaction(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    spi_transfer(a);
    spi_transfer(b);
    spi_transfer(c);
    return spi_transfer(d);
}

/* ---------------- LEDs ---------------- */

static void pulse(uint8_t bit, volatile uint8_t *port, int times) {
    do {
        *port |= (1 << bit);
        delay_ms(30);
        *port &= ~(1 << bit);
        delay_ms(30);
    } while (times--);
}

/* ---------------- state ---------------- */

static int ISPError = 0;
static int pmode = 0;
static unsigned int here;
static uint8_t buff[256];

#define beget16(addr) (*addr * 256 + *(addr + 1))

typedef struct param {
    uint8_t devicecode;
    uint8_t revision;
    uint8_t progtype;
    uint8_t parmode;
    uint8_t polling;
    uint8_t selftimed;
    uint8_t lockbytes;
    uint8_t fusebytes;
    uint8_t flashpoll;
    uint16_t eeprompoll;
    uint16_t pagesize;
    uint16_t eepromsize;
    uint32_t flashsize;
} parameter;

static parameter param;
static int rst_active_high;

static uint8_t fill_tmo(int n) {
    uint8_t ok;
    for (int x = 0; x < n; x++) {
        buff[x] = getch_tmo(&ok);
        if (!ok) return 0;
    }
    return 1;
}

static void reset_target(int reset) {
    if (reset) PORTB |= (1 << RESET);
    else PORTB &= ~(1 << RESET);
}

static void start_pmode(void) {
    reset_target(1);
    DDRB |= (1 << RESET);
    spi_init();
    PORTB &= ~(1 << PB5);       /* SCK low */
    delay_ms(20);
    reset_target(0);
    _delay_us(100);
    reset_target(1);
    delay_ms(50);
    spi_transaction(0xAC, 0x53, 0x00, 0x00);   /* programming enable */
    pmode = 1;
}

static void end_pmode(void) {
    spi_end();
    DDRB &= ~((1 << PB3) | (1 << PB5));
    reset_target(0);
    DDRB &= ~(1 << RESET);
    pmode = 0;
}

/* ---------------- protocol replies ---------------- */

static void empty_reply(void) {
    uint8_t ok;
    uint8_t c = getch_tmo(&ok);
    if (ok && c == CRC_EOP) {
        send(STK_INSYNC);
        send(STK_OK);
    } else {
        ISPError++;
        send(STK_NOSYNC);
    }
}

static void breply(uint8_t b) {
    uint8_t ok;
    uint8_t c = getch_tmo(&ok);
    if (ok && c == CRC_EOP) {
        send(STK_INSYNC);
        send(b);
        send(STK_OK);
    } else {
        ISPError++;
        send(STK_NOSYNC);
    }
}

static void get_version(uint8_t c) {
    switch (c) {
        case 0x80: case 0x96: breply(HWVER); break;
        case 0x81: case 0x97: breply(SWMAJ); break;
        case 0x82: case 0x98: breply(SWMIN); break;
        case 0x93: breply('S'); break;
        default: breply(0);
    }
}

static void set_parameters(void) {
    param.devicecode = buff[0];
    param.revision = buff[1];
    param.progtype = buff[2];
    param.parmode = buff[3];
    param.polling = buff[4];
    param.selftimed = buff[5];
    param.lockbytes = buff[6];
    param.fusebytes = buff[7];
    param.flashpoll = buff[8];
    param.eeprompoll = beget16(&buff[10]);
    param.pagesize = beget16(&buff[12]);
    param.eepromsize = beget16(&buff[14]);
    param.flashsize = (uint32_t)buff[16] << 24 | (uint32_t)buff[17] << 16 |
                      (uint32_t)buff[18] << 8 | buff[19];
    rst_active_high = (param.devicecode >= 0xe0);
}

/* ---------------- target operations ---------------- */

static uint8_t flash(uint8_t hilo, unsigned int addr, uint8_t data) {
    return spi_transaction(0x40 + 8 * hilo, (addr >> 8) & 0xFF, addr & 0xFF, data);
}

static void commit(unsigned int addr) {
    spi_transaction(0x4C, (addr >> 8) & 0xFF, addr & 0xFF, 0);
    delay_ms(30);
}

static unsigned int current_page(void) {
    if (param.pagesize == 32) return here & 0xFFFFFFF0;
    if (param.pagesize == 64) return here & 0xFFFFFFE0;
    if (param.pagesize == 128) return here & 0xFFFFFFC0;
    if (param.pagesize == 256) return here & 0xFFFFFF80;
    return here;
}

static uint8_t write_flash_pages(int length) {
    int x = 0;
    unsigned int page = current_page();
    while (x < length) {
        if (page != current_page()) {
            commit(page);
            page = current_page();
        }
        flash(0, here, buff[x++]);
        flash(1, here, buff[x++]);
        here++;
    }
    commit(page);
    return STK_OK;
}

static void write_flash(int length) {
    if (!fill_tmo(length)) {
        ISPError++;
        send(STK_NOSYNC);
        return;
    }
    uint8_t ok;
    uint8_t eop = getch_tmo(&ok);
    if (ok && eop == CRC_EOP) {
        send(STK_INSYNC);
        send(write_flash_pages(length));
    } else {
        ISPError++;
        send(STK_NOSYNC);
    }
}

#define EECHUNK 32

static uint8_t write_eeprom_chunk(unsigned int start, unsigned int length) {
    if (!fill_tmo(length)) {
        ISPError++;
        return STK_FAILED;
    }
    for (unsigned int x = 0; x < length; x++) {
        unsigned int addr = start + x;
        spi_transaction(0xC0, (addr >> 8) & 0xFF, addr & 0xFF, buff[x]);
        delay_ms(45);
    }
    return STK_OK;
}

static uint8_t write_eeprom(unsigned int length) {
    unsigned int start = here * 2;
    unsigned int remaining = length;
    if (length > param.eepromsize) {
        ISPError++;
        return STK_FAILED;
    }
    while (remaining > EECHUNK) {
        write_eeprom_chunk(start, EECHUNK);
        start += EECHUNK;
        remaining -= EECHUNK;
    }
    write_eeprom_chunk(start, remaining);
    return STK_OK;
}

static void program_page(void) {
    uint8_t ok;
    unsigned int length = 256 * getch_tmo(&ok);
    if (!ok) { ISPError++; send(STK_NOSYNC); return; }
    length += getch_tmo(&ok);
    if (!ok) { ISPError++; send(STK_NOSYNC); return; }
    char memtype = (char)getch_tmo(&ok);
    if (!ok) { ISPError++; send(STK_NOSYNC); return; }
    if (memtype == 'F') {
        write_flash(length);
        return;
    }
    if (memtype == 'E') {
        uint8_t r = write_eeprom(length);
        uint8_t eop = getch_tmo(&ok);
        if (ok && eop == CRC_EOP) {
            send(STK_INSYNC);
            send(r);
        } else {
            ISPError++;
            send(STK_NOSYNC);
        }
        return;
    }
    send(STK_FAILED);
}

static uint8_t flash_read(uint8_t hilo, unsigned int addr) {
    return spi_transaction(0x20 + hilo * 8, (addr >> 8) & 0xFF, addr & 0xFF, 0);
}

static char flash_read_page(int length) {
    for (int x = 0; x < length; x += 2) {
        uint8_t low = flash_read(0, here);
        send(low);
        uint8_t high = flash_read(1, here);
        send(high);
        here++;
    }
    return STK_OK;
}

static char eeprom_read_page(int length) {
    int start = here * 2;
    for (int x = 0; x < length; x++) {
        int addr = start + x;
        uint8_t ee = spi_transaction(0xA0, (addr >> 8) & 0xFF, addr & 0xFF, 0xFF);
        send(ee);
    }
    return STK_OK;
}

static void read_page(void) {
    uint8_t ok;
    int length = 256 * getch_tmo(&ok);
    if (!ok) { ISPError++; send(STK_NOSYNC); return; }
    length += getch_tmo(&ok);
    if (!ok) { ISPError++; send(STK_NOSYNC); return; }
    char memtype = (char)getch_tmo(&ok);
    if (!ok) { ISPError++; send(STK_NOSYNC); return; }
    uint8_t eop = getch_tmo(&ok);
    if (!ok || eop != CRC_EOP) { ISPError++; send(STK_NOSYNC); return; }
    send(STK_INSYNC);
    char result = (char)STK_FAILED;
    if (memtype == 'F') result = flash_read_page(length);
    if (memtype == 'E') result = eeprom_read_page(length);
    send(result);
}

static void read_signature(void) {
    uint8_t ok;
    uint8_t eop = getch_tmo(&ok);
    if (!ok || eop != CRC_EOP) { ISPError++; send(STK_NOSYNC); return; }
    send(STK_INSYNC);
    send(spi_transaction(0x30, 0x00, 0x00, 0x00));
    send(spi_transaction(0x30, 0x00, 0x01, 0x00));
    send(spi_transaction(0x30, 0x00, 0x02, 0x00));
    send(STK_OK);
}

static void universal(void) {
    if (!fill_tmo(4)) { ISPError++; send(STK_NOSYNC); return; }
    uint8_t ch = spi_transaction(buff[0], buff[1], buff[2], buff[3]);
    breply(ch);
}

/* ---------------- command dispatcher ---------------- */

static void avrisp(void) {
    uint8_t ok;
    uint8_t ch = getch_tmo(&ok);
    switch (ch) {
        case '0':
            ISPError = 0;
            empty_reply();
            break;
        case '1': {
            uint8_t c = getch_tmo(&ok);
            if (ok && c == CRC_EOP) {
                send(STK_INSYNC);
                send_str("AVR ISP");
                send(STK_OK);
            } else {
                ISPError++;
                send(STK_NOSYNC);
            }
            break;
        }
        case 'A': {
            uint8_t c = getch_tmo(&ok);
            if (ok) get_version(c);
            else { ISPError++; send(STK_NOSYNC); }
            break;
        }
        case 'B':
            if (fill_tmo(20)) {
                set_parameters();
                empty_reply();
            } else {
                ISPError++;
                send(STK_NOSYNC);
            }
            break;
        case 'E':
            if (fill_tmo(5)) empty_reply();
            else { ISPError++; send(STK_NOSYNC); }
            break;
        case 'P':
            if (!pmode)
                start_pmode();
            empty_reply();
            break;
        case 'U': {
            uint8_t lo = getch_tmo(&ok);
            if (!ok) { ISPError++; send(STK_NOSYNC); break; }
            uint8_t hi = getch_tmo(&ok);
            if (!ok) { ISPError++; send(STK_NOSYNC); break; }
            here = lo + 256 * hi;
            empty_reply();
            break;
        }
        case 0x60:
            getch_tmo(&ok);
            getch_tmo(&ok);
            empty_reply();
            break;
        case 0x61:
            getch_tmo(&ok);
            empty_reply();
            break;
        case 0x64:
            program_page();
            break;
        case 0x74:
            read_page();
            break;
        case 'V':
            universal();
            break;
        case 'Q':
            ISPError = 0;
            end_pmode();
            empty_reply();
            break;
        case 0x75:
            read_signature();
            break;
        case CRC_EOP:
            ISPError++;
            send(STK_NOSYNC);
            break;
        default: {
            uint8_t c = getch_tmo(&ok);
            if (ok && c == CRC_EOP) {
                send(STK_UNKNOWN);
            } else {
                ISPError++;
                send(STK_NOSYNC);
            }
            break;
        }
    }
}

int main(void) {
    DDRD |= (1 << LED_PMODE);
    DDRB |= (1 << LED_ERR);
    pulse(LED_PMODE, &PORTD, 2);
    pulse(LED_ERR, &PORTB, 2);
    uart_init();

    for (;;) {
        if (pmode) PORTD |= (1 << LED_PMODE);
        else PORTD &= ~(1 << LED_PMODE);
        if (ISPError) PORTB |= (1 << LED_ERR);
        else PORTB &= ~(1 << LED_ERR);
        if (serial_available())
            avrisp();
    }
    return 0;
}
