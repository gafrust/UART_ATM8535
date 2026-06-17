/*			Ispolnaemii fail s funciami obmena po UART		*/

#include "uart.h"

volatile uint8_t rx_buffer = 0;
volatile uint8_t rx_ready = 0;

// Обработчик прерывания приёма
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
	while(!(UCSRA & (1 << TXC)));  // Ждем ПОЛНОЙ отправки
	UCSRA |= (1 << TXC);           // Сбрасываем флаг
	sei();
}

uint8_t in_uart(void) {
	while(!rx_ready);
	rx_ready = 0;
	return rx_buffer;
}

// Блокирующее чтение с таймаутом (миллисекунды)
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
	return 0xFF; // Таймаут
}




// Неблокирующая функция (возвращает 1 если данные есть)
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





























///*			Ispolnaemii fail s funciami obmena po  UART		*/
//
//#include "UART.h"									// Zagolovochnii fail s prototipami i konstantami
//
///****************************************************
		//Nabor funcii dla raboti cherez UART
//****************************************************/
////void init_uart(void){								// Nastroika UART
	////UCSRB = (1<<TXEN|1<<RXEN|1<<RXCIE);						
	////UCSRC = (1<<URSEL|1<<UCSZ1|1<<UCSZ0);
	////UBRRL = SPEED & 0xFF;
	////UBRRH = SPEED >> 8;
////}
//
//void init_uart(void) {
	//UCSRB = (1 << TXEN) | (1 << RXEN) | (1 << RXCIE);
	//UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0) | (1 << UPM1);  // UPM1=1, UPM0=0
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
//uint8_t in_uart(void){								// Priem dannih iz UART
	//while(!(UCSRA&(1<<RXC)));						// Ogidanie prihoda baita
	//return UDR;										// Vozvrachenie prinatogo baita
//}
//
//
//void vkl_tx_485(void){
	//
	//PORTD |=   (1 << PD5);    //DE = 1
	//PORTD &=  ~(1 << PD4);  //RE = 0
	//_delay_us(10);
//}
//
//
//void vkl_rx_485(void){
	//
	//PORTD &= ~(1 << PD5);   //DE = 0
	////PORTD |=  (1 << PD4);   //RE = 1
	//PORTD &=  ~(1 << PD4);  //RE = 0
	//_delay_us(50);
//}
//
//
//volatile uint8_t rx_buffer = 0;
//volatile uint8_t rx_ready = 0;
//
//// Обработчик прерывания приёма
//ISR(USART_RX_vect) {  //
	//rx_buffer = UDR;
	//rx_ready = 1;
//}
//
////// Функция чтения (блокирующая)
////uint8_t in_uart(void) {
	////while(!rx_ready);  // Ждём данные
	////rx_ready = 0;
	////return rx_buffer;
////}
//
////// Неблокирующая функция чтения
////uint8_t in_uart_nonblock(uint8_t *data) {
	////vkl_rx_485();
	////_delay_us(10);
	////if(rx_ready) {
		////*data = rx_buffer;
		////rx_ready = 0;
		////return 1;  // Данные получены
	////}
	////return 0;  // Нет данных
////}
//// Добавить в конец файла uart.c после всех функций:
//
//uint8_t in_uart_nonblock(void) {
	//if (UCSRA & (1 << RXC)) {    // Если данные есть в буфере
		//return UDR;
	//}
	//return 0xFF;  // Возвращаем 0xFF если данных нет
//}
//
//
