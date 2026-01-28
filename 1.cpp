#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>

volatile sig_atomic_t stop_flag = 0;

void handle_signal(int sig) {
    stop_flag = 1;
}

int main() {
    const char* port = "/dev/ttyACM0";

    signal(SIGINT, handle_signal);

    int serial_port = open(port, O_RDWR | O_NOCTTY);

    if (serial_port < 0) {
        printf("Ошибка открытия порта %s\n", port);
        return 1;
    }

    // Настройка порта
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    tcgetattr(serial_port, &tty);

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    tcsetattr(serial_port, TCSANOW, &tty);
    tcflush(serial_port, TCIOFLUSH);

    printf("Прием всех устройств... Ctrl+C для выхода\n");
    printf("===========================================\n");

    unsigned char packet[26];
    int packet_idx = 0;
    int packet_count = 0;

    int sensor_count = 0;
    int repeater_count = 0;
    int unknown_count = 0;

    while (!stop_flag) {
        int n = read(serial_port, &packet[packet_idx], 1);

        if (n > 0) {
            packet_idx++;

            if (packet_idx == 26) {
                packet_count++;

                // Тип устройства
                uint8_t device_type = packet[2];
                const char* device_type_str;

                if (device_type == 0xF0) {
                    device_type_str = "ДАТЧИК";
                    sensor_count++;
                } else if (device_type == 0xF1) {
                    device_type_str = "РЕПИТЕР";
                    repeater_count++;
                } else {
                    device_type_str = "НЕИЗВЕСТНО";
                    unknown_count++;
                }

                // ID в правильном порядке (0x7857E03F)
                uint32_t device_id = ((uint32_t)packet[6] << 24) |
                                     ((uint32_t)packet[5] << 16) |
                                     ((uint32_t)packet[4] << 8) |
                                     packet[3];

                // Давление
                uint16_t pressure = ((uint16_t)packet[8] << 8) | packet[7];
                float pressure_bar = 2750.0f * (pressure - 1) / 100000.0f;

                // Температура
                int8_t temp_raw = packet[14];
                int temp_celsius = (int)temp_raw - 55;

                // Напряжение
                uint8_t voltage_raw = packet[13];
                float voltage_volts = voltage_raw * 0.01512f;

                // Версия прошивки
                uint8_t fw_unsigned = packet[18];
                int8_t fw_signed = (int8_t)fw_unsigned;
                int fw_abs = abs((int)fw_signed);

                // RSSI
                int rssi_positive = -(int8_t)packet[24];

                // Выводим информацию
                printf("[Пакет %d] %s | ID: 0x%08X\n",
                       packet_count, device_type_str, device_id);

                printf("  Версия: %d | Давление: %.3f бар\n",
                       fw_abs, pressure_bar);

                printf("  Температура: %d°C | Напряжение: %.3fВ\n",
                       temp_celsius, voltage_volts);

                printf("  RSSI: %d\n", rssi_positive);

                // Показываем байты как в пакете
                printf("  Байты ID в пакете: 0x%02X 0x%02X 0x%02X 0x%02X\n",
                       packet[3], packet[4], packet[5], packet[6]);

                printf("  HEX: ");
                for (int i = 0; i < 26; i++) {
                    printf("%02X ", packet[i]);
                }
                printf("\n");

                printf("--------------------------------------------\n");

                packet_idx = 0;
            }
        }

        usleep(1000);
    }

    // Статистика
    printf("\n=== СТАТИСТИКА ===\n");
    printf("Всего пакетов: %d\n", packet_count);
    printf("Датчиков: %d\n", sensor_count);
    printf("Репитеров: %d\n", repeater_count);
    printf("Неизвестных: %d\n", unknown_count);

    close(serial_port);
    return 0;
}