#ifndef COMPORTBASE_H
#define COMPORTBASE_H

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <string>
#include <vector>

#define PORT_PREFIX "/dev/"

class ComPortBase {
public:
    ComPortBase() : port(nullptr) {}

    int setup_serial_port();
    int search_port();
    const char* port;
    speed_t speed_m;

private:
    std::string name;
    DIR* dir;
    struct dirent* entry;
    char full_port[50];
    int serial_port;
    struct termios tty;
    int found_ports = 0;
    int connected_port = -1;
};
#endif