#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Платформозависимые заголовки
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#define PORT_PREFIX "\\\\.\\"
#else
#include <unistd.h>
    #include <fcntl.h>
    #include <termios.h>
    #include <errno.h>
    #include <signal.h>
    #define PORT_PREFIX "/dev/"
#endif

typedef struct {
    const char* type_str;
    uint32_t id;
    float pressure_bar;
    int temperature_c;
    float voltage_v;
    int fw_version;
    int rssi;
    uint8_t raw_packet[26];
} DeviceData;

volatile int stop_flag = 0;
int serial_port;
char full_port[50];
struct termios tty;
int _port;

// Прототипы функций
void handle_signal(int sig);
#ifdef _WIN32
HANDLE setup_serial_port(const char* port);
BOOL win32_read_byte(HANDLE hSerial, uint8_t* byte);
#else
int setup_serial_port(const char* port);
#endif
const char* get_device_type_str(uint8_t type_byte);
void parse_packet(const uint8_t* packet, DeviceData* data);
void print_device_data_oneline(int packet_num, const DeviceData* data);

// Обработчик сигналов (только для Linux)
#ifndef _WIN32
void handle_signal(int sig) {
    stop_flag = 1;
}
#endif

#ifdef _WIN32
// Настройка COM-порта для Windows
HANDLE setup_serial_port(const char* port) {
    char full_port[50];
    snprintf(full_port, sizeof(full_port), "%s%s", PORT_PREFIX, port);

    HANDLE hSerial = CreateFileA(
            full_port,
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
    );

    if (hSerial == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    // Настройка параметров порта
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    // Настройка таймаутов
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    return hSerial;
}

// Чтение одного байта для Windows
BOOL win32_read_byte(HANDLE hSerial, uint8_t* byte) {
    DWORD bytes_read;
    return ReadFile(hSerial, byte, 1, &bytes_read, NULL) && bytes_read == 1;
}
#else
// Настройка последовательного порта для Linux
int setup_serial_port(const char* port) {
    snprintf(full_port, sizeof(full_port), "%s%s", PORT_PREFIX, port);

    serial_port = open(full_port, O_RDWR | O_NOCTTY | O_NDELAY);

    if (serial_port < 0) {
        return -1;
    }

    // Восстанавливаем блокирующий режим
    fcntl(serial_port, F_SETFL, 0);

    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(serial_port, &tty) != 0) {
        close(serial_port);
        return -1;
    }

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
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        close(serial_port);
        return -1;
    }

    tcflush(serial_port, TCIOFLUSH);

    return serial_port;
}
#endif

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
#ifdef _WIN32
    // Установка UTF-8 кодировки
    SetConsoleOutputCP(CP_UTF8);
    // Также можно установить ввод
    SetConsoleCP(CP_UTF8);

#endif
    // Определение порта по умолчанию
    const char* default_port;

#ifdef _WIN32
    default_port = "COM3";  //
    printf("Платформа: Windows\n");
#else
    default_port = "ttyACM0";  // По умолчанию для Linux
    printf("Платформа: Linux\n");

    // Настройка обработчика Ctrl+C только для Linux
    signal(SIGINT, handle_signal);
#endif

    printf("Прием всех устройств... Ctrl+C для выхода\n");
    printf("===========================================\n");

#ifdef _WIN32
    // Настройка COM-порта для Windows
    HANDLE serial_port = setup_serial_port(default_port);
    if (serial_port == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Ошибка открытия порта %s\n", default_port);
        return 1;
    }
#else
    // Настройка последовательного порта для Linux
    int _port = setup_serial_port(default_port);
    if (_port < 0) {
        fprintf(stderr, "Ошибка открытия порта %s%s\n", PORT_PREFIX, default_port);
        return 1;
    }
#endif

    uint8_t packet[26];
    int packet_idx = 0;
    int total_packets = 0;

    // Главный цикл приема данных
    while (!stop_flag) {
        int byte_read = 0;
        uint8_t byte = 0;

#ifdef _WIN32
        // Чтение для Windows
        if (win32_read_byte(serial_port, &byte)) {
            byte_read = 1;

            // Проверка на Ctrl+C для Windows
            if (_kbhit()) {
                char ch = _getch();
                if (ch == 0x03) {  // Ctrl+C
                    stop_flag = 1;
                    break;
                }
            }
        }
#else
        // Чтение для Linux
        int n = read(serial_port, &byte, 1);
        if (n > 0) {
            byte_read = 1;
        }
#endif

        if (byte_read) {
            packet[packet_idx++] = byte;

            if (packet_idx == 26) {
                // Создаем структуру для данных
                DeviceData data;

                // Парсим пакет
                parse_packet(packet, &data);

                // Выводим данные
                print_device_data_oneline(total_packets++, &data);

                // Сбрасываем индекс для следующего пакета
                packet_idx = 0;
            }
        }

#ifdef _WIN32
        Sleep(1);  // Задержка 1 мс для Windows
#else
        usleep(1000);  // Задержка 1 мс для Linux
#endif
    }

    // Закрытие порта
#ifdef _WIN32
    CloseHandle(serial_port);
#else
    close(serial_port);
#endif

    printf("\nЗавершение работы...\n");
    return 0;
}