/*			Ispolnaemii fail s funciami obmena po  UART		*/

#include "UART.h"									// Zagolovochnii fail s prototipami i konstantami

/****************************************************
		Nabor funcii dla raboti cherez UART
****************************************************/
//void init_uart(void){								// Nastroika UART
//	UCSRB = (1<<TXEN|1<<RXEN);						
//	UCSRC = (1<<URSEL|1<<UCSZ1|1<<UCSZ0);
//	UBRRL = SPEED & 0xFF;
//	UBRRH = SPEED >> 8;
//}

void init_uart(void) {
	UCSRB = (1<<TXEN|1<<RXEN);
	UCSRC = (1<<URSEL|1<<UCSZ1|1<<UCSZ0);
	UBRRL = 24;   // varianti 23,24,25,26,27
	UBRRH = 0;
}


void out_uart(uint8_t data){						// Peredacha baita cherez UART
	while(!(UCSRA&(1<<UDRE)));						// Ogidanie gotovnosti  UART к передаче
	UDR = data;										// Zapis v registr UDR baita dannih nachinaet process peredachi
}

uint8_t in_uart(void){								// Priem dannih iz UART
	while(!(UCSRA&(1<<RXC)));						// Ogidanie prihoda baita
	return UDR;										// Vozvrachenie prinatogo baita
}
