#include "Enod.h"
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <iomanip>

Enod::Enod(QObject* parent) : QObject(parent) {
}

void Enod::parce_packet() {
    packet_ = packet_data.data();

    auto get_device_type_str = [](uint8_t type_byte) -> const char* {
        if (type_byte == 0xF0) return "ДАТЧИК";
        if (type_byte == 0xF1) return "РЕПИТЕР";
        return "НЕИЗВЕСТНО";
    };

    std::memcpy(device_data_.raw_packet, packet_, 26);
    device_data_.type_str = get_device_type_str(packet_[2]);

    device_data_.id = ((uint32_t)packet_[6] << 24) |
                      ((uint32_t)packet_[5] << 16) |
                      ((uint32_t)packet_[4] << 8) |
                      packet_[3];

    // Давление
    pressure = ((uint16_t)packet_[8] << 8) | packet_[7];
    device_data_.pressure_bar = 2750.0f * (pressure - 1) / 100000.0f;

    // Температура
    temp_raw = packet_[14];
    device_data_.temperature_c = (int)temp_raw - 55;

    // Напряжение
    voltage_raw = packet_[13];
    device_data_.voltage_v = voltage_raw * 0.01512f;

    // Версия прошивки
    fw_unsigned = packet_[19];
    fw_signed = (int8_t)fw_unsigned;
    device_data_.fw_version = abs((int)fw_signed);

    // RSSI
    device_data_.rssi = -(int8_t)packet_[24];
}

std::string Enod::get_data_string() {

    snprintf(buffer, sizeof(buffer),
             "[%2d] %-7s | ID: 0x%08X | "
             "Вер: %2d | "
             "Д: %6.3f бар | "
             "Т: %2d°C | "
             "Н: %5.3fВ | "
             "RSSI: %3d | "
             "Байты: 0x%02X%02X%02X%02X",
             packet_num,
             device_data_.type_str,
             device_data_.id,
             device_data_.fw_version,
             device_data_.pressure_bar,
             device_data_.temperature_c,
             device_data_.voltage_v,
             device_data_.rssi,
             device_data_.raw_packet[3],
             device_data_.raw_packet[4],
             device_data_.raw_packet[5],
             device_data_.raw_packet[6]);

    packet_num++;
    return std::string(buffer);
}

void Enod::read_port() {
    _port = search_port();

    if (_port < 0) {
        return;
    }

    stop_flag = false;

    while (!stop_flag) {
        bytes_read = read(_port, &byte, 1);

        if (bytes_read > 0) {
            packet[packet_idx++] = byte;

            if (packet_idx == 26) {
                std::memcpy(packet_data.data(), packet, 26);
                parce_packet();

                // Получаем строку данных
                data_str = get_data_string();

                // Испускаем сигнал с данными
                emit newDataAvailable(QString::fromStdString(data_str));

                // Сбрасываем индекс
                packet_idx = 0;
            }
        }
        else if (bytes_read < 0) {
            break;
        }

        usleep(1000);
    }

    if (_port >= 0) {
        close(_port);
    }

    stop_flag = false;
}