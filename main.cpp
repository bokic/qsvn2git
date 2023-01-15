#include "qmaindialog_old.h"
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
    QMainDialog_Old w;
    w.show();

    return a.exec();
}
