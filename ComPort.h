#ifndef COMPORTBASE_H
#define COMPORTBASE_H

#ifdef _WIN32
#include <windows.h>
    #include <winbase.h>
    #define PORT_PREFIX ""
    #define O_RDWR GENERIC_READ | GENERIC_WRITE
    #define O_NOCTTY 0
    #define O_NDELAY 0
    #define tcflush FlushFileBuffers
    #define TCIOFLUSH 0
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#define PORT_PREFIX "/dev/"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
    #include <setupapi.h>
    #include <devguid.h>
    #include <regstr.h>
    #pragma comment(lib, "setupapi.lib")
#endif

class ComPortBase {
public:
    ComPortBase() : port(nullptr), serial_port(-1), found_ports(0), connected_port(-1), dir(nullptr), entry(nullptr) {
#ifdef _WIN32
        memset(&dcb, 0, sizeof(dcb));
#endif
    }

    int setup_serial_port();
    int search_port();
    void close_port() {
        if (serial_port >= 0) {
#ifdef _WIN32
            CloseHandle((HANDLE)serial_port);
#else
            close(serial_port);
#endif
            serial_port = -1;
        }
    }

    const char* get_port() const { return port; }
    void set_port(const char* p) { port = p; }
    void set_speed(speed_t speed) { speed_m = speed; }

#ifdef _WIN32
    void set_speed_win(DWORD speed) { baud_rate = speed; }
#endif

    const char* port;
private:
    speed_t speed_m;

#ifdef _WIN32
    DWORD baud_rate;
    DCB dcb;
    COMMTIMEOUTS timeouts;
#endif

    std::string name;
    void* dir;
    void* entry;
    char full_port[50];
    int serial_port;

#ifndef _WIN32
    struct termios tty;
#endif

    int found_ports;
    int connected_port;

    // Вспомогательные методы для поиска портов
#ifdef _WIN32
    std::vector<std::string> get_windows_ports();
#endif
    bool test_port_connection(const std::string& port_name);
};

#endif