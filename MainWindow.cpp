#include "MainWindow.h"
#include <QtConcurrent/QtConcurrent>
#include <QTextCursor>

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent), enod(nullptr), isConnected(false)
{
    // Инициализация Enod
    enod = new Enod(this);  // Передаем this как родителя

    // Настройка главного окна
    setWindowTitle("Enod Data Monitor");
    setMinimumSize(1000, 700);

    // Создаем интерфейс
    setupUI();
    setupStatusBar();

    // Подключаем сигналы и слоты
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectToPort);
    connect(disconnectButton, &QPushButton::clicked, this, &MainWindow::disconnectFromPort);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearDisplay);

    // Подключаем сигнал от Enod - ИЗМЕНЯЕМ НА НОВЫЙ СИГНАЛ
    connect(enod, &Enod::newDataAvailable, this, &MainWindow::onDataReceived);

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

    QLabel *portLabel = new QLabel("Порт:", portControlGroup);
    portComboBox = new QComboBox(portControlGroup);
    portComboBox->addItem("Выберите порт...");

    QLabel *speedLabel = new QLabel("Скорость:", portControlGroup);
    speedComboBox = new QComboBox(portControlGroup);
    speedComboBox->addItems({"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"});
    speedComboBox->setCurrentText("115200");

    refreshButton = new QPushButton("Обновить список портов", portControlGroup);
    connectButton = new QPushButton("Подключиться", portControlGroup);
    disconnectButton = new QPushButton("Отключиться", portControlGroup);
    disconnectButton->setEnabled(false);
    clearButton = new QPushButton("Очистить лог", portControlGroup);

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
    portLayout->addStretch();

    // ========== ЦЕНТРАЛЬНАЯ ПАНЕЛЬ: Лог данных ==========
    QGroupBox *dataGroup = new QGroupBox("Лог данных", centralWidget);
    dataDisplay = new QTextEdit(dataGroup);
    dataDisplay->setReadOnly(true);
    dataDisplay->setPlaceholderText("Данные с COM-порта будут отображаться здесь...");

    QVBoxLayout *dataLayout = new QVBoxLayout(dataGroup);
    dataLayout->addWidget(dataDisplay);

    // ========== ПРАВАЯ ПАНЕЛЬ: Статус ==========
    QGroupBox *statusGroup = new QGroupBox("Статус", centralWidget);

    connectionStatusLabel = new QLabel("Статус: Не подключено", statusGroup);
    portInfoLabel = new QLabel("Порт: -", statusGroup);
    speedInfoLabel = new QLabel("Скорость: -", statusGroup);

    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
    statusLayout->addWidget(connectionStatusLabel);
    statusLayout->addWidget(portInfoLabel);
    statusLayout->addWidget(speedInfoLabel);
    statusLayout->addStretch();

    mainLayout->addWidget(portControlGroup, 1);
    mainLayout->addWidget(dataGroup, 2);
    mainLayout->addWidget(statusGroup, 1);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Готово к работе");
    statusBarLabel = new QLabel("Статус: Ожидание");
    statusBar()->addPermanentWidget(statusBarLabel);
}

void MainWindow::refreshPorts()
{
    portComboBox->clear();
    portComboBox->addItem("Выберите порт...");

    dataDisplay->append("Поиск доступных COM-портов...");

    // Используем временный Enod для поиска
    Enod tempEnod;
    int result = tempEnod.search_port();

    if (result >= 0 && tempEnod.port) {
        QString portName = QString(tempEnod.port);
        dataDisplay->append("✓ Найден порт: " + portName);
        portComboBox->addItem(portName);
        statusBar()->showMessage("Найден порт: " + portName, 3000);
    } else {
        dataDisplay->append("✗ COM-порты не обнаружены");
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

    dataDisplay->append("Подключение к порту: " + selectedPort);
    dataDisplay->append("Скорость: " + speedText + " бод");

    // Устанавливаем порт
    enod->port = selectedPort.toUtf8().constData();

    // Устанавливаем скорость в зависимости от выбранного значения
    if (speedText == "1200") {
        enod->speed_t = B1200;
    } else if (speedText == "2400") {
        enod->speed_t = B2400;
    } else if (speedText == "4800") {
        enod->speed_t = B4800;
    } else if (speedText == "9600") {
        enod->speed_t = B9600;
    } else if (speedText == "19200") {
        enod->speed_t = B19200;
    } else if (speedText == "38400") {
        enod->speed_t = B38400;
    } else if (speedText == "57600") {
        enod->speed_t = B57600;
    } else if (speedText == "115200") {
        enod->speed_t = B115200;
    } else {
        enod->speed_t = B115200; // По умолчанию
    }

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
    statusBarLabel->setText("Статус: Подключено");

    dataDisplay->append("✓ Успешно подключено");
    dataDisplay->append("Ожидание данных...\n");

    // Запускаем чтение порта в отдельном потоке
    QtConcurrent::run([this]() {
        enod->read_port();
    });
}

void MainWindow::disconnectFromPort()
{
    enod->stop();
    isConnected = false;

    connectButton->setEnabled(true);
    disconnectButton->setEnabled(false);
    portComboBox->setEnabled(true);
    speedComboBox->setEnabled(true);
    refreshButton->setEnabled(true);

    connectionStatusLabel->setText("Статус: Не подключено");
    portInfoLabel->setText("Порт: -");
    speedInfoLabel->setText("Скорость: -");

    statusBar()->showMessage("Отключено");
    statusBarLabel->setText("Статус: Ожидание");

    dataDisplay->append("✓ Соединение закрыто");
}

void MainWindow::onDataReceived(const QString& data)
{
    // Добавляем данные в лог с временной меткой
    QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss]");
    dataDisplay->append(timestamp + " " + data);

    // Прокручиваем вниз
    QTextCursor cursor = dataDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    dataDisplay->setTextCursor(cursor);
}

void MainWindow::clearDisplay()
{
    dataDisplay->clear();
    dataDisplay->append("Лог очищен " + QDateTime::currentDateTime().toString("[hh:mm:ss]"));
    statusBar()->showMessage("Лог очищен", 2000);
}