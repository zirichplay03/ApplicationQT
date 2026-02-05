#include "ComPort.h"

#ifdef _WIN32
std::vector<std::string> ComPortBase::get_windows_ports() {
    std::vector<std::string> ports;
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, 0, 0, DIGCF_PRESENT);

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return ports;
    }

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &deviceInfoData); i++) {
        DWORD size = 0;

        // Получаем имя устройства
        SetupDiGetDeviceRegistryProperty(hDevInfo, &deviceInfoData, SPDRP_FRIENDLYNAME,
                                         NULL, NULL, 0, &size);

        if (size > 0) {
            std::vector<char> buffer(size);
            if (SetupDiGetDeviceRegistryProperty(hDevInfo, &deviceInfoData, SPDRP_FRIENDLYNAME,
                                                 NULL, (PBYTE)buffer.data(), size, NULL)) {
                std::string deviceName(buffer.begin(), buffer.end() - 1); // Убираем нулевой символ

                // Ищем COM порты
                size_t comPos = deviceName.find("(COM");
                if (comPos != std::string::npos) {
                    size_t start = comPos + 1; // Пропускаем '('
                    size_t end = deviceName.find(")", start);
                    if (end != std::string::npos) {
                        std::string comPort = deviceName.substr(start, end - start);
                        ports.push_back(comPort);
                    }
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return ports;
}

bool ComPortBase::test_port_connection(const std::string& port_name) {
    std::string full_name = "\\\\.\\" + port_name;
    HANDLE hPort = CreateFile(full_name.c_str(),
                              GENERIC_READ | GENERIC_WRITE,
                              0,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL);

    if (hPort == INVALID_HANDLE_VALUE) {
        return false;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(hPort, &dcb)) {
        CloseHandle(hPort);
        return false;
    }

    // Пробуем установить базовые настройки
    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    if (!SetCommState(hPort, &dcb)) {
        CloseHandle(hPort);
        return false;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hPort, &timeouts)) {
        CloseHandle(hPort);
        return false;
    }

    CloseHandle(hPort);
    return true;
}

#else

bool ComPortBase::test_port_connection(const std::string& port_name) {
    std::string full_name = PORT_PREFIX + port_name;
    int fd = open(full_name.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd < 0) {
        return false;
    }

    close(fd);
    return true;
}

#endif

int ComPortBase::setup_serial_port() {
    if (port == nullptr) {
        return -1;
    }

#ifdef _WIN32
    // Формируем имя порта для Windows
    snprintf(full_port, sizeof(full_port), "\\\\.\\%s", port);

    // Открываем порт
    HANDLE hPort = CreateFile(full_port,
                              GENERIC_READ | GENERIC_WRITE,
                              0,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL);

    if (hPort == INVALID_HANDLE_VALUE) {
        return -1;
    }

    serial_port = (int)hPort;

    // Получаем текущие настройки
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(hPort, &dcb)) {
        CloseHandle(hPort);
        return -1;
    }

    // Настраиваем параметры порта
    dcb.BaudRate = baud_rate;  // Используем baud_rate вместо speed_m
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(hPort, &dcb)) {
        CloseHandle(hPort);
        return -1;
    }

    // Настраиваем таймауты
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hPort, &timeouts)) {
        CloseHandle(hPort);
        return -1;
    }

    // Очищаем буферы
    PurgeComm(hPort, PURGE_RXCLEAR | PURGE_TXCLEAR);

#else
    // Linux версия (остается почти без изменений)
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
#endif

    return serial_port;
}

int ComPortBase::search_port() {
#ifdef _WIN32
    // Windows: получаем список COM портов
    std::vector<std::string> ports = get_windows_ports();

    for (const auto& port_name : ports) {
        found_ports++;

        // Сохраняем имя порта
        port = port_name.c_str();

        // Пробуем подключиться к порту
        serial_port = setup_serial_port();

        if (serial_port >= 0) {
            connected_port = serial_port;
            break;
        } else {
            port = nullptr;
        }
    }

#else
    // Linux версия (остается почти без изменений)
    dir = opendir(PORT_PREFIX);
    if (!dir) {
        return -1;
    }

    while ((entry = readdir((DIR*)dir)) != nullptr) {
        name = ((dirent*)entry)->d_name;

        if (name.find("ttyUSB") == 0 || name.find("ttyACM") == 0 ||
            name.find("ttyS") == 0 || name.find("ttyAMA") == 0) {
            found_ports++;

            port = name.c_str();
            serial_port = setup_serial_port();

            if (serial_port >= 0) {
                connected_port = serial_port;
                break;
            } else {
                port = nullptr;
            }
        }
    }

    if (dir) {
        closedir((DIR*)dir);
    }
#endif

    return connected_port;
}