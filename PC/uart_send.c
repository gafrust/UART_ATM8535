#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdint.h>
#include <errno.h>

#define DEFAULT_PORT     "/dev/ttyUSB0"
#define DEFAULT_BAUDRATE B115200
#define MAX_CHUNK        256   // максимальный размер одного пакета (буфер МК)

// Настройка UART
int setup_uart(const char *device, speed_t baud_rate) {
    int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, baud_rate);
    cfsetispeed(&tty, baud_rate);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8 бит данных
    tty.c_iflag &= ~IGNBRK;                     // отключить break
    tty.c_lflag = 0;                            // неканонический режим
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 5;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);     // отключить software flow control
    tty.c_cflag |= (CLOCAL | CREAD);            // игнорировать линии управления
    tty.c_cflag &= ~(PARENB | PARODD);          // без чётности
    tty.c_cflag &= ~CSTOPB;                     // 1 стоп-бит
    tty.c_cflag &= ~CRTSCTS;                    // отключить аппаратный flow control

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(fd);
        return -1;
    }
    return fd;
}

// Отправка буфера (может быть любого размера, но мы ограничим MAX_CHUNK)
int send_data(int fd, const uint8_t *data, size_t len) {
    size_t sent_total = 0;
    while (sent_total < len) {
        size_t chunk = len - sent_total;
        if (chunk > MAX_CHUNK) chunk = MAX_CHUNK;
        ssize_t n = write(fd, data + sent_total, chunk);
        if (n != (ssize_t)chunk) {
            perror("write");
            return -1;
        }
        sent_total += n;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const char *port = DEFAULT_PORT;
    speed_t baud = DEFAULT_BAUDRATE;
    const char *filename = "repacked_4bytes.txt";
    size_t total_bytes = 400;   // по умолчанию отправляем 400 байт

    // Разбор аргументов: ./prog [порт] [скорость] [размер] [файл]
    if (argc > 1) port = argv[1];
    if (argc > 2) {
        int br = atoi(argv[2]);
        switch (br) {
            case 9600:   baud = B9600; break;
            case 19200:  baud = B19200; break;
            case 38400:  baud = B38400; break;
            case 115200: baud = B115200; break;
            default: fprintf(stderr, "Скорость %d не поддерживается, использую 115200\n", br);
        }
    }
    if (argc > 3) {
        total_bytes = atoi(argv[3]);
        if (total_bytes == 0) total_bytes = 400;
    }
    if (argc > 4) filename = argv[4];

    printf("Параметры: порт=%s, скорость=%d, отправляем %zu байт, файл=%s\n",
           port, (baud==B9600?9600:baud==B19200?19200:baud==B38400?38400:115200),
           total_bytes, filename);

    // Открываем UART
    int uart_fd = setup_uart(port, baud);
    if (uart_fd < 0) return 1;

    // Открываем файл
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        close(uart_fd);
        return 1;
    }

    // Выделяем буфер для чтения нужного количества байт
    uint8_t *buffer = (uint8_t*)malloc(total_bytes);
    if (!buffer) {
        fprintf(stderr, "Не удалось выделить память\n");
        fclose(f);
        close(uart_fd);
        return 1;
    }

    // Читаем из файла ровно total_bytes байт (или меньше, если файл короче)
    size_t actually_read = fread(buffer, 1, total_bytes, f);
    fclose(f);

    if (actually_read == 0) {
        fprintf(stderr, "Файл пуст или не удалось прочитать данные\n");
        free(buffer);
        close(uart_fd);
        return 1;
    }

    printf("Прочитано %zu байт из файла (запрошено %zu)\n", actually_read, total_bytes);

    // Отправляем прочитанные данные по UART
    if (send_data(uart_fd, buffer, actually_read) != 0) {
        fprintf(stderr, "Ошибка при отправке\n");
        free(buffer);
        close(uart_fd);
        return 1;
    }

    printf("Успешно отправлено %zu байт (пакетами не более %d байт)\n", actually_read, MAX_CHUNK);

    free(buffer);
    close(uart_fd);
    return 0;
}