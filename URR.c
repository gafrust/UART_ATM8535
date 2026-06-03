#include "uart.h"
#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// ---------- Конфигурация EEPROM ----------
#define EEPROM_SIZE     512
#define CMD_ADDR        0       // ячейка 0 хранит счётчик команд (макс. 100)
#define CMD_START       1       // начало массива команд (4 байта на команду)
#define MAX_COMMANDS    100


// ---------- Конфигурация ----------
#define TEMP_BUFFER_SIZE 256  // Размер временного буфера



// Буфер для временного хранения
uint8_t temp_buffer[TEMP_BUFFER_SIZE];
uint16_t temp_buffer_len = 0;
uint16_t temp_buffer_addr = 0;
uint8_t temp_buffer_ready = 0;





// ---------- Режимы работы ----------
#define MODE_NORMAL     0       // постоянная передача данных из EEPROM
#define MODE_PROGRAM    1       // режим программирования (приём/запись)

// Глобальные переменные
volatile uint8_t current_mode = MODE_NORMAL;

// ---------- Работа с EEPROM (чтение/запись) ----------
unsigned char eeprom_read_byte(unsigned int uiAddress) {
	while (EECR & (1 << EEWE));   // ждём завершения записи
	EEAR = uiAddress;
	EECR |= (1 << EERE);
	return EEDR;
}

//void eeprom_write_byte(unsigned int uiAddress, unsigned char ucData) {
//	while (EECR & (1 << EEWE));   // ждём готовности
//;	EEAR = uiAddress;
//	EEDR = ucData;
//	EECR |= (1 << EEWE);          // запуск записи
//}

void eeprom_write_byte(unsigned int uiAddress, unsigned char ucData) {
	// Ждём окончания предыдущей записи (если была)
	while (EECR & (1 << EEWE));
	// Устанавливаем адрес и данные
	EEAR = uiAddress;
	EEDR = ucData;
	// Последовательность для записи: сначала EEMWE, потом EEWE
	EECR |= (1 << EEMWE);
	EECR |= (1 << EEWE);
}






// ---------- Отправка всех данных из EEPROM (как было) ----------
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


//---------------------------------------------------------------------------
// ---------- Отправка 4 байт из EEPROM начиная с указанного адреса ----------
void send_4_bytes_from_eeprom(uint16_t start_address) {
	// Проверка, чтобы адрес не выходил за пределы EEPROM
	if (start_address+3 >= EEPROM_SIZE) {
		// Если выход за границы - отправляем ошибку
		out_uart('E');
		out_uart('R');
		return;
	}
	
	// Читаем и отправляем 4 байта последовательно
	for (uint8_t i = 0; i < 4; i++) {
		uint8_t data = eeprom_read_byte(start_address + i);
		out_uart(data);
		_delay_ms(5);  // Небольшая задержка между байтами для надежности
	}
}

//-------------------------------------------------------------------------


// ---------- Функции буфера ----------
void buffer_write_byte(uint8_t data) {
	if(temp_buffer_len < TEMP_BUFFER_SIZE) {
		temp_buffer[temp_buffer_len++] = data;
	}
}

void buffer_clear(void) {
	temp_buffer_len = 0;
	temp_buffer_ready = 0;
}

void buffer_flush_to_eeprom(void) {
	if(!temp_buffer_ready) return;
	
	// Выключаем прерывания на время записи в EEPROM
	cli();
	
	for(uint16_t i = 0; i < temp_buffer_len; i++) {
		eeprom_write_byte(temp_buffer_addr + i, temp_buffer[i]);
	}
	
	sei();
	
	buffer_clear();
	
	// Сигнализируем о завершении
	out_uart('F'); out_uart('L'); out_uart('S'); out_uart('H');
}

