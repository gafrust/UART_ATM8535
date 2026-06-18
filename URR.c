#include "uart.h"
#include <util/delay.h>
#include <avr/io.h>

// ---------- Конфигурация EEPROM ----------
#define EEPROM_SIZE     512
#define CMD_ADDR        0
#define CMD_START       1
#define MAX_COMMANDS    100

// ---------- Режимы работы ----------
#define MODE_NORMAL     0
#define MODE_PROGRAM    1

volatile uint8_t current_mode = MODE_NORMAL;

// ---------- Работа с EEPROM ----------
//unsigned char eeprom_read_byte(unsigned int uiAddress) {
	//while (EECR & (1 << EEWE));
	//EEAR = uiAddress;
	//EECR |= (1 << EERE);
	//return EEDR;
//}

unsigned char eeprom_read_byte(unsigned int uiAddress) {
	uint8_t sreg = SREG;  // Сохраняем статус прерываний
	cli();                // Запрещаем прерывания
	
	while (EECR & (1 << EEWE));  // Ждем завершения записи
	
	EEAR = uiAddress;     // Устанавливаем адрес
	EECR |= (1 << EERE);  // Запускаем чтение
	
	SREG = sreg;          // Восстанавливаем прерывания
	return EEDR;          // Возвращаем данные
}



void eeprom_write_byte(unsigned int uiAddress, unsigned char ucData) {
	while (EECR & (1 << EEWE));
	EEAR = uiAddress;
	EEDR = ucData;
	EECR |= (1 << EEMWE);
	EECR |= (1 << EEWE);
}

// ---------- Отправка всех данных из EEPROM ----------
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

// ---------- Отправка 4 байт из EEPROM ----------
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
	// Читаем UDR, пока есть данные
	while (UCSRA & (1 << RXC)) {
		(void)UDR;  // Читаем и игнорируем
	}
	// Сбрасываем флаг готовности
	rx_ready = 0;
}


#define WAIT_BYTE(x) while(!in_uart_nonblock(&(x))) // { _delay_ms(1); }
	
	void handle_programming_mode(void) {
		vkl_rx_485();
		clear_uart_buffer();
		
		uint8_t cmd;
		WAIT_BYTE(cmd);
		
		// Проверяем, что команда валидная
		if(cmd != 'R' && cmd != 'W' && cmd != 'X') {
			// Неизвестная команда - отправляем ERROR
			vkl_tx_485();
			out_uart('E');
			out_uart('R');
			while(!(UCSRA & (1 << TXC)));
			vkl_rx_485();
			clear_uart_buffer();
			return;  // Выходим, не зависаем
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
				// Сначала переключаемся на передачу для ответа
				vkl_tx_485();
				
				// Записываем в EEPROM
				eeprom_write_byte(addr, data);
				
				// Небольшая задержка для завершения записи в EEPROM
				_delay_ms(10);
				
				// Отправляем подтверждение
				out_uart('O');
				out_uart('K');
				
				// Возвращаемся в режим приёма
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
				
				_delay_ms(10);
				
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

		case 'X':
		current_mode = MODE_NORMAL;
		clear_uart_buffer();
		vkl_tx_485();
		_delay_ms(10);
		out_uart('O');
		out_uart('K');
		
		vkl_rx_485();
		break;
		
		default:
		break;
	}
}






// ---------- Основной цикл ответа по 4 байта ----------

