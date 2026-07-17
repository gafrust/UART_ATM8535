/*			Ispolnaemii fail s funciami obmena po UART		*/

#include "uart.h"

volatile uint8_t rx_buffer = 0;
volatile uint8_t rx_ready = 0;

// Obrabotchik prerivania priema
ISR(USART_RX_vect) {
	rx_buffer = UDR;
	rx_ready = 1;
}






void init_uart(void) {
	UCSRB = (1 << TXEN) | (1 << RXEN) | (1 << RXCIE);
	UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0) | (1 << UPM1);
	UBRRL = SPEED & 0xFF;
	UBRRH = SPEED >> 8;
}

//void out_uart(uint8_t data) {
	//while(!(UCSRA & (1 << UDRE)));
	//UDR = data;
	 //_delay_us(10);
//}

void out_uart(uint8_t data) {
	cli();
	while(!(UCSRA & (1 << UDRE)));
	UDR = data;
	while(!(UCSRA & (1 << TXC)));  // Gdem polnoi otpravki
	UCSRA |= (1 << TXC);           // Sbrasivaem flag
	sei();
}

uint8_t in_uart(void) {
	while(!rx_ready);
	rx_ready = 0;
	return rx_buffer;
}

// Blocirujushee chtenie s taimautom (ms)
uint8_t in_uart_timeout(uint16_t timeout_ms) {
	uint16_t timeout = 0;
	while(timeout < timeout_ms) {
		if(rx_ready) {
			rx_ready = 0;
			return rx_buffer;
		}
		_delay_ms(1);
		timeout++;
	}
	return 0xFF; // Taimaut
}




// Neblokirujuchaa funkcia chtenia (vozvrashaet 1 esli dannie est)
uint8_t in_uart_nonblock(uint8_t *data) {
	if(rx_ready) {
		*data = rx_buffer;
		rx_ready = 0;
		return 1;
	}
	return 0;
}

void vkl_tx_485(void) {
	PORTD |= (1 << PD5);    // DE = 1
	PORTD &= ~(1 << PD4);   // RE = 0
	_delay_us(10);
}

void vkl_rx_485(void) {
	PORTD &= ~(1 << PD5);   // DE = 0
	PORTD &= ~(1 << PD4);   // RE = 0
	_delay_us(10);
}

void otkl_485(void) {
	PORTD &= ~(1 << PD5);   // DE = 0
	PORTD |= (1 << PD4);   // RE = 1
	_delay_us(10);
}