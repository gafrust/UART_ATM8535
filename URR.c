/*
 * URR.c
 *
 * Created: 04.05.2026 19:49:20
 *  Author: gafurov
 */ 
#include "uart.h"			// Podkluchenie zagolovochih failov
#include <util/delay.h>




int main(void)
{
   
        init_uart();			// Prototip funkcii propisan v uart.h, a sama funkcia v uart.c
        
        
        // Nastroika porta B: pin 0 kak vihod
        DDRB |= (1 << PB0);   // PB0 = output
		
		 // Nastroika porta D: pin 5 kak vihod
		 DDRD |= (1 << PD5);   // PD5 = output
		 
		 PORTD |= (1 << PD5);   // vkluchit LED
		
		
        
        while(1)
        {
	        out_uart( 0x55 );
	        
	        PORTB |= (1 << PB0);   // vkluchit LED
	        _delay_ms(500);
	        PORTB &= ~(1 << PB0);  // vicluchit LED
	        _delay_ms(500);
        }
}