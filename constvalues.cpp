#include "constvalues.h"

const QMap<QString, QString> constValues::colorTranslate = QMap<QString, QString>({{"clWindowText", "black"}, {"clFuchsia", "#FF00FF"}, {"clOlive", "#808000"}, {"clMaroon", "#800000"}, {"clNavy", "#000080"}, {"clBlack", "black"}, {"clGreen", "green"}, {"clRed", "red"}, {"clBlue", "blue"}, {"clWhite", "white"}, {"clYellow", "yellow"}, {"clPurple", "purple"}, {"clSilver", "#C0C0C0"}, {"clBtnFaceColor", "#F0F0F0"}, {"clWindow", "transparent"}});
const QMap<unsigned char, QString> constValues::NUDirectives =
        QMap<unsigned char, QString>({
                                {0x0D, "ПОДКСОЕД"},
                                {0x0A, "СБР_ПОДКЛ"},
                                {0x01, "ПОДКЛ_М"},
                                {0x02, "ПОДКЛ_П"},
                                {0x09, "ИЗМ_СН"},
                                {0x08, "ИЗМ_СВ"},
                                {0x0B, "ПОДКЛ100"},
                                {0x06, "ИЗМ_НАПР"},
                                {0x20, "ПУЧОК"},
                                {0x21, "СОПР_П"},
                                {0x22, "ПОДК_1М"},
                                {0x23, "А_КОНТР"},
                                {0x66, "ПРОВЕРКА_ВОЛЬТМЕТРА"}
                            });

QAtomicInt constValues::isImitMode = 0;
QAtomicInt constValues::haveVnPr = 0;

QLocalSocket *timeControlSocket = nullptr;

QStringList getTimeWorkAppcp(){
    if (timeControlSocket->state() != QLocalSocket::ConnectedState){
        return QStringList();
    }
    QString message("get_time_work_appcp");

    timeControlSocket->write(message.toUtf8());
    timeControlSocket->flush();

    if (timeControlSocket->waitForReadyRead(3000)){
        QByteArray reply = timeControlSocket->readAll();
        QString replyMessage = QString::fromUtf8(reply);
        QStringList timeWork = replyMessage.split("##");
        if (timeWork.length() != 2 || timeWork[0].isEmpty() || timeWork[1].isEmpty()){
            return QStringList();
        }
        return timeWork;
    } else{
        return QStringList();
    }
}

QString ipAppcpServ = "";
int portAppcpWriteAndRead = 0x4567;
int portAppcpOnlyRead = 0x4568;

QAtomicInt constValues::isNeedCheckKS = 1;
