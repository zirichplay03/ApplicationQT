#ifndef ENOD_H
#define ENOD_H

#include "ComPort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <array>
#include <string>
#include <QObject>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
    #define usleep(x) Sleep((x)/1000)
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

class Enod : public QObject, public ComPortBase {
Q_OBJECT

public:
    Enod(QObject* parent = nullptr);
    void parce_packet();
    std::string get_data_string();
    void read_port();
    void stop() {
        stop_flag = true;
    }
    void reset() {
        stop_flag = false;
        packet_idx = 0;
        packet_num = 0;
    }

    DeviceData device_data_;
    volatile bool stop_flag = false;

    // Делаем эти переменные публичными для доступа из MainWindow
    int packet_idx = 0;
    int packet_num = 0;

signals:
    void newDataAvailable(const QString& data);

private:
    char buffer[200];
    std::string data_str;
    std::array<uint8_t, 26> packet_data;
    int _port;
    uint8_t byte = 0;
    uint8_t packet[26];
    const uint8_t* packet_;
    int bytes_read;
    uint16_t pressure;
    int8_t temp_raw;
    uint8_t voltage_raw;
    uint8_t fw_unsigned;
    int8_t fw_signed;

#ifdef _WIN32
    DWORD bytes_read_win;
#endif
};

#endif