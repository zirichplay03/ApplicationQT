#include <QApplication>
#include "MainWindow.h"
#include "Enod.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
//Enod enod;
//enod.read_port();
}