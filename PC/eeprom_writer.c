#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>   // для uint8_t, uint16_t

// Конфигурация UART
#define UART_DEVICE     "/dev/ttyUSB0"  // измените под ваш порт
#define BAUD_RATE       B115200
#define TIMEOUT_MS      2000

// Адрес начала записи в EEPROM
#define START_ADDR      0x06

// Глобальный дескриптор UART
int uart_fd = -1;

// Прототипы функций
int uart_init(const char* device, speed_t baud);
int send_byte(uint8_t data);
int recv_byte(uint8_t* data, int timeout_ms);
int expect_ok(int timeout_ms);
int enter_programming_mode(void);
int write_byte(uint16_t addr, uint8_t data);
int parse_hex_line(const char* line, uint8_t bytes[4]);

// Инициализация UART
int uart_init(const char* device, speed_t baud) {
    int fd = open(device, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("Ошибка открытия UART");
        return -1;
    }
    
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(fd, &tty) != 0) {
        perror("Ошибка tcgetattr");
        close(fd);
        return -1;
    }
    
    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);
    
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(INLCR | ICRNL | IGNCR);
    tty.c_oflag &= ~OPOST;
    
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;  // 1 секунда таймаут
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Ошибка tcsetattr");
        close(fd);
        return -1;
    }
    
    return fd;
}

// Отправка байта
int send_byte(uint8_t data) {
    ssize_t n = write(uart_fd, &data, 1);
    if (n != 1) {
        printf("Ошибка отправки байта 0x%02X\n", data);
        return -1;
    }
    return 0;
}

// Приём байта с таймаутом
int recv_byte(uint8_t* data, int timeout_ms) {
    fd_set set;
    struct timeval tv;
    
    FD_ZERO(&set);
    FD_SET(uart_fd, &set);
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int rv = select(uart_fd + 1, &set, NULL, NULL, &tv);
    if (rv == -1) {
        perror("select");
        return -1;
    } else if (rv == 0) {
        return 0;  // таймаут
    }
    
    ssize_t n = read(uart_fd, data, 1);
    if (n != 1) {
        return -1;
    }
    return 1;
}

// Ожидание ответа "OK" (два байта)
int expect_ok(int timeout_ms) {
    uint8_t byte1, byte2;
    
    if (recv_byte(&byte1, timeout_ms) != 1) {
        printf("Таймаут ожидания 'O'\n");
        return 0;
    }
    if (byte1 != 'O') {
        printf("Ожидал 'O', получил 0x%02X\n", byte1);
        return 0;
    }
    if (recv_byte(&byte2, timeout_ms) != 1) {
        printf("Таймаут ожидания 'K' после 'O'\n");
        return 0;
    }
    if (byte2 != 'K') {
        printf("Ожидал 'K', получил 0x%02X\n", byte2);
        return 0;
    }
    return 1;
}

// Переключение в режим программирования
int enter_programming_mode(void) {
    printf("\n=== Переключение в режим программирования ===\n");
    tcflush(uart_fd, TCIFLUSH);
    
    printf("Отправка: 'P' (0x50)\n");
    if (send_byte('P') < 0) return -1;
    
    printf("Ожидание ответа 'PROG'...\n");
    uint8_t buf[4];
    int received = 0;
    clock_t start = clock();
    int timeout_ms = 3000;
    
    while ((clock() - start) * 1000 / CLOCKS_PER_SEC < timeout_ms) {
        uint8_t ch;
        int ret = recv_byte(&ch, 100);
        if (ret == 1) {
            if (received < 4) {
                buf[received] = ch;
                received++;
            }
            if (received >= 4) {
                if (buf[0] == 'P' && buf[1] == 'R' && buf[2] == 'O' && buf[3] == 'G') {
                    printf("✓ Получен ответ: PROG\n");
                    return 0;
                }
                memmove(buf, buf + 1, 3);
                received--;
            }
        }
    }
    printf("✗ Ошибка: не получен ответ PROG\n");
    return -1;
}

