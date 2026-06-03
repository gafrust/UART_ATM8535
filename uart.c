
/*			Ispolnaemii fail s funciami obmena po  UART		*/

#include "uart.h"
#include <avr/interrupt.h>  // ? ЭТО ВАЖНО! Для ISR и sei()
#include <avr/io.h>

/****************************************************
		Nabor funcii dla raboti cherez UART
****************************************************/

void init_uart(void) {
    UCSRB = (1 << TXEN) | (1 << RXEN) | (1 << RXCIE);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
    UBRRL = SPEED & 0xFF;
    UBRRH = SPEED >> 8;
}

void out_uart(uint8_t data) {
    while(!(UCSRA & (1 << UDRE)));
    UDR = data;
}

volatile uint8_t rx_buffer = 0;
volatile uint8_t rx_ready = 0;

// Обработчик прерывания приёма
// ВАЖНО: Правильное имя вектора для ATmega8535
ISR(USART_RX_vect) {  // Или USART_RX_vect? Проверьте даташит
    rx_buffer = UDR;
    rx_ready = 1;
}

// Функция чтения (блокирующая)
uint8_t in_uart(void) {
    while(!rx_ready);  // Ждём данные
    rx_ready = 0;
    return rx_buffer;
}

// Неблокирующая функция чтения (опционально)
uint8_t in_uart_nonblock(uint8_t *data) {
    if(rx_ready) {
        *data = rx_buffer;
        rx_ready = 0;
        return 1;  // Данные получены
    }
    return 0;  // Нет данных
}



















///*			Ispolnaemii fail s funciami obmena po  UART		*/
//
//#include "UART.h"									// Zagolovochnii fail s prototipami i konstantami
//
///****************************************************
		//Nabor funcii dla raboti cherez UART
//****************************************************/
//void init_uart(void){			// Nastroika UART
	//UCSRB = (1 << TXEN)  | (1 << RXEN) | (1 << RXCIE);						
	//UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
	//UBRRL = SPEED & 0xFF;
	//UBRRH = SPEED >> 8;
//}
//
////void init_uart(void) {
	////UCSRB = (1<<TXEN|1<<RXEN);
	////UCSRC = (1<<URSEL|1<<UCSZ1|1<<UCSZ0);
	////UBRRL = 24;   // varianti 23,24,25,26,27
	////UBRRH = 0;
////}
//
//
//void out_uart(uint8_t data){						// Peredacha baita cherez UART
	//while(!(UCSRA&(1<<UDRE)));						// Ogidanie gotovnosti  UART к передаче
	//UDR = data;										// Zapis v registr UDR baita dannih nachinaet process peredachi
//}
//
////uint8_t in_uart(void){								// Priem dannih iz UART
	////while(!(UCSRA&(1<<RXC)));						// Ogidanie prihoda baita
	////return UDR;										// Vozvrachenie prinatogo baita
////}
//
//
//volatile uint8_t rx_buffer = 0;
//volatile uint8_t rx_ready = 0;  // Флаг готовности данных
//
//// Обработчик прерывания приёма
//ISR(USART_RXC_vect) {
	//rx_buffer = UDR;           // Сразу читаем, чтобы сбросить флаг
	//rx_ready = 1;              // Сигнализируем, что данные пришли
//}
//
//// Функция чтения (неблокирующая)
//uint8_t in_uart(void) {
	//while(!rx_ready);          // Ждём флаг от прерывания
	//rx_ready = 0;              // Сбрасываем для следующих данных
	//return rx_buffer;
//}