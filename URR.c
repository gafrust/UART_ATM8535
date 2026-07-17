#include "uart.h"
#include <util/delay.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdio.h>

// ---------- Konfiguracia EEPROM ----------
#define EEPROM_SIZE    1024
#define CMD_ADDR        0
#define CMD_START       1
#define MAX_COMMANDS    100

// ---------- Regim raboti ----------
#define MODE_NORMAL     0
#define MODE_PROGRAM    1

volatile uint8_t current_mode = MODE_NORMAL;

const char build_date[] PROGMEM __attribute__((used)) = "Build Date: " __DATE__;
const char build_time[] PROGMEM __attribute__((used)) = "Build Time: " __TIME__;



unsigned char eeprom_read_byte(unsigned int uiAddress) {
	uint8_t sreg = SREG;  // Sohranaem status prerivanii
	cli();                // Zapreshaem prerivania
	
	while (EECR & (1 << EEWE));  // Gdem zavershenia zapisi
	
	EEAR = uiAddress;     // Ustanavlivaem adress
	EECR |= (1 << EERE);  // Zapuskaem chtenie
	
	SREG = sreg;          // Vostanavlivaem prerivania
	return EEDR;          // Vozvrashaem dannie
}



void eeprom_write_byte(unsigned int uiAddress, unsigned char ucData) {
	while (EECR & (1 << EEWE));
	EEAR = uiAddress;
	EEDR = ucData;
	EECR |= (1 << EEMWE);
	EECR |= (1 << EEWE);
}

// ---------- Otpravka vseh dannih iz EEPROM ----------
void send_all_eeprom_data(void) {
	uint8_t cmd_count = eeprom_read_byte(CMD_ADDR);
	out_uart(cmd_count);
	_delay_ms(10);

	for (uint8_t i = 0; i < cmd_count; i++) {
		uint16_t addr = CMD_START + i * 4;
		for (uint8_t j = 0; j < 4; j++) {
			out_uart(eeprom_read_byte(addr + j));
			_delay_ms(10);
		}
	}
}

// ---------- Otpravka 4 bait iz EEPROM ----------
void send_4_bytes_from_eeprom(uint16_t start_address) {
	if (start_address + 3 >= EEPROM_SIZE) {
		vkl_tx_485();
		out_uart('E');
		out_uart('R');
		vkl_rx_485();
		return;
	}
	
	vkl_tx_485();
	for (uint8_t i = 0; i < 4; i++) {
		uint8_t data = eeprom_read_byte(start_address + i);
		out_uart(data);
		_delay_ms(5);
	}
	vkl_rx_485();
}


void clear_uart_buffer(void) {
	// Chitaem  UDR, poka est dannie
	while (UCSRA & (1 << RXC)) {
		(void)UDR;  // Chitaem i ignoriruem
	}
	// Sbrasivaem flag gotovnosti
	rx_ready = 0;
}


