#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

// Конфигурация UART
#define UART_DEVICE     "/dev/ttyUSB0"
#define BAUD_RATE       B115200
#define START_ADDR      0x06        // адрес начала записи
#define READ_START      0x05        // адрес начала чтения по умолчанию

int uart_fd = -1;

// Прототипы
int uart_init(const char* device, speed_t baud);
int send_byte(uint8_t data);
int recv_byte(uint8_t* data, int timeout_ms);
int expect_ok(int timeout_ms);
int enter_programming_mode(void);
int write_byte(uint16_t addr, uint8_t data);
int read_byte(uint16_t addr, uint8_t *data);
int parse_hex_line(const char* line, uint8_t bytes[4]);

// ------------------------------------------------------------------
// Инициализация UART (четность even – как при записи)
// ------------------------------------------------------------------
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
    tty.c_cflag |= PARENB;   // even parity
    tty.c_cflag &= ~PARODD;
    tty.c_cflag &= ~CSTOPB;  // 1 стоп-бит
    tty.c_cflag &= ~CRTSCTS;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(INLCR | ICRNL | IGNCR);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;    // 1 секунда таймаут

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Ошибка tcsetattr");
        close(fd);
        return -1;
    }

    return fd;
}

// ------------------------------------------------------------------
// Отправка байта
// ------------------------------------------------------------------
int send_byte(uint8_t data) {
    ssize_t n = write(uart_fd, &data, 1);
    if (n != 1) {
        printf("Ошибка отправки байта 0x%02X\n", data);
        return -1;
    }
    return 0;
}

// ------------------------------------------------------------------
// Приём байта с таймаутом
// ------------------------------------------------------------------
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
        return 0;   // таймаут
    }

    ssize_t n = read(uart_fd, data, 1);
    if (n != 1) return -1;
    return 1;
}

// ------------------------------------------------------------------
// Ожидание ответа "OK"
// ------------------------------------------------------------------
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
        printf("Таймаут ожидания 'K'\n");
        return 0;
    }
    if (byte2 != 'K') {
        printf("Ожидал 'K', получил 0x%02X\n", byte2);
        return 0;
    }
    tcflush(uart_fd, TCIFLUSH);
    return 1;
}

// ------------------------------------------------------------------
// Переключение в режим программирования
// ------------------------------------------------------------------
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

// ------------------------------------------------------------------
// Запись одного байта (работает)
// ------------------------------------------------------------------
int write_byte(uint16_t addr, uint8_t data) {
    if (send_byte('W') < 0) return 0;
    if (send_byte((addr >> 8) & 0xFF) < 0) return 0;
    if (send_byte(addr & 0xFF) < 0) return 0;
    if (send_byte(data) < 0) return 0;

    if (expect_ok(1000)) {
        usleep(100000);  // пауза после OK
        return 1;
    }
    return 0;
}

// ------------------------------------------------------------------
// Чтение одного байта (побайтовое, с проверенными задержками)
// ------------------------------------------------------------------
int read_byte(uint16_t addr, uint8_t *data) {
    tcflush(uart_fd, TCIFLUSH);

    if (send_byte('R') < 0) return 0;
    if (send_byte((addr >> 8) & 0xFF) < 0) return 0;
    if (send_byte(addr & 0xFF) < 0) return 0;

    usleep(50000);  // 50 мс на подготовку

    uint8_t val;
    int ret = recv_byte(&val, 2000);
    if (ret != 1) {
        printf("Ошибка чтения адреса 0x%04X (таймаут)\n", addr);
        return 0;
    }

    *data = val;

    // Очистка буфера и пауза перед следующей командой
    tcflush(uart_fd, TCIFLUSH);
    usleep(100000);  // 100 мс

    return 1;
}

// ------------------------------------------------------------------
// Парсинг строки из файла
// ------------------------------------------------------------------
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

// ------------------------------------------------------------------
// MAIN
// ------------------------------------------------------------------
int main(int argc, char* argv[]) {
    printf("========================================\n");
    printf("=== EEPROM программатор (запись/чтение) ===\n");
    printf("=== Скорость UART: 115200 бод ===\n");
    printf("========================================\n\n");

    printf("Открытие UART: %s\n", UART_DEVICE);
    uart_fd = uart_init(UART_DEVICE, BAUD_RATE);
    if (uart_fd < 0) return 1;
    printf("✓ UART открыт\n\n");

    if (enter_programming_mode() < 0) {
        close(uart_fd);
        return 1;
    }

    // --- РЕЖИМ ЧТЕНИЯ (ключ -r) ---
    if (argc > 1 && strcmp(argv[1], "-r") == 0) {
        uint16_t start_addr = READ_START;   // по умолчанию 0x05
        uint16_t count = 400;                // по умолчанию 16 байт

        if (argc >= 3) start_addr = (uint16_t)strtol(argv[2], NULL, 0);
        if (argc >= 4) count = (uint16_t)strtol(argv[3], NULL, 0);

        printf("\n=== Режим ЧТЕНИЯ ===\n");
        printf("Адрес начала: 0x%04X, количество байт: %d\n", start_addr, count);
        printf("Чтение...\n\n");

        int success = 0;
        for (uint16_t addr = start_addr; addr < start_addr + count; addr++) {
            uint8_t val;
            if (read_byte(addr, &val)) {
                printf("0x%04X: 0x%02X\n", addr, val);
                success++;
            } else {
                printf("0x%04X: ошибка чтения\n", addr);
                break;
            }
        }

        printf("\nПрочитано байт: %d\n", success);
        close(uart_fd);
        return 0;
    }

    // --- РЕЖИМ ЗАПИСИ (по умолчанию) ---
    printf("\n=== Режим ЗАПИСИ ===\n");

    FILE* input_file = fopen("repacked_4bytes.txt", "r");
    if (!input_file) {
        perror("✗ Ошибка открытия файла repacked_4bytes.txt");
        close(uart_fd);
        return 1;
    }
    printf("✓ Файл repacked_4bytes.txt открыт\n");

    char line[128];
    int line_num = 0;
    int success_count = 0;
    int fail_count = 0;
    uint16_t current_addr = START_ADDR;

    printf("\nСтартовый адрес: 0x%04X\n", START_ADDR);
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
        usleep(10000); // 10 мс между строками
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
