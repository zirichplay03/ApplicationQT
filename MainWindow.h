#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Enod.h"
#include <QDateTime>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QMessageBox>
#include <QApplication>
#include <QThread>

class ReadThread : public QThread {
Q_OBJECT
public:
    ReadThread(Enod* enod) : enod(enod), running(false) {}
    void stop() { running = false; enod->stop_flag = true; }

signals:
    void dataReceived(const QString& data);

protected:
    void run() override {
        running = true;
        enod->read_port();  // Запускаем read_port из Enod
    }

private:
    Enod* enod;
    bool running;
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
    void onDataReceived(const QString& data);

private:
    void setupUI();
    void setupStatusBar();

    // Элементы интерфейса
    QComboBox *portComboBox;
    QComboBox *speedComboBox;
    QPushButton *refreshButton;
    QPushButton *connectButton;
    QPushButton *disconnectButton;
    QPushButton *clearButton;
    QTextEdit *dataDisplay;
    QLabel *connectionStatusLabel;
    QLabel *portInfoLabel;
    QLabel *speedInfoLabel;
    QLabel *statusBarLabel;

    // Объекты для работы
    Enod *enod;
    ReadThread *readThread;
    bool isConnected;
};

#endif // MAINWINDOW_H