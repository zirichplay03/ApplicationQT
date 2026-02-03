#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>


//int ComPortBase::auto_detect_port() {
//    printf("=== Список COM-портов ===\n");
//
//    DIR* dir = opendir("/dev/");
//    if (!dir) {
//        printf("Ошибка: не могу открыть /dev/\n");
//        return -1;
//    }
//
//    struct dirent* entry;
//    int port_count = 0;
//
//    while ((entry = readdir(dir)) != nullptr) {
//        std::string name = entry->d_name;
//
//        if (name.find("ttyUSB") == 0 || name.find("ttyACM") == 0) {
//            port_count++;
//
//            char path[100];
//            snprintf(path, sizeof(path), "%s%s", PORT_PREFIX, name.c_str());
//
//            printf("%d. %s - ", port_count, name.c_str());
//
//            int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
//            if (fd >= 0) {
//                close(fd);
//                printf("свободен\n");
//            } else {
//                printf("занят\n");
//            }
//        }
//    }
//
//    closedir(dir);
//
//    if (port_count == 0) {
//        printf("Нет подключенных COM-портов\n");
//    } else {
//        printf("Всего портов: %d\n", port_count);
//    }
//
//    return port_count;  // Возвращаем количество найденных портов





//int ComPortBase::search_port() {
//    printf("=== Поиск доступных COM-портов ===\n");
//
//    dir = opendir(PORT_PREFIX);
//    if (!dir) {
//        printf("Ошибка: не могу открыть %s\n", PORT_PREFIX);
//        return -1;
//    }
//
//    std::vector<std::string> available_ports;
//
//    // Поиск всех портов ttyUSB и ttyACM
//    while ((entry = readdir(dir)) != nullptr) {
//        name = entry->d_name;
//
//        if (name.find("ttyUSB") == 0 || name.find("ttyACM") == 0 ||
//            name.find("ttyS") == 0) {
//            char port_path[100];
//            snprintf(port_path, sizeof(port_path), "%s%s", PORT_PREFIX, name.c_str());
//
//            // Пробуем открыть порт
//            int test_fd = open(port_path, O_RDWR | O_NOCTTY | O_NDELAY);
//            if (test_fd >= 0) {
//                // Проверяем, действительно ли это serial-порт
//                struct termios test_tty;
//                if (tcgetattr(test_fd, &test_tty) == 0) {
//                    available_ports.push_back(name);
//                    printf("Найден порт: %s\n", name.c_str());
//                }
//                close(test_fd);
//            }
//        }
//    }
//
//    closedir(dir);
//
//    if (available_ports.empty()) {
//        printf("Не найдено доступных COM-портов\n");
//        return -1;
//    }
//
//    // Сортируем порты для предсказуемого порядка
//    std::sort(available_ports.begin(), available_ports.end());
//
//    // Пробуем подключиться к каждому порту
//    for (const auto& port_name : available_ports) {
//        printf("Попытка подключения к %s...\n", port_name.c_str());
//
//        // Устанавливаем найденный порт
//        port = port_name.c_str();
//
//        // Пробуем настройку порта
//        int result = setup_serial_port();
//
//        if (result >= 0) {
//            printf("Успешно подключились к порту: %s\n", port_name.c_str());
//            return result;
//        } else {
//            printf("Не удалось подключиться к порту: %s\n", port_name.c_str());
//        }
//    }
//
//    printf("Не удалось подключиться ни к одному из доступных портов\n");
//    return -1;
//}

// ========== Структуры для данных ==========
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

typedef struct {
    int total;
    int sensors;
    int repeaters;
    int unknown;
} Statistics;

// ========== Глобальные переменные ==========
volatile sig_atomic_t stop_flag = 0;
Statistics stats = {0};

// ========== Прототипы функций ==========
void handle_signal(int sig);
int setup_serial_port(const char* port);
const char* get_device_type_str(uint8_t type_byte);
void parse_packet(const uint8_t* packet, DeviceData* data);
void print_device_data(int packet_num, const DeviceData* data);
void update_statistics(uint8_t device_type);

// ========== Обработчик сигнала ==========
void handle_signal(int sig) {
    stop_flag = 1;
}

// ========== Настройка последовательного порта ==========
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

// ========== Определение типа устройства ==========
const char* get_device_type_str(uint8_t type_byte) {
    if (type_byte == 0xF0) {
        return "ДАТЧИК";
    } else if (type_byte == 0xF1) {
        return "РЕПИТЕР";
    } else {
        return "НЕИЗВЕСТНО";
    }
}

// ========== Парсинг пакета данных ==========
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

// ========== Вывод данных устройства ==========
void print_device_data(int packet_num, const DeviceData* data) {
    printf("[Пакет %d] %s | ID: 0x%08X\n",
           packet_num, data->type_str, data->id);

    printf("  Версия: %d | Давление: %.3f бар\n",
           data->fw_version, data->pressure_bar);

    printf("  Температура: %d°C | Напряжение: %.3fВ\n",
           data->temperature_c, data->voltage_v);

    printf("  RSSI: %d\n", data->rssi);

    // Байты ID как в пакете
    printf("  Байты ID в пакете: 0x%02X 0x%02X 0x%02X 0x%02X\n",
           data->raw_packet[3], data->raw_packet[4],
           data->raw_packet[5], data->raw_packet[6]);

    // HEX вывод всего пакета
    printf("  HEX: ");
    for (int i = 0; i < 26; i++) {
        printf("%02X ", data->raw_packet[i]);
    }
    printf("\n");

    printf("--------------------------------------------\n");
}

// ========== Обновление статистики ==========
void update_statistics(uint8_t device_type) {
    stats.total++;

    if (device_type == 0xF0) {
        stats.sensors++;
    } else if (device_type == 0xF1) {
        stats.repeaters++;
    } else {
        stats.unknown++;
    }
}

// ========== Вывод статистики ==========
void print_statistics(void) {
    printf("\n=== СТАТИСТИКА ===\n");
    printf("Всего пакетов: %d\n", stats.total);
    printf("Датчиков: %d\n", stats.sensors);
    printf("Репитеров: %d\n", stats.repeaters);
    printf("Неизвестных: %d\n", stats.unknown);
}

// ========== Главная функция ==========
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

                // Обновляем статистику
                update_statistics(packet[2]);

                // Выводим данные
                print_device_data(stats.total, &data);

                // Сбрасываем индекс для следующего пакета
                packet_idx = 0;
            }
        }

        usleep(1000);
    }

    // Вывод статистики и закрытие порта
    print_statistics();
    close(serial_port);

    return 0;
}