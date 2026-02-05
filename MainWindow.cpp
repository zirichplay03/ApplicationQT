#include "MainWindow.h"
#include <QtConcurrent/QtConcurrent>
#include <QHeaderView>
#include <QDateTime>
#include <QTimer>
#include <QScrollBar>
#include <QStatusBar>
#include <QFont>
#include <QFontMetrics>
#include <QPalette>

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent), enod(nullptr), isConnected(false),
          uniquePacketCount(0), totalPacketCount(0),
          sensorCount(0), repeaterCount(0), unknownCount(0),
          packetCountInPeriod(0), uniquePacketCountInPeriod(0),
          lastSummaryTime(QDateTime::currentDateTime()),
          lastPacketTime(QDateTime::currentDateTime()),
          lastRepeaterTime(QDateTime::currentDateTime())
{
    // Инициализация Enod
    enod = new Enod(this);

    // Настройка главного окна
    setWindowTitle("Enod Data Monitor");
    setMinimumSize(1400, 800);

    // Создаем интерфейс
    setupUI();
    setupStatusBar();

    // Подключаем сигналы и слоты
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectToPort);
    connect(disconnectButton, &QPushButton::clicked, this, &MainWindow::disconnectFromPort);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearDisplay);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetData);

    // Подключаем сигнал от Enod
    connect(enod, &Enod::newDataAvailable, this, &MainWindow::onDataReceived);

    // Таймер для обновления текущего времени
    QTimer *clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    clockTimer->start(1000);

    // Таймер для обновления индикаторов связи
    QTimer *indicatorTimer = new QTimer(this);
    connect(indicatorTimer, &QTimer::timeout, this, &MainWindow::updateConnectionIndicators);
    indicatorTimer->start(1000);

    // Таймер для периодической сводки
    QTimer *summaryTimer = new QTimer(this);
    connect(summaryTimer, &QTimer::timeout, this, &MainWindow::generateSummary);
    summaryTimer->start(30000);

    // Первоначальное обновление списка портов
    refreshPorts();
}

MainWindow::~MainWindow()
{
    if (enod) {
        enod->stop();
        delete enod;
    }
}