void send_eeprom_data_4byte(void) {
	uint8_t cmd;
	static uint16_t cooldown = 0;
	
	// Если таймаут активен
	if (cooldown > 0) {
		cooldown--;
		// ПРИНУДИТЕЛЬНО ЧИСТИМ БУФЕР КАЖДЫЙ РАЗ!
		clear_uart_buffer();
		return;
	}
	
	if (!in_uart_nonblock(&cmd)) {
		return;  // Нет команды - выходим
	}
	
	// Фильтруем мусор
	if (cmd == 'O' || cmd == 'K' || cmd == 'E' || cmd == 'R' || cmd == 0xFF) {
		clear_uart_buffer();
		return;
	}
	
	// Диапазон команд от 5 до 200
	if (cmd >= 5 && cmd <= 200) {
		uint8_t block_num = cmd - 4;
		uint16_t addr = 0x05 + (block_num - 1) * 4;
		
		if (addr + 3 < EEPROM_SIZE) {
			send_4_bytes_from_eeprom(addr);
			// Включаем защиту на 1 секунду
			cooldown = 10;  // 10 * 100мс = 1 секунда
			} else {
			vkl_tx_485();
			out_uart('E');
			out_uart('R');
			vkl_rx_485();
			// Тоже включаем защиту при ошибке
			cooldown = 5;  // 0.5 секунды
		}
		} else {
		// Неизвестная команда - чистим буфер
		clear_uart_buffer();
	}
}

//void send_eeprom_data_4byte(void) {
	//uint8_t cmd;
	//static uint16_t cooldown = 0;  // Статическая переменная для отсчета
	//
	//// Если таймаут активен, уменьшаем счетчик и выходим
	//if (cooldown > 0) {
		//cooldown--;
		//return;
	//}
	//
	//if (!in_uart_nonblock(&cmd)) {
		//return;  // Нет команды - выходим
	//}
	//
	//// Диапазон команд от 5 до 200
	//if (cmd >= 5 && cmd <= 200) {
		//uint8_t block_num = cmd - 4;
		//uint16_t addr = 0x05 + (block_num - 1) * 4;
		//
		//if (addr + 3 < EEPROM_SIZE) {
			//send_4_bytes_from_eeprom(addr);
			//// После успешного ответа включаем защиту на 1 секунду
			//cooldown = 10;  // 10 * 100мс = 1 секунда
			//} else {
			//vkl_tx_485();
			//out_uart('E');
			//out_uart('R');
			//vkl_rx_485();
		//}
	//}
//}

//void send_eeprom_data_4byte(void) {
	//uint8_t cmd;
	//
	//if (!in_uart_nonblock(&cmd)) {
		//return;  // Нет команды - выходим
	//}
	//
	//// Диапазон команд от 5 до 200
	//if (cmd >= 5 && cmd <= 200) {
		//uint8_t block_num = cmd - 4;
		//uint16_t addr = 0x05 + (block_num - 1) * 4;
		//
		//if (addr + 3 < EEPROM_SIZE) {
			//send_4_bytes_from_eeprom(addr);
			//} else {
			//vkl_tx_485();
			//out_uart('E');
			//out_uart('R');
			//vkl_rx_485();
		//}
	//}
//}

