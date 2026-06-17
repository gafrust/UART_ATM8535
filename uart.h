/*		Zagolovochnii fail uart.h		*/

#ifndef UART_H_
#define UART_H_

#define F_CPU 7372800UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

//////////////////////////////////////////
//				Parametri UART			//
//////////////////////////////////////////
#define	BAUD	 115200UL
#define SPEED	((F_CPU+BAUD*8)/(BAUD*16)-1)
extern volatile uint8_t rx_ready;

//////////////////////////////////////////
//			PROTOTIPI FUNCII			//
//////////////////////////////////////////
uint8_t	in_uart(void);
void	out_uart(uint8_t data);
void	init_uart(void);
void	send_4_bytes_from_eeprom(uint16_t start_address);
void	send_eeprom_data_4byte(void);
uint8_t	in_uart_timeout(uint16_t timeout_ms);
uint8_t	in_uart_nonblock(uint8_t *data);
void	eeprom_write_byte(unsigned int uiAddress, unsigned char ucData);
void	handle_uart_commands(void);
void	send_eeprom_data_loop(void);
void	vkl_tx_485(void);
void	vkl_rx_485(void);
void    otkl_485(void);
void clear_uart_buffer(void);

#endif /* UART_H_ */



















///*		Zagolovochnii fail uart.с
//
//Sodergit prototipi funcii i constanti dla ispolzovania v drugih modulah
//*/
//
//#ifndef UART_H_						// Zachita ot povtornogo vkluchenia faila
//#define UART_H_
//#define F_CPU 7372800UL
//#include <avr/io.h>
//#include <util/delay.h>
//#include <avr/interrupt.h>
//
////#define F_CPU	1000000UL						// Taktovaa chactota microcontrollera
//
////#define F_CPU 7372770UL
//
//
////////////////////////////////////////////
////				Parametri UART			//
////////////////////////////////////////////
//#define	BAUD	 115200UL //2400UL	460800UL	 					// Scorost obmena po UART
//#define SPEED	((F_CPU+BAUD*8)/(BAUD*16)-1)
//
//
////////////////////////////////////////////
////			PROTOTIPI FUNCII			//
////////////////////////////////////////////
//uint8_t	in_uart			(void);
//void	out_uart		(uint8_t);//?
//void	init_uart		(void);
//void send_4_bytes_from_eeprom(uint16_t);
//void send_eeprom_data_4byte(void);
//// Добавить прототип в uart.h
//uint8_t in_uart_nonblock(void);
//
//// Добавить в конец файла перед #endif
//void eeprom_write_byte(unsigned int uiAddress, unsigned char ucData);
//void handle_uart_commands(void);
//void send_eeprom_data_loop(void);
//void vkl_tx_485(void);
//void vkl_rx_485(void);
//
//
//#endif /* UART_H_ */