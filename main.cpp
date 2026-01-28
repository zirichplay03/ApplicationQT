#include <QApplication>
#include <QWidget>
#include <QLabel>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Qt 6.7.0 Test");
    window.resize(400, 300);

    QLabel *label = new QLabel("Success! Using Qt 6.7.0", &window);
    label->setAlignment(Qt::AlignCenter);
    label->setGeometry(100, 100, 200, 50);

    window.show();

    return app.exec();
}