// ---------- Ожидание команды перехода в режим программирования ----------
void wait_for_programming_mode(void) {
	uint16_t timeout = 0;
	uint8_t ch;
	
	while (timeout < 6000) {  // ~6 секунд
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

// ---------- Main ----------
int main(void) {
	init_uart();
	sei();  // Разрешаем прерывания
	
	DDRD |= (1 << PD4);
	DDRD |= (1 << PD5);
	SFIOR |= (1 << PUD);
	
	vkl_rx_485();
	
	// Небольшая задержка перед проверкой команды
	_delay_ms(200);
	wait_for_programming_mode();
	
	// Основной цикл
	while (1) {
		if (current_mode == MODE_PROGRAM) {
			handle_programming_mode();
			} else {
		  //  otkl_485();
			//_delay_ms(100);
			vkl_rx_485();
			send_eeprom_data_4byte();
		}
		_delay_ms(100);
	}
}

//int main(void) {
	//init_uart();
	//sei();
	//
	//DDRD |= (1 << PD4);
	//DDRD |= (1 << PD5);
	//SFIOR |= (1 << PUD);
	//
	//// Начинаем в режиме приема
	//vkl_rx_485();
	//
	//_delay_ms(200);
	//wait_for_programming_mode();
	//
	//while (1) {
		//if (current_mode == MODE_PROGRAM) {
			//handle_programming_mode();
			//} else {
			//// Нормальный режим
			//// 1. Отключаем микросхему (чтобы не мешала)
			//otkl_485();
			//_delay_ms(10);
			//
			//// 2. Включаем прием
			//vkl_rx_485();
			//_delay_ms(10);
			//
			//// 3. Проверяем команду
			//send_eeprom_data_4byte();
			//
			//// 4. Небольшая задержка
			//_delay_ms(50);
		//}
	//}
//}










//#include "uart.h"
//#include <util/delay.h>
//#include <avr/io.h>
//
//// ---------- Конфигурация EEPROM ----------
//#define EEPROM_SIZE     512
//#define CMD_ADDR        0       // ячейка 0 хранит счётчик команд (макс. 100)
//#define CMD_START       1       // начало массива команд (4 байта на команду)
//#define MAX_COMMANDS    100
//
//// ---------- Режимы работы ----------
//#define MODE_NORMAL     0       // постоянная передача данных из EEPROM
//#define MODE_PROGRAM    1       // режим программирования (приём/запись)
//
//volatile uint8_t current_mode = MODE_NORMAL;
//
//// ---------- Работа с EEPROM (чтение/запись) ----------
//unsigned char eeprom_read_byte(unsigned int uiAddress) {
	//while (EECR & (1 << EEWE));   // ждём завершения записи
	//EEAR = uiAddress;
	//EECR |= (1 << EERE);
	//return EEDR;
//}
//
////void eeprom_write_byte(unsigned int uiAddress, unsigned char ucData) {
////	while (EECR & (1 << EEWE));   // ждём готовности
////;	EEAR = uiAddress;
////	EEDR = ucData;
////	EECR |= (1 << EEWE);          // запуск записи
////}
//
//void eeprom_write_byte(unsigned int uiAddress, unsigned char ucData) {
	//// Ждём окончания предыдущей записи (если была)
	//while (EECR & (1 << EEWE));
	//// Устанавливаем адрес и данные
	//EEAR = uiAddress;
	//EEDR = ucData;
	//// Последовательность для записи: сначала EEMWE, потом EEWE
	//EECR |= (1 << EEMWE);
	//EECR |= (1 << EEWE);
//}
//
//
//
//
//
//
//// ---------- Отправка всех данных из EEPROM (как было) ----------
//void send_all_eeprom_data(void) {
	//uint8_t cmd_count = eeprom_read_byte(CMD_ADDR);
	//out_uart(cmd_count);
	//_delay_ms(10);
//
	//for (uint8_t i = 0; i < cmd_count; i++) {
		//uint16_t addr = CMD_START + i * 4;
		//for (uint8_t j = 0; j < 4; j++) {
			//out_uart(eeprom_read_byte(addr + j));
			//_delay_ms(10);
		//}
	//}
//}
//
//
////---------------------------------------------------------------------------
//// ---------- Отправка 4 байт из EEPROM начиная с указанного адреса ----------
//void send_4_bytes_from_eeprom(uint16_t start_address) {
	//// Проверка, чтобы адрес не выходил за пределы EEPROM
	//if (start_address+3 >= EEPROM_SIZE) {
		//// Если выход за границы - отправляем ошибку
		//vkl_tx_485();
		//out_uart('E');
		//out_uart('R');
		//vkl_rx_485();
		//return;
	//}
	//
	//// Читаем и отправляем 4 байта последовательно
	////vkl_tx_485();
	//for (uint8_t i = 0; i < 4; i++) {
		//uint8_t data = eeprom_read_byte(start_address + i);
		//vkl_tx_485();
		//out_uart(data);
		//_delay_ms(5); 
		//vkl_rx_485(); // Небольшая задержка между байтами для надежности
	//}
	////vkl_rx_485();
//}
//
////-------------------------------------------------------------------------
//
//
//
////void send_4_bytes_from_eeprom(uint16_t start_address) {
	////// Временный тест: отправляем фиксированные байты
	////out_uart(0xAA);
	////out_uart(0xBB);
	////out_uart(0xCC);
	////out_uart(0xDD);
////}
//
//
//
//// ---------- Обработка команд в режиме программирования ----------
//
//
//void handle_programming_mode(void) {
	//vkl_rx_485();
	//
	//// Неблокирующее ожидание команды с таймаутом
	//uint8_t cmd = 0xFF;
	//uint16_t timeout = 0;
	//while (timeout < 5000) {  // Таймаут ~5000 циклов
		//cmd = in_uart_nonblock();
		//if (cmd != 0xFF) break;
		//_delay_ms(1);
		//timeout++;
	//}
	//
	////if (cmd == 0x58) {
		////// Таймаут - выходим из режима программирования
		////current_mode = MODE_NORMAL;
		////return;
	////}
//
	//switch (cmd) {
		//case 'W': {  // Запись байта: W <addr_high> <addr_low> <data>
			//// Чтение addr_high с таймаутом
			//uint8_t ah = 0xFF;
			//timeout = 0;
			//while (timeout < 1000) {
				//ah = in_uart_nonblock();
				//if (ah != 0xFF) break;
				//_delay_ms(1);
				//timeout++;
			//}
			//if (ah == 0xFF) break;
			//
			//// Чтение addr_low с таймаутом
			//uint8_t al = 0xFF;
			//timeout = 0;
			//while (timeout < 1000) {
				//al = in_uart_nonblock();
				//if (al != 0xFF) break;
				//_delay_ms(1);
				//timeout++;
			//}
			//if (al == 0xFF) break;
			//
			//// Чтение data с таймаутом
			//uint8_t data = 0xFF;
			//timeout = 0;
			//while (timeout < 1000) {
				//data = in_uart_nonblock();
				//if (data != 0xFF) break;
				//_delay_ms(1);
				//timeout++;
			//}
			//if (data == 0xFF) break;
			//
			//uint16_t addr = (ah << 8) | al;
			//if (addr < EEPROM_SIZE) {
				//eeprom_write_byte(addr, data);
				//vkl_tx_485();
				//out_uart('O'); out_uart('K');
				//vkl_rx_485();
				//} else {
				//vkl_tx_485();
				//out_uart('E'); out_uart('R');
				//vkl_rx_485();
			//}
		//}
		//break;
//
		//case 'R': {  // Чтение байта: R <addr_high> <addr_low>
			//// Чтение addr_high
			//uint8_t ah = 0xFF;
			//timeout = 0;
			//while (timeout < 1000) {
				//ah = in_uart_nonblock();
				//if (ah != 0xFF) break;
				//_delay_ms(1);
				//timeout++;
			//}
			//if (ah == 0xFF) break;
			//
			//// Чтение addr_low
			//uint8_t al = 0xFF;
			//timeout = 0;
			//while (timeout < 1000) {
				//al = in_uart_nonblock();
				//if (al != 0xFF) break;
				//_delay_ms(1);
				//timeout++;
			//}
			//if (al == 0xFF) break;
			//
			//uint16_t addr = (ah << 8) | al;
			//if (addr < EEPROM_SIZE) {
				//uint8_t data = eeprom_read_byte(addr);
				//vkl_tx_485();
				//out_uart(data);
				//vkl_rx_485();
				//} else {
				//vkl_tx_485();
				//out_uart(0xFF);
				//vkl_rx_485();
			//}
		//}
		//break;
//
		//case 'X': {  // Выход из режима программирования
			//current_mode = MODE_NORMAL;
			//vkl_tx_485();
			//out_uart('O'); out_uart('K');
			//vkl_rx_485();
		//}
		//break;
		//
		//default:
		//// Неизвестная команда - игнорируем
		//break;
	//}
//}
//
//
//
////void handle_programming_mode(void) {
	////vkl_rx_485();
	////uint8_t cmd = in_uart_nonblock();   // блокирующее ожидание байта
////
	////switch (cmd) {
		////case 'W':   // Запись байта: W <addr_high> <addr_low> <data>
		////{
			////uint8_t ah = in_uart();
			////uint8_t al = in_uart();
			////uint8_t data = in_uart();
			////uint16_t addr = (ah << 8) | al;
			////if (addr < EEPROM_SIZE) {
				////eeprom_write_byte(addr, data);
				////vkl_tx_485();
				////out_uart('O'); out_uart('K');  
				////vkl_rx_485(); // подтверждение
				////} else {
					////vkl_tx_485();
				////out_uart('E'); out_uart('R');
				////vkl_rx_485();
			////}
		////}
		////break;
////
		////case 'R':   // Чтение байта: R <addr_high> <addr_low>
		////{
			////uint8_t ah = in_uart();
			////uint8_t al = in_uart();
			////uint16_t addr = (ah << 8) | al;
			////if (addr < EEPROM_SIZE) {
				////uint8_t data = eeprom_read_byte(addr);
				////vkl_tx_485();
				////out_uart(data);
				////vkl_rx_485();
				////} else {
				////vkl_tx_485();
				////out_uart(0xFF);
				////vkl_rx_485();
			////}
		////}
		////break;
////
		////
////
		////case 'X':   // Выход из режима программирования
		////current_mode = MODE_NORMAL;
		////vkl_tx_485();
		////out_uart('O'); out_uart('K');
		////vkl_rx_485();
		////break;
////
		//////default:
		//////out_uart('?');   // неизвестная команда
		//////break;
	////}
////}
//
//// ---------- Основной цикл передачи данных (режим MODE_NORMAL) ----------
//void send_eeprom_data_loop(void) {
	//send_all_eeprom_data();
//
	//// Мигание LED (PB0)
	//PORTB |= (1 << PB0);
	//_delay_ms(500);
	//PORTB &= ~(1 << PB0);
	//_delay_ms(500);
	//_delay_ms(1000);   // пауза ~2 секунды
//}
//
//
//// ---------- Основной цикл ответа по 4 байта из еепром  (режим MODE_NORMAL) ----------
////void send_eeprom_data_4byte(void) {
////	uint8_t cmd = in_uart();   // блокирующее ожидание байта
//
////if((cmd<=200)|(cmd>=5)){
	       //
			////uint8_t ah = in_uart();
			////uint8_t al = in_uart();
		////	uint16_t addr = cmd;
		////	if (addr < EEPROM_SIZE) {
		////		send_4_bytes_from_eeprom(addr);
		////		} else {
		////		out_uart(0xFA);
		////	}
	////	}
	//
////}
////----------------------------------
//
//
//
//
//
//
//
//
//void send_eeprom_data_4byte(void) {
	//uint8_t cmd = in_uart_nonblock();
	//
	//if (cmd == 0xFF) {
		//return;  // Нет команды - выходим
	//}
	//
	//// Диапазон команд от 5 до 200
	//if (cmd >= 5 && cmd <= 200) {
		//uint8_t block_num = cmd - 4;
		//uint16_t addr = 0x05 + (block_num - 1) * 4;
		//
		//if (addr + 3 < EEPROM_SIZE) {
			//send_4_bytes_from_eeprom(addr);
			//} else {
			//vkl_tx_485();
			//out_uart('E');
			//out_uart('R');
			//vkl_rx_485();
			////out_uart('R');
		//}
	//}
	//// Игнорируем неверные команды
//}
//
//
//
//
//
//
//
//
//
//
//
//
////void send_eeprom_data_4byte(void) {
	////uint8_t cmd = in_uart();
	////
	////// Диапазон команд от 5 до 200 (можно настроить)
	////if (cmd >= 5 && cmd <= 200) {
		////// Переводим команду в номер блока (1..200)
		////uint8_t block_num = cmd - 4;  // если cmd=5 -> block_num=1
		////
		////// Адрес = START_ADDR + (block_num - 1) * 4
		////uint16_t addr = 0x05 + (block_num - 1) * 4;
		////
		////// Проверка границ
		////if (addr + 3 < EEPROM_SIZE) {
			////// Отправляем 4 байта из EEPROM
			////send_4_bytes_from_eeprom(addr);
			////} else {
			////// Ошибка: выход за границы EEPROM
			////out_uart('E');
			////out_uart('R');
			////out_uart('R');
		////}
		////} else {
		////// Игнорируем неверные команды или отправляем ошибку
		////// out_uart('?');
	////}
////}
////---------------------
//
////void send_eeprom_data_4byte(void) {
	////uint8_t cmd = in_uart();
	////
	////// Отправляем полученную команду обратно
	////out_uart('C');
	////out_uart(cmd);
	////out_uart('=');
	////
	////if (cmd >= 5 && cmd <= 200) {
		////uint16_t addr = 0x06 + (cmd - 5) * 4;
		////
		////// Отправляем вычисленный адрес
		////out_uart('A');
		////out_uart((addr >> 8) & 0xFF);
		////out_uart(addr & 0xFF);
		////out_uart(':');
		////
		////if (addr + 3 < EEPROM_SIZE) {
			////send_4_bytes_from_eeprom(addr);
			////} else {
			////out_uart('E');
		////}
	////}
////}
//
//
//
//
////// ---------- Ожидание команды перехода в режим программирования ----------
////void wait_for_programming_mode(void) {
	////// Ждём 1 секунду, если пришёл 'P' — переходим в режим программирования
	////for (uint8_t i = 0; i < 60; i++) {   // 10 раз по 100 мс = 1 сек
		////if (UCSRA & (1 << RXC)) {        // есть данные в буфере
			////uint8_t ch = UDR;
			////if (ch == 'P') {
				////current_mode = MODE_PROGRAM;
				////vkl_tx_485();
				////out_uart('P');
				////out_uart('R');
				////out_uart('O');
			    ////out_uart('G'); // подтверждение
				////vkl_rx_485();
				////return;
			////}
		////}
		////_delay_ms(100);
	////}
////}
//
//void wait_for_programming_mode(void) {
	//uint16_t timeout = 0;
	//
	//while (timeout < 6000) {  // ~6000 мс таймаут
		//uint8_t ch = in_uart_nonblock();
		//if (ch != 0xFF && ch == 'P') {
			//current_mode = MODE_PROGRAM;
			//vkl_tx_485();
			//out_uart('P');
			//out_uart('R');
			////out_uart('O');
			////out_uart('G');
			//vkl_rx_485();
			//return;
		//}
		//_delay_ms(1);
		//timeout++;
	//}
//}
//
//
//
//
//
//// ---------- Main ----------
//int main(void) {
	//init_uart();
	//sei();
	//DDRD |= (1 << PD4);
	//DDRD |= (1 << PD5);
	//SFIOR |= (1 << PUD);
	////SFIOR &= ~(1 << PUD);
    ////vkl_tx_485();
	////out_uart(0x53);
	////out_uart(0x54);
	////out_uart(0x41);
	////out_uart(0x52);
	////out_uart(0x54);
    //vkl_rx_485();
//
	//// Небольшая задержка перед проверкой команды
	//_delay_ms(200);
	//wait_for_programming_mode();
//
	//// Основной цикл
	//while (1) {
		//if (current_mode == MODE_PROGRAM) {
			//handle_programming_mode();   // обработка команд W/R/B/X
			//} else {
			////send_eeprom_data_loop();     // обычная передача данных из EEPROM
			//vkl_rx_485();
		    //send_eeprom_data_4byte();
			//vkl_rx_485();
			////vkl_tx_485();
			////out_uart(0x55);
			////out_uart('P');
		//} _delay_ms(1000);
	//}
//}