// ---------- Модифицированная обработка команд ----------
void handle_programming_mode(void) {
	uint8_t cmd = in_uart();

	switch (cmd) {
		case 'W':   // Запись байта (сразу в EEPROM)
		{
			uint8_t ah = in_uart();
			uint8_t al = in_uart();
			uint8_t data = in_uart();
			uint16_t addr = (ah << 8) | al;
			if (addr < EEPROM_SIZE) {
				eeprom_write_byte(addr, data);
				out_uart('O'); out_uart('K');
				} else {
				out_uart('E'); out_uart('R');
			}
		}
		break;

		case 'R':   // Чтение байта
		{
			uint8_t ah = in_uart();
			uint8_t al = in_uart();
			uint16_t addr = (ah << 8) | al;
			if (addr < EEPROM_SIZE) {
				uint8_t data = eeprom_read_byte(addr);
				out_uart(data);
				} else {
				out_uart(0xFF);
			}
		}
		break;

		case 'B':   // Запись блока в БУФЕР (быстро!)
		{
			uint8_t len = in_uart();
			uint8_t ah = in_uart();
			uint8_t al = in_uart();
			uint16_t addr = (ah << 8) | al;
			
			if (addr + len <= EEPROM_SIZE && len <= TEMP_BUFFER_SIZE) {
				buffer_clear();
				temp_buffer_addr = addr;
				
				// Быстрый прием в буфер
				for (uint8_t i = 0; i < len; i++) {
					buffer_write_byte(in_uart());
				}
				
				temp_buffer_ready = 1;
				out_uart('O'); out_uart('K');
				} else {
				out_uart('E'); out_uart('R');
				out_uart('!');
			}
		}
		break;
		
		case 'F':   // Flush - записать буфер в EEPROM
		{
			if(temp_buffer_ready && temp_buffer_len > 0) {
				buffer_flush_to_eeprom();
				out_uart('O'); out_uart('K');
				} else {
				out_uart('E'); out_uart('M');
				out_uart('P');
			}
		}
		break;
		
		case 'C':   // Clear - очистить буфер без записи
		{
			buffer_clear();
			out_uart('O'); out_uart('K');
		}
		break;

		case 'X':   // Выход из режима программирования
		current_mode = MODE_NORMAL;
		out_uart('O'); out_uart('K');
		break;
	}
}



//void send_4_bytes_from_eeprom(uint16_t start_address) {
	//// Временный тест: отправляем фиксированные байты
	//out_uart(0xAA);
	//out_uart(0xBB);
	//out_uart(0xCC);
	//out_uart(0xDD);
//}



// ---------- Обработка команд в режиме программирования ----------
//void handle_programming_mode(void) {
	//uint8_t cmd = in_uart();   // блокирующее ожидание байта
//
	//switch (cmd) {
		//case 'W':   // Запись байта: W <addr_high> <addr_low> <data>
		//{
			//uint8_t ah = in_uart();
			//uint8_t al = in_uart();
			//uint8_t data = in_uart();
			//uint16_t addr = (ah << 8) | al;
			//if (addr < EEPROM_SIZE) {
				//eeprom_write_byte(addr, data);
				//out_uart('O'); out_uart('K');   // подтверждение
				//} else {
				//out_uart('E'); out_uart('R');
			//}
		//}
		//break;
//
		//case 'R':   // Чтение байта: R <addr_high> <addr_low>
		//{
			//uint8_t ah = in_uart();
			//uint8_t al = in_uart();
			//uint16_t addr = (ah << 8) | al;
			//if (addr < EEPROM_SIZE) {
				//uint8_t data = eeprom_read_byte(addr);
				//out_uart(data);
				//} else {
				//out_uart(0xFF);
			//}
		//}
		//break;
//
		//case 'B':   // Запись блока: B <len> <addr_high> <addr_low> <data0> ... <dataN-1>
		//{
			//uint8_t len = in_uart();
			//uint8_t ah = in_uart();
			//uint8_t al = in_uart();
			//uint16_t addr = (ah << 8) | al;
			//if (addr + len <= EEPROM_SIZE) {
				//for (uint8_t i = 0; i < len; i++) {
					//eeprom_write_byte(addr + i, in_uart());
				//}
				//out_uart('O'); out_uart('K');
				//} else {
				//out_uart(ah); out_uart(al); out_uart(len);//out_uart('E'); out_uart('R');
			//}
		//}
		//break;
//
		//case 'X':   // Выход из режима программирования
		//current_mode = MODE_NORMAL;
		//out_uart('O'); out_uart('K');
		//break;
//
		////default:
		////out_uart('?');   // неизвестная команда
		////break;
	//}
//}




// ---------- Основной цикл передачи данных (режим MODE_NORMAL) ----------
void send_eeprom_data_loop(void) {
	send_all_eeprom_data();

	// Мигание LED (PB0)
	PORTB |= (1 << PB0);
	_delay_ms(500);
	PORTB &= ~(1 << PB0);
	_delay_ms(500);
	_delay_ms(1000);   // пауза ~2 секунды
}


// ---------- Основной цикл ответа по 4 байта из еепром  (режим MODE_NORMAL) ----------
//void send_eeprom_data_4byte(void) {
//	uint8_t cmd = in_uart();   // блокирующее ожидание байта

//if((cmd<=200)|(cmd>=5)){
	       
			//uint8_t ah = in_uart();
			//uint8_t al = in_uart();
		//	uint16_t addr = cmd;
		//	if (addr < EEPROM_SIZE) {
		//		send_4_bytes_from_eeprom(addr);
		//		} else {
		//		out_uart(0xFA);
		//	}
	//	}
	