void MainWindow::setupUI()
{
    // Центральный виджет
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    // ========== ЛЕВАЯ ПАНЕЛЬ: Управление портом ==========
    QGroupBox *portControlGroup = new QGroupBox("Управление портом", centralWidget);
    portControlGroup->setMaximumWidth(280);

    QLabel *portLabel = new QLabel("Порт:", portControlGroup);
    portComboBox = new QComboBox(portControlGroup);
    portComboBox->addItem("Выберите порт...");

    QLabel *speedLabel = new QLabel("Скорость:", portControlGroup);
    speedComboBox = new QComboBox(portControlGroup);
    speedComboBox->addItems(QStringList() << "1200" << "2400" << "4800" << "9600" << "19200" << "38400" << "57600" << "115200");
    speedComboBox->setCurrentIndex(3);

    refreshButton = new QPushButton("Обновить список портов", portControlGroup);
    connectButton = new QPushButton("Подключиться", portControlGroup);
    disconnectButton = new QPushButton("Отключиться", portControlGroup);
    disconnectButton->setEnabled(false);
    clearButton = new QPushButton("Очистить таблицу", portControlGroup);
    resetButton = new QPushButton("Сброс данных", portControlGroup);
    resetButton->setToolTip("Сбросить все данные и статистику");

    QVBoxLayout *portLayout = new QVBoxLayout(portControlGroup);
    portLayout->addWidget(portLabel);
    portLayout->addWidget(portComboBox);
    portLayout->addWidget(speedLabel);
    portLayout->addWidget(speedComboBox);
    portLayout->addSpacing(10);
    portLayout->addWidget(refreshButton);
    portLayout->addWidget(connectButton);
    portLayout->addWidget(disconnectButton);
    portLayout->addWidget(clearButton);
    portLayout->addWidget(resetButton);
    portLayout->addStretch();

    // ========== ЦЕНТРАЛЬНАЯ ПАНЕЛЬ: Таблица данных ==========
    QGroupBox *dataGroup = new QGroupBox("Данные с порта", centralWidget);

    // Создаем таблицу
    dataTable = new QTableWidget(dataGroup);
    dataTable->setColumnCount(9);
    QStringList headers;
    headers << "Время пакета" << "ID устройства" << "Тип" << "Версия"
            << "Давление (бар)" << "Температура (°C)" << "Напряжение (В)"
            << "RSSI" << "Всего пакетов";
    dataTable->setHorizontalHeaderLabels(headers);

    // Настройка внешнего вида таблицы
    dataTable->setAlternatingRowColors(true);
    dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    dataTable->verticalHeader()->setVisible(false);
    dataTable->setSortingEnabled(false);
    dataTable->setShowGrid(true);

    // Настраиваем размеры колонок
    dataTable->setColumnWidth(0, 120);
    dataTable->setColumnWidth(1, 120);
    dataTable->setColumnWidth(2, 80);
    dataTable->setColumnWidth(3, 70);
    dataTable->setColumnWidth(4, 110);
    dataTable->setColumnWidth(5, 110);
    dataTable->setColumnWidth(6, 100);
    dataTable->setColumnWidth(7, 70);
    dataTable->setColumnWidth(8, 100);

    // Устанавливаем высоту строк
    dataTable->verticalHeader()->setDefaultSectionSize(24);

    // Настраиваем заголовки
    QHeaderView* header = dataTable->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(QHeaderView::Fixed);

    QVBoxLayout *dataLayout = new QVBoxLayout(dataGroup);
    dataLayout->addWidget(dataTable);

    // ========== ПРАВАЯ ПАНЕЛЬ: Статус и статистика ==========
    QGroupBox *statusGroup = new QGroupBox("Статус и статистика", centralWidget);
    statusGroup->setMaximumWidth(350);

    // Текущее время
    currentTimeLabel = new QLabel("Текущее время: -", statusGroup);
    currentTimeLabel->setFont(QFont("Arial", 10, QFont::Bold));

    // Индикаторы связи
    QGroupBox *indicatorsGroup = new QGroupBox("Индикаторы связи", statusGroup);
    QVBoxLayout *indicatorsLayout = new QVBoxLayout(indicatorsGroup);

    // Индикатор датчиков
    QHBoxLayout *sensorIndicatorLayout = new QHBoxLayout();
    QLabel *sensorIndicatorLabel = new QLabel("Датчики:", indicatorsGroup);
    sensorIndicator = new QLabel("●", indicatorsGroup);
    sensorIndicator->setFixedSize(20, 20);
    sensorIndicator->setAlignment(Qt::AlignCenter);
    sensorIndicator->setStyleSheet("QLabel { background-color: red; border-radius: 10px; color: white; font-weight: bold; }");
    sensorIndicatorLayout->addWidget(sensorIndicatorLabel);
    sensorIndicatorLayout->addWidget(sensorIndicator);
    sensorIndicatorLayout->addStretch();

    // Индикатор репитера
    QHBoxLayout *repeaterIndicatorLayout = new QHBoxLayout();
    QLabel *repeaterIndicatorLabel = new QLabel("Репитер:", indicatorsGroup);
    repeaterIndicator = new QLabel("●", indicatorsGroup);
    repeaterIndicator->setFixedSize(20, 20);
    repeaterIndicator->setAlignment(Qt::AlignCenter);
    repeaterIndicator->setStyleSheet("QLabel { background-color: red; border-radius: 10px; color: white; font-weight: bold; }");
    repeaterIndicatorLayout->addWidget(repeaterIndicatorLabel);
    repeaterIndicatorLayout->addWidget(repeaterIndicator);
    repeaterIndicatorLayout->addStretch();

    indicatorsLayout->addLayout(sensorIndicatorLayout);
    indicatorsLayout->addLayout(repeaterIndicatorLayout);
    indicatorsLayout->addStretch();

    // Статус подключения
    connectionStatusLabel = new QLabel("Статус: Не подключено", statusGroup);
    portInfoLabel = new QLabel("Порт: -", statusGroup);
    speedInfoLabel = new QLabel("Скорость: -", statusGroup);

    // Статистика по типам устройств
    QGroupBox *deviceStatsGroup = new QGroupBox("Статистика устройств", statusGroup);
    QVBoxLayout *deviceStatsLayout = new QVBoxLayout(deviceStatsGroup);

    sensorCountLabel = new QLabel("Датчиков: 0", deviceStatsGroup);
    repeaterCountLabel = new QLabel("Репитеров: 0", deviceStatsGroup);
    totalDevicesLabel = new QLabel("Всего устройств: 0", deviceStatsGroup);
    totalPacketsLabel = new QLabel("Всего пакетов: 0", deviceStatsGroup);
    packetRateLabel = new QLabel("Скорость приема: 0 пак/с", deviceStatsGroup);

    // Устанавливаем шрифт для статистики
    QFont statsFont("Arial", 9);
    sensorCountLabel->setFont(statsFont);
    repeaterCountLabel->setFont(statsFont);
    totalDevicesLabel->setFont(statsFont);
    totalPacketsLabel->setFont(statsFont);
    packetRateLabel->setFont(statsFont);

    deviceStatsLayout->addWidget(sensorCountLabel);
    deviceStatsLayout->addWidget(repeaterCountLabel);
    deviceStatsLayout->addWidget(totalDevicesLabel);
    deviceStatsLayout->addWidget(totalPacketsLabel);
    deviceStatsLayout->addWidget(packetRateLabel);
    deviceStatsLayout->addStretch();

    // Информация о последнем пакете (датчика)
    QGroupBox *lastPacketGroup = new QGroupBox("Последний датчик", statusGroup);
    QVBoxLayout *lastPacketLayout = new QVBoxLayout(lastPacketGroup);

    lastPacketTimeLabel = new QLabel("Время: -", lastPacketGroup);
    lastDeviceIdLabel = new QLabel("ID: -", lastPacketGroup);
    lastDeviceTypeLabel = new QLabel("Тип: -", lastPacketGroup);
    lastDeviceVersionLabel = new QLabel("Версия: -", lastPacketGroup);
    lastPressureLabel = new QLabel("Давление: -", lastPacketGroup);
    lastTemperatureLabel = new QLabel("Температура: -", lastPacketGroup);
    lastVoltageLabel = new QLabel("Напряжение: -", lastPacketGroup);
    lastRssiLabel = new QLabel("RSSI: -", lastPacketGroup);
    lastTotalPacketsLabel = new QLabel("Всего пакетов: -", lastPacketGroup);

    // Устанавливаем шрифт для информации о последнем пакете
    QFont lastPacketFont("Arial", 9);
    lastPacketTimeLabel->setFont(lastPacketFont);
    lastDeviceIdLabel->setFont(lastPacketFont);
    lastDeviceTypeLabel->setFont(lastPacketFont);
    lastDeviceVersionLabel->setFont(lastPacketFont);
    lastPressureLabel->setFont(lastPacketFont);
    lastTemperatureLabel->setFont(lastPacketFont);
    lastVoltageLabel->setFont(lastPacketFont);
    lastRssiLabel->setFont(lastPacketFont);
    lastTotalPacketsLabel->setFont(lastPacketFont);

    lastPacketLayout->addWidget(lastPacketTimeLabel);
    lastPacketLayout->addWidget(lastDeviceIdLabel);
    lastPacketLayout->addWidget(lastDeviceTypeLabel);
    lastPacketLayout->addWidget(lastDeviceVersionLabel);
    lastPacketLayout->addWidget(lastPressureLabel);
    lastPacketLayout->addWidget(lastTemperatureLabel);
    lastPacketLayout->addWidget(lastVoltageLabel);
    lastPacketLayout->addWidget(lastRssiLabel);
    lastPacketLayout->addWidget(lastTotalPacketsLabel);
    lastPacketLayout->addStretch();

    // Сводка за период
    QGroupBox *summaryGroup = new QGroupBox("Сводка за 30 сек", statusGroup);
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryGroup);

    periodPacketsLabel = new QLabel("Пакетов: 0", summaryGroup);
    periodUniqueLabel = new QLabel("Новых устройств: 0", summaryGroup);
    periodRateLabel = new QLabel("Скорость: 0 пак/с", summaryGroup);

    summaryLayout->addWidget(periodPacketsLabel);
    summaryLayout->addWidget(periodUniqueLabel);
    summaryLayout->addWidget(periodRateLabel);
    summaryLayout->addStretch();

    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
    statusLayout->addWidget(currentTimeLabel);
    statusLayout->addSpacing(5);
    statusLayout->addWidget(indicatorsGroup);
    statusLayout->addSpacing(5);
    statusLayout->addWidget(connectionStatusLabel);
    statusLayout->addWidget(portInfoLabel);
    statusLayout->addWidget(speedInfoLabel);
    statusLayout->addSpacing(10);
    statusLayout->addWidget(deviceStatsGroup);
    statusLayout->addSpacing(10);
    statusLayout->addWidget(lastPacketGroup);
    statusLayout->addSpacing(10);
    statusLayout->addWidget(summaryGroup);
    statusLayout->addStretch();

    // Распределяем пространство
    mainLayout->addWidget(portControlGroup, 1);
    mainLayout->addWidget(dataGroup, 3);
    mainLayout->addWidget(statusGroup, 2);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Готово к работе");
    statusBarLabel = new QLabel("Время: - | Устройств: 0 | Пакетов: 0");
    statusBar()->addPermanentWidget(statusBarLabel);
}