#define WAIT_BYTE(x) while(!in_uart_nonblock(&(x))) // { _delay_ms(1); }
	
	void handle_programming_mode(void) {
		vkl_rx_485();
		clear_uart_buffer();
		
		uint8_t cmd;
		WAIT_BYTE(cmd);
		
		// Proveriaem chto komanda validnaa
		if(cmd != 'R' && cmd != 'W' && cmd != 'X') {
			// Neizvestnaa komanda - otpravlaem ERROR
			vkl_tx_485();
			clear_uart_buffer();
			out_uart('E');
			out_uart('R');
			//while(!(UCSRA & (1 << TXC)));
			vkl_rx_485();
			clear_uart_buffer();
			//return;  // Vihodim, ne zavisaem
		}


	switch (cmd) {
		case 'W': {
			uint8_t ah, al, data;
			WAIT_BYTE(ah);
			WAIT_BYTE(al);
			WAIT_BYTE(data);
			
			uint16_t addr = (ah << 8) | al;
			if (addr < EEPROM_SIZE) {
				clear_uart_buffer();
				//Snachala perecluchaemsa na peredachu otveta
				vkl_tx_485();
				
				// Zapisivaem v EEPROM
				eeprom_write_byte(addr, data);
				
				// Nebolshaya zadergka posle zapisi v  EEPROM
 				_delay_ms(1);
				
				// Otpravlaem podtvergdenia
				out_uart('O');
				out_uart('K');
				
				// Vozvrashaemsa v regim priema
				vkl_rx_485();
				} else {
				vkl_tx_485();
				out_uart('E');
				out_uart('R');
				vkl_rx_485();
			}
		}
		break;
        
		
		
		case 'R': {
			
			uint8_t ah, al;
			WAIT_BYTE(ah);
			WAIT_BYTE(al);
			uint16_t addr = (ah << 8) | al;
			if (addr < EEPROM_SIZE) {
				clear_uart_buffer();
				uint8_t data = eeprom_read_byte(addr);
				vkl_tx_485();
				
				_delay_ms(1);
				
				out_uart(data);
				
				//out_uart(ah);
				//out_uart(al);
				vkl_rx_485();
				} else {
				vkl_tx_485();
				out_uart(0xFE);
				vkl_rx_485();
			}
		}
		break;
		
		
		case 'B':   // Zapis bloka: B <len> <addr_high> <addr_low> <data0> ... <dataN-1>
		{
			uint8_t len = in_uart();
			uint8_t ah = in_uart();
			uint8_t al = in_uart();
			uint16_t addr = (ah << 8) | al;
			if (addr + len <= EEPROM_SIZE) {
				for (uint8_t i = 0; i < len; i++) {
					eeprom_write_byte(addr + i, in_uart());
				}
				out_uart('O'); out_uart('K');
				} else {
				out_uart(ah); out_uart(al); out_uart(len);//out_uart('E'); out_uart('R');
			}
		}
		break;
		

		case 'X':
		current_mode = MODE_NORMAL;
		clear_uart_buffer();
		vkl_tx_485();
		_delay_ms(10);
		out_uart('O');
		out_uart('K');
		clear_uart_buffer();
		_delay_ms(10);
		vkl_rx_485();
		break;
		
		default:
		break;
	}
}






// ---------- Osnovnoi cikl otveta po 4 baita ----------

void send_eeprom_data_4byte(void) {
	uint8_t cmd;
	static uint16_t cooldown = 0;
	
	// Esli taimaut aktiven
	if (cooldown > 0) {
		cooldown--;
		clear_uart_buffer();
		return;
	}
	
	if (!in_uart_nonblock(&cmd)) {
		return;  // Net komandi - vihodim
			}
	
	// *** Perehod v regim programmirovania 250 ***
	if (cmd == 250) {
		// Perekluchaemsa v regim programmirovania
		current_mode = MODE_PROGRAM;
		clear_uart_buffer();
		
		// Otpravlaem podtvergdenie
		vkl_tx_485();
		_delay_ms(10);
		out_uart('P');
		out_uart('R');
		out_uart('O');
		out_uart('G');
		_delay_ms(10);
		vkl_rx_485();
		
		cooldown = 10;  // Zachita ot povtorov
		clear_uart_buffer();
		return;
	}
	
	
	
	// Obrabotka komand 254 i 255
	if (cmd == 254 || cmd == 255) {
		uint16_t addr;
		if (cmd == 254) {
			addr = 0x0330;  // Adres dla komandi 254
			} else { // cmd == 254
			addr = 0x0334;  // Adres dla komandi 255
		}
		
		// Proveraem, chto adres v predelah EEPROM
		if (addr + 3 < EEPROM_SIZE) {
			send_4_bytes_from_eeprom(addr);
			cooldown = 10;  // Zachita ot povtorov
			} else {
			// Esli adres vne diapazona - vidaem 0x00 0x00 0x00 0x00
			vkl_tx_485();
			for (uint8_t i = 0; i < 4; i++) {
				out_uart(0x00);
				_delay_ms(5);
			}
			vkl_rx_485();
			cooldown = 5;
		}
		clear_uart_buffer();
		return;
	}
	
	// Obrabotka normalnih komand (ot 5 do 200)
	if (cmd >= 1 && cmd <= 50) {
		uint8_t block_num = cmd;// - 4;
		uint16_t addr = 0x00 + (block_num - 1) * 4;
		
		if (addr + 3 < EEPROM_SIZE) {
			send_4_bytes_from_eeprom(addr);
			cooldown = 10;
			} else {
			vkl_tx_485();
			out_uart('E');
			out_uart('R');
			vkl_rx_485();
			cooldown = 5;
		}
		clear_uart_buffer();
		return;
	} else {
	
	// Neizvestnaya komanda - vidaem 0x00 0x00 0x00 0x00
	clear_uart_buffer();
	vkl_tx_485();
	for (uint8_t i = 0; i < 4; i++) {
		out_uart(0x00);
		_delay_ms(5);
	                                }
	vkl_rx_485();
	clear_uart_buffer();
	       }
}



