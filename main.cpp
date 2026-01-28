#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>

typedef struct {
    const char* type_str;
    uint32_t id;
    float pressure_bar;
    int temperature_c;
    float voltage_v;
    int fw_version;
    int rssi;
    uint8_t raw_packet[26];
    int total;
} DeviceData;

volatile sig_atomic_t stop_flag = 0;
DeviceData data1 = {0};

void handle_signal(int sig);
int setup_serial_port(const char* port);
const char* get_device_type_str(uint8_t type_byte);
void parse_packet(const uint8_t* packet, DeviceData* data);
void print_device_data_oneline(int packet_num, const DeviceData* data);

void handle_signal(int sig) {
    stop_flag = 1;
}

int setup_serial_port(const char* port) {
    int serial_port = open(port, O_RDWR | O_NOCTTY);

    if (serial_port < 0) {
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    tcgetattr(serial_port, &tty);

    // Настройка скорости
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    // Настройка битов управления
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    // Настройка локальных флагов
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;

    // Настройка управляющих символов
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    // Применение настроек
    tcsetattr(serial_port, TCSANOW, &tty);
    tcflush(serial_port, TCIOFLUSH);

    return serial_port;
}

const char* get_device_type_str(uint8_t type_byte) {
    if (type_byte == 0xF0) {
        return "ДАТЧИК";
    } else if (type_byte == 0xF1) {
        return "РЕПИТЕР";
    } else {
        return "НЕИЗВЕСТНО";
    }
}

void parse_packet(const uint8_t* packet, DeviceData* data) {
    // Копируем сырые данные
    memcpy(data->raw_packet, packet, 26);

    // Тип устройства
    data->type_str = get_device_type_str(packet[2]);

    // ID устройства
    data->id = ((uint32_t)packet[6] << 24) |
               ((uint32_t)packet[5] << 16) |
               ((uint32_t)packet[4] << 8) |
               packet[3];

    // Давление
    uint16_t pressure = ((uint16_t)packet[8] << 8) | packet[7];
    data->pressure_bar = 2750.0f * (pressure - 1) / 100000.0f;

    // Температура
    int8_t temp_raw = packet[14];
    data->temperature_c = (int)temp_raw - 55;

    // Напряжение
    uint8_t voltage_raw = packet[13];
    data->voltage_v = voltage_raw * 0.01512f;

    // Версия прошивки
    uint8_t fw_unsigned = packet[19];
    int8_t fw_signed = (int8_t)fw_unsigned;
    data->fw_version = abs((int)fw_signed);

    // RSSI
    data->rssi = -(int8_t)packet[24];
}

void print_device_data_oneline(int packet_num, const DeviceData* data) {
    printf("[%d] %s | ID: 0x%08X | ",
           packet_num, data->type_str, data->id);

    printf("Версия: %d | ", data->fw_version);
    printf("Давление: %.3f бар | ", data->pressure_bar);
    printf("Температура: %d°C | ", data->temperature_c);
    printf("Напряжение: %.3fВ | ", data->voltage_v);
    printf("RSSI: %d | ", data->rssi);

    // Байты ID как в пакете
    printf("ID байты: 0x%02X 0x%02X 0x%02X 0x%02X",
           data->raw_packet[3], data->raw_packet[4],
           data->raw_packet[5], data->raw_packet[6]);

    printf("\n");
}

int main() {
    const char* port = "/dev/ttyACM0";

    // Настройка обработчика Ctrl+C
    signal(SIGINT, handle_signal);

    // Настройка последовательного порта
    int serial_port = setup_serial_port(port);
    if (serial_port < 0) {
        fprintf(stderr, "Ошибка открытия порта %s\n", port);
        return 1;
    }

    printf("Прием всех устройств... Ctrl+C для выхода\n");
    printf("===========================================\n");

    uint8_t packet[26];
    int packet_idx = 0;

    // Главный цикл приема данных
    while (!stop_flag) {
        int n = read(serial_port, &packet[packet_idx], 1);

        if (n > 0) {
            packet_idx++;

            if (packet_idx == 26) {
                // Создаем структуру для данных
                DeviceData data;

                // Парсим пакет
                parse_packet(packet, &data);

                // Выводим данные В ОДНОЙ СТРОКЕ
                print_device_data_oneline(data1.total, &data);

                // Сбрасываем индекс для следующего пакета
                packet_idx = 0;
            }
        }

        usleep(1000);
    }

    close(serial_port);

    return 0;
}