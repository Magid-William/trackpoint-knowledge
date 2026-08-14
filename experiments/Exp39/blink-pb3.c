#include <avr/io.h>
#include <util/delay.h>

#define LED PB3

int main(void)
{
    DDRB |= (1 << LED);

    for (;;) {
        PORTB |= (1 << LED);
        _delay_ms(100);
        PORTB &= ~(1 << LED);
        _delay_ms(100);
        PORTB |= (1 << LED);
        _delay_ms(100);
        PORTB &= ~(1 << LED);
        _delay_ms(1000);
    }
}