// Запись одного байта по адресу командой W
int write_byte(uint16_t addr, uint8_t data) {
    if (send_byte('W') < 0) return 0;
    if (send_byte((addr >> 8) & 0xFF) < 0) return 0;
    if (send_byte(addr & 0xFF) < 0) return 0;
    if (send_byte(data) < 0) return 0;
    
    if (expect_ok(1000)) return 1;
    else return 0;
}

// Парсинг строки из файла (формат: "13 52 9A CD")
int parse_hex_line(const char* line, uint8_t bytes[4]) {
    char line_copy[64];
    strcpy(line_copy, line);
    
    char* token = strtok(line_copy, " \t\n\r");
    for (int i = 0; i < 4; i++) {
        if (token == NULL) return -1;
        bytes[i] = (uint8_t)strtol(token, NULL, 16);
        token = strtok(NULL, " \t\n\r");
    }
    return 0;
}

int main(int argc, char* argv[]) {
    FILE* input_file = NULL;
    char line[128];
    int line_num = 0;
    int success_count = 0;
    int fail_count = 0;
    uint16_t current_addr = START_ADDR;
    
    printf("========================================\n");
    printf("=== EEPROM программатор (побайтная запись) ===\n");
    printf("=== Скорость UART: 115200 бод ===\n");
    printf("========================================\n\n");
    
    printf("Открытие UART: %s\n", UART_DEVICE);
    uart_fd = uart_init(UART_DEVICE, BAUD_RATE);
    if (uart_fd < 0) return 1;
    printf("✓ UART открыт на скорости 115200 бод\n\n");
    
    input_file = fopen("repacked_4bytes.txt", "r");
    if (!input_file) {
        perror("✗ Ошибка открытия файла repacked_4bytes.txt");
        close(uart_fd);
        return 1;
    }
    printf("✓ Файл repacked_4bytes.txt открыт\n");
    
    if (enter_programming_mode() < 0) {
        fclose(input_file);
        close(uart_fd);
        return 1;
    }
    
    printf("\n=== Начинаем запись в EEPROM ===\n");
    printf("Стартовый адрес: 0x%04X\n", START_ADDR);
    printf("Формат: команда W + адрес + байт, ожидание OK\n\n");
    
    while (fgets(line, sizeof(line), input_file)) {
        if (strlen(line) < 3) continue;
        
        uint8_t bytes[4];
        if (parse_hex_line(line, bytes) < 0) {
            printf("Строка %d: ошибка парсинга\n", line_num + 1);
            fail_count++;
            line_num++;
            continue;
        }
        
        printf("Строка %3d: [%02X %02X %02X %02X]\n", 
               line_num + 1, bytes[0], bytes[1], bytes[2], bytes[3]);
        
        int line_ok = 1;
        for (int i = 0; i < 4; i++) {
            printf("  байт %d (адрес 0x%04X = 0x%02X): ", 
                   i, current_addr + i, bytes[i]);
            fflush(stdout);
            
            if (write_byte(current_addr + i, bytes[i])) {
                printf("OK ✓\n");
                success_count++;
            } else {
                printf("FAIL ✗\n");
                line_ok = 0;
                fail_count++;
                break;
            }
        }
        
        if (line_ok) {
            printf("  -> строка OK\n");
        } else {
            printf("  -> Ошибка на строке %d, останов\n", line_num + 1);
            break;
        }
        
        current_addr += 4;
        line_num++;
        usleep(10000);
    }
    
    printf("\n========================================\n");
    printf("=== РЕЗУЛЬТАТЫ ===\n");
    printf("Обработано строк: %d\n", line_num);
    printf("Успешных записей байт: %d\n", success_count);
    printf("Ошибок: %d\n", fail_count);
    printf("Последний адрес: 0x%04X\n", current_addr);
    printf("========================================\n");
    
    fclose(input_file);
    close(uart_fd);
    return 0;
}
