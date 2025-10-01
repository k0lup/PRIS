#include "mainwindow.h"
#include "manualmode.h"
#include "widgetinfo.h"
#include "commandline.h"
#include "directparser.h"
#include "directrunner.h"
#include <QtWidgets>

#include <QApplication>
#include <QMetaType>

#include "stepwgt.h"

#include "constvalues.h"
//запускаем контроль времени
//соединяемся с ним через LocalTCP
//при СП или закрытии запрашиваем время работы за тек. день, тек. месяц, тек. год

int main(int argc, char *argv[])
{
    static_assert(sizeof (float) == 4, "float must be 4 bytes!");
    qRegisterMetaType<directRunner::DIRECT_VARIABLE>("directRunner::DIRECT_VARIABLE");
    qRegisterMetaType<QTextCursor>("QTextCursor");
    qRegisterMetaType<QVector<int>>("QVector<int>");
    qRegisterMetaType<DirectParser::Direct>("DirectParser::Direct");
    qRegisterMetaType<directRunner::textStyle>("directRunner::textStyle");

    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("RKK Energia");
    QCoreApplication::setApplicationName("PRIS CROSSPLATFORM APP");
    MainWindow w;
    if (!w.statusOpenned) return -1;
    w.show();


    return a.exec();
}