// ---------- Ogidanie komandi perehoda v regim programmirovania----------
void wait_for_programming_mode(void) {
	uint16_t timeout = 0;
	uint8_t ch;
	
	while (timeout < 6000) {  // ~6 sekund
		if (in_uart_nonblock(&ch)) {
			if (ch == 'P') {
				current_mode = MODE_PROGRAM;
				vkl_tx_485();
				cli();
				out_uart('P');
				out_uart('R');
				out_uart('O');
				out_uart('G');
				vkl_rx_485();
				sei();
				return;
			}
		}
		_delay_ms(1);
		timeout++;
	}
}





// ---------- Zapis dati sborki v EEPROM ----------
void write_build_info_to_eeprom(void) {
	uint16_t addr = 0x0350;  // Adres v EEPROM
	char buffer[30];
	
	// Zapisivaem datu
	strcpy_P(buffer, build_date);
	for (uint8_t i = 0; buffer[i] != '\0'; i++) {
		eeprom_write_byte(addr + i, buffer[i]);
	}
	eeprom_write_byte(addr + strlen_P(build_date), '\0');  // Zavershaushii null
	
}

#include <string.h>   // for strcmp

// Vspomogatelnaya funkcia: mesac (3 bukvi) ? nomer (1-12)
uint8_t month_to_num(const char *mon) {
	const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
	"Jul","Aug","Sep","Oct","Nov","Dec"};
	for (uint8_t i = 0; i < 12; i++) {
		if (strcmp(mon, months[i]) == 0) return i + 1;
	}
	return 0;
}

// Zapis kompaktnoi dati (4 baita) v EEPROM po adresu 0x0330

void write_compact_date_to_eeprom(void) {
	char date_str[30];
	strcpy_P(date_str, build_date); // "Build Date: Jun 19 2026"

	// ====== Propuskaim "Build Date: " ======
	char* p = date_str + 12; // teper p ukazivaet na "Jun 19 2026"

	char mon[4];
	int day, year;
	if (sscanf(p, "%3s %d %d", mon, &day, &year) != 3) {
		return; // oshibka parsinga
	}

	uint8_t month = month_to_num(mon);
	if (month == 0) return;

	uint8_t data[4] = {
		 month,              // month (6)
		(uint8_t)day,       // day (19)
		(uint8_t)(year / 100), // starhaja chast goda (20)
		(uint8_t)(year % 100)  // mladshaja chast goda (26)
	};

	uint16_t addr =  0x0330;
	for (uint8_t i = 0; i < 4; i++) {
		eeprom_write_byte(addr + i, data[i]);
	}
}









// ---------- Main ----------
int main(void) {
	init_uart();
	sei();  // 
	
	
	
	write_build_info_to_eeprom();
	_delay_ms(10);
	write_compact_date_to_eeprom();
	
	DDRD |= (1 << PD4);
	DDRD |= (1 << PD5);
	//SFIOR |= (1 << PUD);
	
	vkl_rx_485();
	
	// Nebolshaja zadergka pered proverkoi komandi
	_delay_ms(200);
	
	wait_for_programming_mode();
	
	// Osnovnoi cikl
	while (1) {
		if (current_mode == MODE_PROGRAM) {
			handle_programming_mode();
			} else {
			vkl_rx_485();
			send_eeprom_data_4byte();
		}
		_delay_ms(1);
	}
}