//}
//----------------------------------
void send_eeprom_data_4byte(void) {
	uint8_t cmd = in_uart();
	
	// Диапазон команд от 5 до 200 (можно настроить)
	if (cmd >= 5 && cmd <= 200) {
		// Переводим команду в номер блока (1..200)
		uint8_t block_num = cmd - 4;  // если cmd=5 -> block_num=1
		
		// Адрес = START_ADDR + (block_num - 1) * 4
		uint16_t addr = 0x06 + (block_num - 1) * 4;
		
		// Проверка границ
		if (addr + 3 < EEPROM_SIZE) {
			// Отправляем 4 байта из EEPROM
			send_4_bytes_from_eeprom(addr);
			} else {
			// Ошибка: выход за границы EEPROM
			out_uart('E');
			out_uart('R');
			out_uart('R');
		}
		} else {
		// Игнорируем неверные команды или отправляем ошибку
		// out_uart('?');
	}
}
//---------------------

//void send_eeprom_data_4byte(void) {
	//uint8_t cmd = in_uart();
	//
	//// Отправляем полученную команду обратно
	//out_uart('C');
	//out_uart(cmd);
	//out_uart('=');
	//
	//if (cmd >= 5 && cmd <= 200) {
		//uint16_t addr = 0x06 + (cmd - 5) * 4;
		//
		//// Отправляем вычисленный адрес
		//out_uart('A');
		//out_uart((addr >> 8) & 0xFF);
		//out_uart(addr & 0xFF);
		//out_uart(':');
		//
		//if (addr + 3 < EEPROM_SIZE) {
			//send_4_bytes_from_eeprom(addr);
			//} else {
			//out_uart('E');
		//}
	//}
//}

//#include <avr/wdt.h>  // Для watchdog

//void wait_for_programming_mode(void) {
	//// Сброс UART
	//UCSRB &= ~(1 << RXEN);
	//_delay_ms(10);
	//UCSRB |= (1 << RXEN);
	//
	//// Очистить буфер чтением UDR
	//while(UCSRA & (1 << RXC)) {
		//(void)UDR;
	//}
	//rx_ready = 0;
	//
	//out_uart('\r');
	//out_uart('\n');
	//out_uart('>');  // Приглашение к вводу
	//
	//uint32_t start_time = 0;
	//while(start_time < 5000) {  // Ждем 5 секунд
		//if (rx_ready) {
			//uint8_t ch = rx_buffer;
			//rx_ready = 0;
			//
			//if (ch == 'P') {
				//current_mode = MODE_PROGRAM;
				//out_uart('P'); out_uart('R'); out_uart('O'); out_uart('G');
				//out_uart('\n');
				//return;
			//}
			//out_uart(ch);  // Эхо
		//}
		//_delay_ms(1);
		//start_time++;
	//}
	//
	//out_uart('\n');
	//out_uart('N');
	//out_uart('O');
	//out_uart('R');
	//out_uart('M');
	//out_uart('\n');
	//current_mode = MODE_NORMAL;
//}





void wait_for_programming_mode(void) {
	//out_uart('W');  // Отладка: начали ожидание
	
	// Ждем команду 'P' в течение 2 секунд
	for (uint16_t i = 0; i < 200; i++) {
		if (rx_ready) {
			uint8_t ch = rx_buffer;
			rx_ready = 0;
			
			// Отладка: показываем принятый символ
			//out_uart('[');
			//out_uart(ch);
			//out_uart(']');
			
			if (ch == 'P') {
				current_mode = MODE_PROGRAM;
				out_uart('P');
				out_uart('R');
				out_uart('O');
				out_uart('G');
				return;
			}
		}
		_delay_ms(10);
	}
	
	//out_uart('T');  // Отладка: таймаут
	current_mode = MODE_NORMAL;
}




 

// ---------- Main ----------
int main(void) {
	init_uart();

	// Настройка портов (LED)
	DDRB |= (1 << PB0);
	DDRD |= (1 << PD5);
	PORTD |= (1 << PD5);   // включить LED на PD5 (индикация питания)

	// Небольшая задержка перед проверкой команды
	_delay_ms(200);
	  sei();                    // 2. ГЛОБАЛЬНОЕ РАЗРЕШЕНИЕ ПРЕРЫВАНИЙ!!!
	  
	  // Отладочное сообщение
	  //out_uart('S');            // Отправляем 'S' - система запущена
	  //out_uart('T');
	  //out_uart('A');
	  //out_uart('R');
	  //out_uart('T');
	  //out_uart('\n');
	wait_for_programming_mode();

	// Основной цикл
	while (1) {
		if (current_mode == MODE_PROGRAM) {
			handle_programming_mode();   // обработка команд W/R/B/X
			} else {
			//send_eeprom_data_loop();     // обычная передача данных из EEPROM
			send_eeprom_data_4byte();
		}
	}
}