void MainWindow::updateClock()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QString timeStr = currentTime.toString("dd.MM.yyyy HH:mm:ss");
    currentTimeLabel->setText("Текущее время: " + timeStr);
    statusBarLabel->setText(QString("Время: %1 | Устройств: %2 | Пакетов: %3")
                                    .arg(currentTime.toString("HH:mm:ss"))
                                    .arg(sensorCount)
                                    .arg(totalPacketCount));
}

void MainWindow::updateConnectionIndicators()
{
    QDateTime currentTime = QDateTime::currentDateTime();

    if (!isConnected) {
        // Красный для обоих индикаторов, если не подключено
        sensorIndicator->setStyleSheet("QLabel { background-color: red; border-radius: 10px; color: white; font-weight: bold; }");
        repeaterIndicator->setStyleSheet("QLabel { background-color: red; border-radius: 10px; color: white; font-weight: bold; }");
        return;
    }

    // Обновляем индикатор датчиков
    qint64 secondsSinceLastPacket = lastPacketTime.secsTo(currentTime);

    if (secondsSinceLastPacket < 2) {
        sensorIndicator->setStyleSheet("QLabel { background-color: #4CAF50; border-radius: 10px; color: white; font-weight: bold; }");
    } else if (secondsSinceLastPacket < 5) {
        sensorIndicator->setStyleSheet("QLabel { background-color: #FFC107; border-radius: 10px; color: black; font-weight: bold; }");
    } else {
        sensorIndicator->setStyleSheet("QLabel { background-color: #F44336; border-radius: 10px; color: white; font-weight: bold; }");
    }

    // Обновляем индикатор репитера
    qint64 secondsSinceLastRepeater = lastRepeaterTime.secsTo(currentTime);

    if (secondsSinceLastRepeater < 2) {
        repeaterIndicator->setStyleSheet("QLabel { background-color: #4CAF50; border-radius: 10px; color: white; font-weight: bold; }");
    } else if (secondsSinceLastRepeater < 5) {
        repeaterIndicator->setStyleSheet("QLabel { background-color: #FFC107; border-radius: 10px; color: black; font-weight: bold; }");
    } else {
        repeaterIndicator->setStyleSheet("QLabel { background-color: #F44336; border-radius: 10px; color: white; font-weight: bold; }");
    }
}

