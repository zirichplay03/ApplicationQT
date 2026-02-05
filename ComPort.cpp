#include "ComPort.h"

int ComPortBase::setup_serial_port() {

    if (port == nullptr) {
        return -1;
    }

    snprintf(full_port, sizeof(full_port), "%s%s", PORT_PREFIX, port);

    serial_port = open(full_port, O_RDWR | O_NOCTTY | O_NDELAY);

    if (serial_port < 0) {
        return -1;
    }

    fcntl(serial_port, F_SETFL, 0);
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(serial_port, &tty) != 0) {
        close(serial_port);
        return -1;
    }

    cfsetospeed(&tty, speed_m);
    cfsetispeed(&tty, speed_m);
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

    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        close(serial_port);
        return -1;
    }

    tcflush(serial_port, TCIOFLUSH);
    return serial_port;
}

// Метод поиска и автоматического выбора порта
int ComPortBase::search_port() {

    // Открываем директорию /dev/
    dir = opendir(PORT_PREFIX);
    if (!dir) {
        return -1;
    }

    // Перебираем все файлы в директории /dev/
    while ((entry = readdir(dir)) != nullptr) {
        // Сохраняем имя файла в переменную класса
        name = entry->d_name;

        // Проверяем, является ли это COM-портом
        if (name.find("ttyUSB") == 0 || name.find("ttyACM") == 0) {
            found_ports++;

            // Сохраняем имя порта в переменную класса
            port = name.c_str();

            // Пробуем подключиться к порту
            serial_port = setup_serial_port();

            if (serial_port >= 0) {
                connected_port = serial_port;
                break; // Нашли рабочий порт, выходим из цикла
            } else {
                port = nullptr; // Сбрасываем порт, если не удалось подключиться
            }
        }
    }

    // Закрываем директорию
    closedir(dir);

    return connected_port;
}


