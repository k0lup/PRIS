#include "mainwindow.h"
#include "manualmode.h"
#include "widgetinfo.h"
#include "commandline.h"
#include "directparser.h"
#include "directrunner.h"
#include <QtWidgets>

#include <QApplication>
#include <QMessageBox>
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
    const QString lockDir =
            QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation).isEmpty()
            ? QDir::tempPath()
            : QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);

    QDir().mkpath(lockDir);

    QLockFile lock(lockDir + "/myapp.lock");
    lock.setStaleLockTime(0); // 0 = Qt сам решает, когда считать lock "протухшим"

    if (!lock.tryLock(0)) {
        QMessageBox::critical(nullptr, "Ошибка", "Приложение уже запущено!");
        return 0;
    }
    MainWindow w;
    if (!w.statusOpenned) return -1;
    w.show();


    return a.exec();
}