void MainWindow::refreshPorts()
{
    portComboBox->clear();
    portComboBox->addItem("Выберите порт...");

    Enod tempEnod;
    int result = tempEnod.search_port();

    if (result >= 0 && tempEnod.port) {
        QString portName = QString(tempEnod.port);
        portComboBox->addItem(portName);
        statusBar()->showMessage("Найден порт: " + portName, 3000);
    } else {
        portComboBox->addItem("Порты не найдены");
        statusBar()->showMessage("COM-порты не обнаружены", 3000);
    }
}

void MainWindow::connectToPort()
{
    if (portComboBox->currentIndex() == 0) {
        QMessageBox::warning(this, "Ошибка", "Выберите порт из списка!");
        return;
    }

    QString selectedPort = portComboBox->currentText();
    QString speedText = speedComboBox->currentText();

    // Устанавливаем порт
    enod->set_port(selectedPort.toUtf8().constData());

    // Устанавливаем скорость
#ifdef _WIN32
    if (speedText == "1200") {
        enod->set_speed_win(CBR_1200);
    } else if (speedText == "2400") {
        enod->set_speed_win(CBR_2400);
    } else if (speedText == "4800") {
        enod->set_speed_win(CBR_4800);
    } else if (speedText == "9600") {
        enod->set_speed_win(CBR_9600);
    } else if (speedText == "19200") {
        enod->set_speed_win(CBR_19200);
    } else if (speedText == "38400") {
        enod->set_speed_win(CBR_38400);
    } else if (speedText == "57600") {
        enod->set_speed_win(CBR_57600);
    } else if (speedText == "115200") {
        enod->set_speed_win(CBR_115200);
    }
#else
    if (speedText == "1200") {
        enod->set_speed(B1200);
    } else if (speedText == "2400") {
        enod->set_speed(B2400);
    } else if (speedText == "4800") {
        enod->set_speed(B4800);
    } else if (speedText == "9600") {
        enod->set_speed(B9600);
    } else if (speedText == "19200") {
        enod->set_speed(B19200);
    } else if (speedText == "38400") {
        enod->set_speed(B38400);
    } else if (speedText == "57600") {
        enod->set_speed(B57600);
    } else if (speedText == "115200") {
        enod->set_speed(B115200);
    }
#endif

    isConnected = true;
    connectButton->setEnabled(false);
    disconnectButton->setEnabled(true);
    portComboBox->setEnabled(false);
    speedComboBox->setEnabled(false);
    refreshButton->setEnabled(false);

    connectionStatusLabel->setText("Статус: Подключено");
    portInfoLabel->setText("Порт: " + selectedPort);
    speedInfoLabel->setText("Скорость: " + speedText);

    statusBar()->showMessage("Подключено к " + selectedPort);

    // Запускаем чтение порта в отдельном потоке
    QtConcurrent::run([this]() {
        enod->read_port();
    });
}

