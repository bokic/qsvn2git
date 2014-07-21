#include "qmaindialog.h"
#include <QApplication>
#include <QMetaType>

void registerTypes()
{
    qRegisterMetaType<QHash<QString, QString> >("QHash<QString, QString>");
}

int main(int argc, char *argv[])
{
    registerTypes();

    QApplication a(argc, argv);
    QMainDialog w;
    w.show();

    return a.exec();
}
