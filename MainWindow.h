#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QElapsedTimer>
#include <QMainWindow>
#include <QStatusBar>
#include "Enod.h"
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QMessageBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QMap>
#include <QList>

struct DevicePacketInfo {
    QString deviceId;
    QString type;
    QString version;
    QString pressure;
    QString temperature;
    QString voltage;
    QString rssi;
    QDateTime firstSeen;
    QDateTime lastSeen;
    int totalPacketCount;
};

class MainWindow : public QMainWindow
{
Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void refreshPorts();
    void connectToPort();
    void disconnectFromPort();
    void clearDisplay();
    void resetData();
    void onDataReceived(const QString& data);
    void updateStatisticsDisplay();
    void generateSummary();
    void updateClock();
    void updateConnectionIndicators();

private:
    void setupUI();
    void setupStatusBar();
    void addPacketToTable(const QString& packetKey, const DevicePacketInfo& info, bool isNewDevice);
    void updatePacketInTable(const QString& packetKey, const DevicePacketInfo& info);
    void clearLastPacketInfo();
    void updateLastPacketInfo(const QDateTime& time, const QString& deviceId,
                              const QString& type, const QString& version,
                              const QString& pressure, const QString& temperature,
                              const QString& voltage, const QString& rssi,
                              int totalPackets);

    // Элементы интерфейса
    QComboBox *portComboBox;
    QComboBox *speedComboBox;
    QPushButton *refreshButton;
    QPushButton *connectButton;
    QPushButton *disconnectButton;
    QPushButton *clearButton;
    QPushButton *resetButton;

    // Индикаторы
    QLabel *sensorIndicator;
    QLabel *repeaterIndicator;

    QTableWidget *dataTable;
    QLabel *statusBarLabel;

    QLabel *connectionStatusLabel;
    QLabel *portInfoLabel;
    QLabel *speedInfoLabel;

    // Статистика и время
    QLabel *currentTimeLabel;
    QLabel *sensorCountLabel;
    QLabel *repeaterCountLabel;
    QLabel *totalDevicesLabel;
    QLabel *totalPacketsLabel;
    QLabel *packetRateLabel;

    // Последний пакет (датчика)
    QLabel *lastPacketTimeLabel;
    QLabel *lastDeviceIdLabel;
    QLabel *lastDeviceTypeLabel;
    QLabel *lastDeviceVersionLabel;
    QLabel *lastPressureLabel;
    QLabel *lastTemperatureLabel;
    QLabel *lastVoltageLabel;
    QLabel *lastRssiLabel;
    QLabel *lastTotalPacketsLabel;

    // Сводка за период
    QLabel *periodPacketsLabel;
    QLabel *periodUniqueLabel;
    QLabel *periodRateLabel;

    // Данные и статистика
    Enod *enod;
    bool isConnected;

    // Статистика пакетов
    int uniquePacketCount;
    int totalPacketCount;
    int packetCountInPeriod;
    int uniquePacketCountInPeriod;
    QDateTime lastSummaryTime;
    QDateTime lastPacketTime;
    QDateTime lastRepeaterTime;

    // Статистика по типам устройств
    int sensorCount;
    int repeaterCount;
    int unknownCount;

    // Хранение данных
    QList<QDateTime> packetTimestamps;
    QMap<QString, int> devicePacketCount;
    QMap<QString, DevicePacketInfo> deviceDataMap;
    QMap<QString, DevicePacketInfo> repeaterDataMap;
};

#endif // MAINWINDOW_H