void MainWindow::disconnectFromPort()
{
    if (isConnected) {
        enod->stop();
        isConnected = false;
    }

    connectButton->setEnabled(true);
    disconnectButton->setEnabled(false);
    portComboBox->setEnabled(true);
    speedComboBox->setEnabled(true);
    refreshButton->setEnabled(true);

    connectionStatusLabel->setText("Статус: Не подключено");
    portInfoLabel->setText("Порт: -");
    speedInfoLabel->setText("Скорость: -");

    statusBar()->showMessage("Отключено");
}

void MainWindow::resetData()
{
    // Останавливаем чтение порта, если подключены
    if (isConnected) {
        // Устанавливаем флаг остановки
        enod->stop_flag = true;

        // Даем время для завершения текущей операции чтения (100 мс достаточно)
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 100) {
            QCoreApplication::processEvents();
        }

        // Сбрасываем состояние Enod
        enod->packet_idx = 0;
        enod->packet_num = 0;
        enod->stop_flag = false;
    }

    // Сбрасываем статистику
    uniquePacketCount = 0;
    totalPacketCount = 0;
    packetCountInPeriod = 0;
    uniquePacketCountInPeriod = 0;
    sensorCount = 0;
    repeaterCount = 0;
    unknownCount = 0;
    lastSummaryTime = QDateTime::currentDateTime();
    lastPacketTime = QDateTime::currentDateTime();
    lastRepeaterTime = QDateTime::currentDateTime();
    packetTimestamps.clear();
    devicePacketCount.clear();
    deviceDataMap.clear();
    repeaterDataMap.clear();

    // Очищаем таблицу
    dataTable->setRowCount(0);

    // Очищаем информацию о последнем пакете
    clearLastPacketInfo();

    // Обновляем статистику
    updateStatisticsDisplay();

    // Обновляем индикаторы
    updateConnectionIndicators();

    // Перезапускаем чтение порта, если были подключены
    if (isConnected) {
        // Запускаем чтение порта в отдельном потоке
        QtConcurrent::run([this]() {
            enod->read_port();
        });
        statusBar()->showMessage("Данные сброшены, чтение порта перезапущено", 3000);
    } else {
        statusBar()->showMessage("Данные сброшены", 3000);
    }
}

