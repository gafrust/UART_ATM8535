/*		Zagolovochnii fail uart.ñ

Sodergit prototipi funcii i constanti dla ispolzovania v drugih modulah
*/

#ifndef UART_H_						// Zachita ot povtornogo vkluchenia faila
#define UART_H_

#include <avr/io.h>

//#define F_CPU	1000000UL						// Taktovaa chactota microcontrollera
#define F_CPU 7372800UL
//#define F_CPU 7372770UL


//////////////////////////////////////////
//				Parametri UART			//
//////////////////////////////////////////
#define	BAUD	460800UL //115200UL //2400UL							// Scorost obmena po UART
#define SPEED	((F_CPU+BAUD*8)/(BAUD*16)-1)


//////////////////////////////////////////
//			PROTOTIPI FUNCII			//
//////////////////////////////////////////
uint8_t	in_uart			(void);
void	out_uart		(uint8_t);
void	init_uart		(void);


#endif /* UART_H_ */