void MainWindow::onDataReceived(const QString& data)
{
    QDateTime currentTime = QDateTime::currentDateTime();
    packetTimestamps.append(currentTime);

    // Обновляем счетчик всех пакетов
    totalPacketCount++;
    packetCountInPeriod++;

    // Парсим данные из строки
    QStringList parts = data.split('|');

    QString type = "НЕИЗВЕСТНО";
    QString deviceId = "N/A";
    QString version = "N/A";
    QString pressure = "N/A";
    QString temperature = "N/A";
    QString voltage = "N/A";
    QString rssi = "N/A";

    if (parts.size() >= 8) {
        // Первая часть: "[X] ТИП"
        QString firstPart = parts[0].trimmed();
        int bracketPos = firstPart.indexOf(']');
        if (bracketPos > 0) {
            QString typeStr = firstPart.mid(bracketPos + 2).trimmed();
            if (typeStr.contains("ДАТЧИК")) {
                type = "ДАТЧИК";
            } else if (typeStr.contains("РЕПИТЕР")) {
                type = "РЕПИТЕР";
            } else {
                type = "НЕИЗВЕСТНО";
            }
        }

        // Вторая часть: "ID: 0xXXXXXXXX"
        if (parts[1].contains("ID:")) {
            QString idPart = parts[1];
            int colonPos = idPart.indexOf(':');
            if (colonPos > 0) {
                deviceId = idPart.mid(colonPos + 1).trimmed();
            }
        }

        // Третья часть: "Вер: XX"
        if (parts[2].contains("Вер:")) {
            QString verPart = parts[2];
            int colonPos = verPart.indexOf(':');
            if (colonPos > 0) {
                version = verPart.mid(colonPos + 1).trimmed();
            }
        }

        // Четвертая часть: "Д: X.XXX бар"
        if (parts[3].contains("Д:")) {
            QString pressurePart = parts[3];
            int colonPos = pressurePart.indexOf(':');
            if (colonPos > 0) {
                QString value = pressurePart.mid(colonPos + 1).trimmed();
                QStringList valueParts = value.split(' ');
                if (!valueParts.isEmpty()) {
                    pressure = valueParts[0];
                }
            }
        }

        // Пятая часть: "Т: XX°C"
        if (parts[4].contains("Т:")) {
            QString tempPart = parts[4];
            int colonPos = tempPart.indexOf(':');
            if (colonPos > 0) {
                QString value = tempPart.mid(colonPos + 1).trimmed();
                QStringList valueParts = value.split(QString::fromUtf8("°"));
                if (!valueParts.isEmpty()) {
                    temperature = valueParts[0];
                }
            }
        }

        // Шестая часть: "Н: X.XXXВ"
        if (parts[5].contains("Н:")) {
            QString voltPart = parts[5];
            int colonPos = voltPart.indexOf(':');
            if (colonPos > 0) {
                QString value = voltPart.mid(colonPos + 1).trimmed();
                if (value.endsWith("В")) {
                    voltage = value.left(value.length() - 1);
                } else {
                    voltage = value;
                }
            }
        }

        // Седьмая часть: "RSSI: XXX"
        if (parts[6].contains("RSSI:")) {
            QString rssiPart = parts[6];
            int colonPos = rssiPart.indexOf(':');
            if (colonPos > 0) {
                rssi = rssiPart.mid(colonPos + 1).trimmed();
            }
        }
    }

    // Игнорируем неизвестные устройства
    if (type == "НЕИЗВЕСТНО") {
        return;
    }

    if (type == "РЕПИТЕР") {
        // Обработка репитера
        repeaterCount++;
        lastRepeaterTime = currentTime;

        // Сохраняем данные репитера
        DevicePacketInfo info;
        info.deviceId = deviceId;
        info.type = type;
        info.version = version;
        info.pressure = pressure;
        info.temperature = temperature;
        info.voltage = voltage;
        info.rssi = rssi;
        info.firstSeen = currentTime;
        info.lastSeen = currentTime;
        info.totalPacketCount = 1;

        repeaterDataMap[deviceId] = info;

    } else if (type == "ДАТЧИК") {
        // Обработка датчика
        lastPacketTime = currentTime;
        QString deviceKey = deviceId;

        // Проверяем, является ли устройство новым
        bool isNewDevice = !deviceDataMap.contains(deviceKey);

        if (isNewDevice) {
            // Новое устройство
            uniquePacketCount++;
            uniquePacketCountInPeriod++;
            sensorCount++;

            // Сохраняем данные устройства
            DevicePacketInfo info;
            info.deviceId = deviceId;
            info.type = type;
            info.version = version;
            info.pressure = pressure;
            info.temperature = temperature;
            info.voltage = voltage;
            info.rssi = rssi;
            info.firstSeen = currentTime;
            info.lastSeen = currentTime;
            info.totalPacketCount = 1;

            deviceDataMap[deviceKey] = info;
            devicePacketCount[deviceId] = 1;

            // Добавляем датчик в таблицу
            addPacketToTable(deviceKey, info, true);
        } else {
            // Обновляем существующее устройство
            DevicePacketInfo& info = deviceDataMap[deviceKey];
            info.lastSeen = currentTime;
            info.totalPacketCount++;

            // Обновляем данные, если они изменились
            bool dataChanged = false;
            if (info.pressure != pressure || info.temperature != temperature ||
                info.voltage != voltage || info.rssi != rssi) {
                info.pressure = pressure;
                info.temperature = temperature;
                info.voltage = voltage;
                info.rssi = rssi;
                dataChanged = true;
            }

            // Обновляем счетчик для этого устройства
            devicePacketCount[deviceId]++;

            // Обновляем строку в таблице
            updatePacketInTable(deviceKey, info);
        }

        // Обновляем информацию о последнем пакете (датчика)
        updateLastPacketInfo(currentTime, deviceId, type, version, pressure,
                             temperature, voltage, rssi, devicePacketCount[deviceId]);
    }

    // Обновляем статистику
    updateStatisticsDisplay();
}

void MainWindow::addPacketToTable(const QString& packetKey, const DevicePacketInfo& info, bool isNewDevice)
{
    int row = dataTable->rowCount();
    dataTable->insertRow(row);

    // Время последнего пакета
    QTableWidgetItem *timeItem = new QTableWidgetItem(info.lastSeen.toString("HH:mm:ss"));

    // ID устройства
    QTableWidgetItem *idItem = new QTableWidgetItem(info.deviceId);

    // Тип
    QTableWidgetItem *typeItem = new QTableWidgetItem(info.type);

    // Версия
    QTableWidgetItem *versionItem = new QTableWidgetItem(info.version);

    // Давление
    QTableWidgetItem *pressureItem = new QTableWidgetItem(info.pressure);

    // Температура
    QTableWidgetItem *tempItem = new QTableWidgetItem(info.temperature);

    // Напряжение
    QTableWidgetItem *voltageItem = new QTableWidgetItem(info.voltage);

    // RSSI
    QTableWidgetItem *rssiItem = new QTableWidgetItem(info.rssi);

    // Всего пакетов
    QTableWidgetItem *totalItem = new QTableWidgetItem(QString::number(info.totalPacketCount));

    // Устанавливаем цвет для датчиков (голубой)
    QColor rowColor = QColor(220, 240, 255);

    // Применяем цвет ко всем ячейкам
    QList<QTableWidgetItem*> items = {timeItem, idItem, typeItem, versionItem, pressureItem,
                                      tempItem, voltageItem, rssiItem, totalItem};
    for (auto item : items) {
        item->setBackground(rowColor);
        item->setForeground(Qt::black);
    }

    // Делаем данные жирными для новых устройств
    if (isNewDevice) {
        QFont boldFont = idItem->font();
        boldFont.setBold(true);
        for (auto item : items) {
            item->setFont(boldFont);
        }
    }

    // Устанавливаем выравнивание для числовых значений
    pressureItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tempItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    voltageItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rssiItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    totalItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Сохраняем ключ устройства в данные строки
    timeItem->setData(Qt::UserRole, packetKey);

    // Добавляем ячейки в таблицу
    dataTable->setItem(row, 0, timeItem);
    dataTable->setItem(row, 1, idItem);
    dataTable->setItem(row, 2, typeItem);
    dataTable->setItem(row, 3, versionItem);
    dataTable->setItem(row, 4, pressureItem);
    dataTable->setItem(row, 5, tempItem);
    dataTable->setItem(row, 6, voltageItem);
    dataTable->setItem(row, 7, rssiItem);
    dataTable->setItem(row, 8, totalItem);
}

void MainWindow::updatePacketInTable(const QString& packetKey, const DevicePacketInfo& info)
{
    // Ищем строку с этим устройством
    for (int row = 0; row < dataTable->rowCount(); ++row) {
        QTableWidgetItem *item = dataTable->item(row, 0);
        if (item && item->data(Qt::UserRole).toString() == packetKey) {
            // Обновляем данные
            QTableWidgetItem *timeItem = dataTable->item(row, 0);
            QTableWidgetItem *pressureItem = dataTable->item(row, 4);
            QTableWidgetItem *tempItem = dataTable->item(row, 5);
            QTableWidgetItem *voltageItem = dataTable->item(row, 6);
            QTableWidgetItem *rssiItem = dataTable->item(row, 7);
            QTableWidgetItem *totalItem = dataTable->item(row, 8);

            if (timeItem) timeItem->setText(info.lastSeen.toString("HH:mm:ss"));
            if (pressureItem) pressureItem->setText(info.pressure);
            if (tempItem) tempItem->setText(info.temperature);
            if (voltageItem) voltageItem->setText(info.voltage);
            if (rssiItem) rssiItem->setText(info.rssi);
            if (totalItem) totalItem->setText(QString::number(info.totalPacketCount));

            break;
        }
    }
}

void MainWindow::clearDisplay()
{
    dataTable->setRowCount(0);
    statusBar()->showMessage("Таблица очищена", 2000);
}

void MainWindow::clearLastPacketInfo()
{
    lastPacketTimeLabel->setText("Время: -");
    lastDeviceIdLabel->setText("ID: -");
    lastDeviceTypeLabel->setText("Тип: -");
    lastDeviceVersionLabel->setText("Версия: -");
    lastPressureLabel->setText("Давление: -");
    lastTemperatureLabel->setText("Температура: -");
    lastVoltageLabel->setText("Напряжение: -");
    lastRssiLabel->setText("RSSI: -");
    lastTotalPacketsLabel->setText("Всего пакетов: -");
}

void MainWindow::updateLastPacketInfo(const QDateTime& time, const QString& deviceId,
                                      const QString& type, const QString& version,
                                      const QString& pressure, const QString& temperature,
                                      const QString& voltage, const QString& rssi,
                                      int totalPackets)
{
    lastPacketTimeLabel->setText("Время: " + time.toString("HH:mm:ss"));
    lastDeviceIdLabel->setText("ID: " + deviceId);
    lastDeviceTypeLabel->setText("Тип: " + type);
    lastDeviceVersionLabel->setText("Версия: " + version);
    lastPressureLabel->setText("Давление: " + pressure + " бар");
    lastTemperatureLabel->setText("Температура: " + temperature + "°C");
    lastVoltageLabel->setText("Напряжение: " + voltage + " В");
    lastRssiLabel->setText("RSSI: " + rssi);
    lastTotalPacketsLabel->setText("Всего пакетов: " + QString::number(totalPackets));
}

void MainWindow::updateStatisticsDisplay()
{
    // Обновляем статистику по типам устройств
    sensorCountLabel->setText(QString("Датчиков: %1").arg(sensorCount));
    repeaterCountLabel->setText(QString("Репитеров: %1").arg(repeaterCount));
    totalDevicesLabel->setText(QString("Всего устройств: %1").arg(sensorCount + repeaterCount));
    totalPacketsLabel->setText(QString("Всего пакетов: %1").arg(totalPacketCount));

    // Рассчитываем скорость приема (пакетов в секунду)
    double packetRate = 0;
    if (!packetTimestamps.isEmpty()) {
        QDateTime now = QDateTime::currentDateTime();
        QDateTime oldest = packetTimestamps.first();
        qint64 seconds = oldest.secsTo(now);
        if (seconds > 0) {
            packetRate = totalPacketCount / (double)seconds;
        }
    }

    packetRateLabel->setText(QString("Скорость приема: %1 пак/с").arg(packetRate, 0, 'f', 1));
}

void MainWindow::generateSummary()
{
    if (!isConnected) return;

    QDateTime now = QDateTime::currentDateTime();
    qint64 seconds = lastSummaryTime.secsTo(now);

    if (seconds > 0) {
        double periodRate = packetCountInPeriod / (double)seconds;

        periodPacketsLabel->setText(QString("Пакетов: %1").arg(packetCountInPeriod));
        periodUniqueLabel->setText(QString("Новых устройств: %1").arg(uniquePacketCountInPeriod));
        periodRateLabel->setText(QString("Скорость: %1 пак/с").arg(periodRate, 0, 'f', 1));

        // Сбрасываем счетчики периода
        packetCountInPeriod = 0;
        uniquePacketCountInPeriod = 0;
        lastSummaryTime = now;
    }
}