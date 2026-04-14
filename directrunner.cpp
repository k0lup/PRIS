#include "directrunner.h"
#include <QDateTime>
#include <QtWidgets>
#include <QThread>
#include <QDataStream>
#include <cstring>

#include "mainwindow.h"
#include "constvalues.h"
#include "rrparam.h"


struct FileHeaderInfo
{
    QString firstLine;
    QString checksum;
    QString encoding;   // "UTF-8" / "CP1251" / ""
    bool ok = false;
};

FileHeaderInfo readDipHeader(const QString &fileName)
{
    FileHeaderInfo result;

    const QString prefix = QStringLiteral("П!КОНТРОЛЬНАЯ СУММА=");

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Open error:" << file.errorString();
        return result;
    }

    // Читаем первую строку как сырые байты
    QByteArray rawLine = file.readLine();
    rawLine = rawLine.trimmed();   // убрать \r\n

    // 1. Пробуем UTF-8
    QString lineUtf8 = QString::fromUtf8(rawLine);
    if (lineUtf8.startsWith(prefix) && lineUtf8.endsWith('!')) {
        result.firstLine = lineUtf8;
        result.checksum = lineUtf8.mid(prefix.length());
        result.encoding = QStringLiteral("UTF-8");
        result.ok = true;
        return result;
    }

    // 2. Пробуем CP1251
    QTextCodec *cp1251 = QTextCodec::codecForName("Windows-1251");
    if (cp1251) {
        QString lineCp1251 = cp1251->toUnicode(rawLine);
        if (lineCp1251.startsWith(prefix) && lineCp1251.endsWith('!')) {
            result.firstLine = lineCp1251;
            result.checksum = lineCp1251.mid(prefix.length());
            result.encoding = QStringLiteral("CP1251");
            result.ok = true;
            return result;
        }
    }

    // Ничего не подошло
    qDebug() << "Unknown encoding or invalid header. Raw bytes:" << rawLine;
    return result;
}

QStack<directRunner::programStruct> directRunner::programs = QStack<directRunner::programStruct>();
QString directRunner::metka = "";
bool directRunner::hasRunProg = false;
bool directRunner::stopProg = false;
QQueue<DirectParser::Direct> directRunner::command = QQueue<DirectParser::Direct>();
directRunner::DIRECT_VARIABLE directRunner::dirVar = directRunner::DIRECT_VARIABLE::EMTY_VAR;

QMap<QString, QMap<QString, QString>> directRunner::styles = QMap<QString, QMap<QString, QString>>();

QList<int> directRunner::directNumPotok = {0, -1, 0, 0, -1, 2, 1, -1, 0, 0, 6, 1, -1, 8, 0, 0, -1, 0, 1, 0, -1, 0, 0, 2, 0, 0, -1, 0, 0, -1, 0};
struct strNUErrorVal{
    QString errorMessage;
    bool needByte;
    strNUErrorVal(const QString& errorMessage, bool needByte = false) : errorMessage(errorMessage), needByte(needByte){}
    strNUErrorVal(){}
};

const QMap<int, strNUErrorVal> NUErrorCodeValue = {{0, strNUErrorVal("ошибок нет")}, {1, strNUErrorVal("ошибка при расчете Контрольной Суммы")}, {2, strNUErrorVal("ошибка входных параметров (КС - в норме)")}, {3, strNUErrorVal("ошибка при подкл./откл. точки. №точки: ", true)},
                                                   {4, strNUErrorVal("зашкал по диапазону вниз")}, {5, strNUErrorVal("зашкал по диапазону вверх")}, {6, strNUErrorVal("при попытке измерения - есть наличие напряжения между шинами '+' и '-'")},
                                                   {7, strNUErrorVal("при выдержке времени tздр. - сопротивление упало. tздр=", true)}, {8, strNUErrorVal("не удалось достичь испытательного напряжения")}, {9, strNUErrorVal("к шине '+' подключены точки")},
                                                   {10, strNUErrorVal("ошибка при подключении регистра 2 уровня коммутации. № платы коммутации: ", true)}, {11, strNUErrorVal("указанный массив точек не был полностью обработан")}, {12, strNUErrorVal("указанный массив точек был полностью обработан, но в процессе работы были обнаружены внештатные ситуации")},
                                                   {13, strNUErrorVal("к шине '-' не было подключено ни одной точки, либо только одна точка")}, {14, strNUErrorVal("ошибка при контроле вторичных электропитаний. Код ошибки: ", true)}, {15, strNUErrorVal("ошибка при определении закороток. № точки: ", true)},
                                                   {16, strNUErrorVal("ошибка при определении разрыва. № точки: ", true)}, {17, strNUErrorVal("ошибка внутренней периферии ")}, {18, strNUErrorVal("ошибка тестирования коммутатора")}, {19, strNUErrorVal("сброс по сторожевому таймеру")},
                                                   {20, strNUErrorVal("таймаут при работе с интерфейсом 12С. Номер регистра: ", true)}, {21, strNUErrorVal("таймаут при приёме команды от НУ")}, {22, strNUErrorVal("выбранная команда отсутствует")}, {23, strNUErrorVal("пропущены байты при приёме команды")},
                                                   {24, strNUErrorVal("автоконтроль был завершён неуспешно (предупреждение)")}, {255, strNUErrorVal("незарегистрированная ошибка")}};

const QMap<char, QString> voltErrorCode = {{1, QString("Ошибка при подсчете контрольной суммы")},
                                           {2, QString("Ошибка входных параметров")},
                                           {4, QString("Зашкал по диапазону вниз")},
                                           {5, QString("Зашкал по диапазону вверх")},
                                           {20, QString("Таймаут или нет связи с вольтметром")},
                                           {22, QString("Выбранная команда отсутствует")},
                                           {255, QString("Незарегистрированная ошибка (резерв)")}};

qint32 CirSum(qint32 x,qint32 y)
{
    int nXHigh = (x & 0xffff0000) >> 16;
    int nXLow = x & 0x0000ffff;
    int nYHigh = (y & 0xffff0000) >> 16;
    int nYLow = y & 0x0000ffff;
    bool bCarry = false;

    // сложим мл. байты
    int nLow = nXLow + nYLow;
    // если переполнение
    if (nLow > 0x0000ffff) {
        // удалим переполнение
        nLow = nLow & 0x0000ffff;
        // флаг переполнения
        bCarry = true;
    }
    // сложим ст. байты
    int nHigh = nXHigh + nYHigh + (bCarry ? 1 : 0);
    bCarry = false;
    // если переполнение
    if (nHigh > 0x0000ffff) {
        // удалим переполнение
        nHigh = nHigh & 0x0000ffff;
        // флаг переполнения
        bCarry = true;
    }
    // если переполнение при сложении старших разрядов, то +1 к младшему
    nLow += bCarry ? 1 : 0;
    // удалим переполнение, если есть
    nLow = nLow & 0x0000ffff;
    // вернем nHigh nLow
    return ((nHigh << 16) + nLow);
}

QString getKS(QByteArray readData, bool needRemoveKS = true){
    //выполняем только в ОС Windows
    readData.replace(char(0x0A), QByteArray().append(char(0x0D)).append(char(0x0A)));
    if (needRemoveKS){
        //добавить проверку на ОС и на кодировку файла
        readData = readData.mid(31);
    }

    QByteArray tempVal;
    tempVal.append(char(0)).append(char(0)).append(char(0)).append(char(0));
    qint32 KS {0};
    qint32 val {0};
    for (int i = 0; i < readData.count() + 1; i+=4){
        for (int j = 0; j < (readData.count() - i > 4 ? 4 : readData.count() - i); j++){
            tempVal[3 - j] = readData[i + j];
        }
        QDataStream stream(tempVal);
        stream >> val;
        KS = CirSum(KS, val);
    }

    QByteArray exitValue;


    exitValue.append(char(KS & 0x000000FF)).append(char((KS & 0x0000FF00) >> (8 * 1))).append(char((KS & 0x00FF0000) >> (8 * 2))).append(char((KS & 0xFF000000) >> (8 * 3)));

    QString KSString;

    for (int i = exitValue.length() - 1; i >= 0; i--){
        KSString.append(QString::number((exitValue[i] & 0xF0) >> 4, 16)).append(QString::number((exitValue[i] & 0x0F), 16));
    }
    return KSString;
}

QString replaceTabulation(const QString& input, int tabSize = 4){
    QStringList lines = input.split("\n");
    QString result;

    for (auto line : lines){
        QString newLine;
        int pos = 0;

        for (QChar ch: line){
            if (ch == '\t'){
                int spacesToAdd = tabSize - (pos % tabSize);
                newLine += QString(spacesToAdd, ' ');
                pos += spacesToAdd;
            } else{
                newLine += ch;
                pos += 1;
            }
        }
        result += newLine + '\n';
    }
    result.chop(1);
    return result;
}
void directRunner::printStartMessage(){
    printInProt(QString("------------СТАРТ ПРИС---------------"), "0", textStyle());
    printInProt(QDateTime::currentDateTime().toString("dd.MM.yyyy") + " " + QDateTime::currentDateTime().toString("HH:mm:ss") + " " + MainWindow::getOnFilePath() + " " + MainWindow::getCfgFilePath(), "0", textStyle());
    printInProt(QString("Каталог загрузки:   ") + QCoreApplication::applicationDirPath(), "0", textStyle());
    printInProt(QString("Текущий каталог:    " + MainWindow::getProgramCatalog().value(0)), "0", textStyle());
    printInProt(QString("Версия ПРИС:        ") + MainWindow::getCfgParam("НОМЕР_ВЕРСИИ"), "0", textStyle());
    printInProt(QString("КА:                 ") + MainWindow::getCfgParam("КОСМИЧЕСКИЙ_АППАРАТ"), "0", textStyle());
    printInProt(QString("-------------------------------------"), "0", textStyle());
}
void directRunner::startWork(){

    printStartMessage();

    if (MainWindow::getCfgParam("JSON_MESSAGE").toUpper() == "TRUE"){
        can2RR = true;
        jsonReceiver = new JsonReceiver(socketCanal2, this);
        QObject::connect(jsonReceiver, &JsonReceiver::messageGet, [this](portBMessage messageStruct){
            if (messageStruct.type == 0) printInProt(QString("%1 : (%2) %3:%4:%5.(%6)  ( %7)").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(messageStruct.type).arg(messageStruct.hour, 2, 10, QChar('0')).arg(messageStruct.min, 2, 10, QChar('0')).arg(messageStruct.sec, 2, 10, QChar('0')).arg(int(messageStruct.ms)/*, 3, 10, QChar('0')*/).arg(messageStruct.text), "22", textStyle());
            else{
                int code = messageStruct.code;
                int nextByte = -1;
                if (!NUErrorCodeValue.contains(code)) code = 255;
                QString errorMessage = NUErrorCodeValue.value(code).errorMessage;
                if (NUErrorCodeValue.value(code).needByte) errorMessage.append(QString(" %1").arg(int(nextByte)));
                printInProt(QString("%1 : (%2) %3:%4:%5.(%6) %7 ( %8)").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(messageStruct.code).arg(messageStruct.hour, 2, 10, QChar('0')).arg(messageStruct.min, 2, 10, QChar('0')).arg(messageStruct.sec, 2, 10, QChar('0')).arg(messageStruct.ms/*, 3, 10, QChar('0')*/).arg(errorMessage).arg(messageStruct.text), "22", textStyle());
            }
            emit socketRRMes();
        });

        QObject::connect(jsonReceiver, &JsonReceiver::errorGet, [this](QString message){
            printInProt(QString("\t\t\t%1\tОшибка получения данных по порту %2: ").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(portAppcpOnlyRead, 8, 16, QChar('0')) + message, "13", textStyle());
        });

        QObject::connect(jsonReceiver, &JsonReceiver::bytesGet, [this](QByteArray bytes){
            QByteArray response = bytes;
            printInProt(QString("%1 NETCL: получено %2 байтов").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(response.length()), "13", textStyle(), true);
            printInProt(QString("\t\t\tПолучено сообщ. port = %2 len = %1").arg(response.length()).arg(portAppcpOnlyRead, 8, 16, QChar('0')), "23", textStyle(), true);
            printInProt(QString("\t\t\tПолучено СООБЩ (print_soob)"), "23", textStyle(), true);
            QStringList byteList;
            int count = 0;
            for (unsigned char byte: response){
                QString curByte;
                if (count == 0) curByte.append("\t\t\t");
                count += 1;
                curByte.append(QString("0x") + QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
                if (count >= 16){
                    curByte += "\n";
                    count = 0;
                }
                byteList << curByte;
            }
            byteList.first().prepend(" ");
            if (byteList.last().at(byteList.last().length()-1) == '\n') byteList.last().chop(1);
            printInProt(byteList.join(" "), "23", textStyle(), true);
        });
    } else {
        QObject::connect(socketCanal2, &QTcpSocket::readyRead, [this](){
            can2RR = true;
           QByteArray response = socketCanal2->readAll();
           printInProt(QString("%1 NETCL: получено %2 байтов").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(response.length()), "13", textStyle(), true);
           printInProt(QString("\t\t\tПолучено сообщ. port = %2 len = %1").arg(response.length()).arg(portAppcpOnlyRead, 8, 16, QChar('0')), "23", textStyle(), true);
           printInProt(QString("\t\t\tПолучено СООБЩ (print_soob)"), "23", textStyle(), true);
           QStringList byteList;
           int count = 0;
           for (unsigned char byte: response){
               QString curByte;
               if (count == 0) curByte.append("\t\t\t");
               count += 1;
               curByte.append(QString("0x") + QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
               if (count >= 16){
                   curByte += "\n";
                   count = 0;
               }
               byteList << curByte;
           }
           byteList.first().prepend(" ");
           if (byteList.last().at(byteList.last().length()-1) == '\n') byteList.last().chop(1);
           printInProt(byteList.join(" "), "23", textStyle(), true);

           if (response.length() != 93){
               printInProt(QString("Ошибка в размере сообщения по порту %1! Ожидалось 93 байта, получено: %2 байт!").arg(portAppcpOnlyRead, 8, 16, QChar('0')).arg(response.length()), "13", textStyle());
               emit this->socketRRMes();
               return;
           }

           QByteArray slice = response.mid(13);
           QByteArray messageBA;
           for (unsigned char byte: slice){
               if (byte == 0) continue;
               messageBA.append(byte);
           }
           if (response[11] == char(0)) printInProt(QString("%1 : (%2) %3:%4:%5.(%6)  ( %7)").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(int(response[0])).arg(response[8], 2, 10, QChar('0')).arg(response[7], 2, 10, QChar('0')).arg(response[10], 2, 10, QChar('0')).arg(int(response[9])/*, 3, 10, QChar('0')*/).arg(codec->toUnicode(messageBA)), "22", textStyle());
           else{
               int code = response[11];
               int nextByte = response[12];
               if (!NUErrorCodeValue.contains(code)) code = 255;
               QString errorMessage = NUErrorCodeValue.value(code).errorMessage;
               if (NUErrorCodeValue.value(code).needByte) errorMessage.append(QString(" %1").arg(int(nextByte)));
               printInProt(QString("%1 : (%2) %3:%4:%5.(%6) %7 ( %8)").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(int(response[0])).arg(response[8], 2, 10, QChar('0')).arg(response[7], 2, 10, QChar('0')).arg(response[10], 2, 10, QChar('0')).arg(int(response[9])/*, 3, 10, QChar('0')*/).arg(errorMessage).arg(codec->toUnicode(messageBA)), "22", textStyle());
           }
           emit this->socketRRMes();
        });
    }
    QObject::connect(socketCanal1, &QTcpSocket::connected, [this](){
        if (socketCanal2->state() == QAbstractSocket::ConnectedState) this->hasConnectNU.store(1);
        printInProt(QString("%1\tСвязь установлена с НУ по каналу 1").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "23", textStyle());
    });
    QObject::connect(socketCanal2, &QTcpSocket::connected, [this](){
        if (socketCanal1->state() == QAbstractSocket::ConnectedState) this->hasConnectNU.store(1);
       printInProt(QString("%1\tСвязь установлена с НУ по каналу 2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "23", textStyle());
    });

    QObject::connect(socketCanal1, &QTcpSocket::disconnected, [this](){
        this->hasConnectNU.store(0);
       printInProt(QString("%1\tСвязь с НУ по каналу 1 разорвана").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "13", textStyle());
    });
    QObject::connect(socketCanal2, &QTcpSocket::disconnected, [this](){
        this->hasConnectNU.store(0);
       printInProt(QString("%1\tСвязь с НУ по каналу 2 разорвана").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "13", textStyle());
    });

    connectNU();

    QObject::connect(socketCanal1, &QTcpSocket::readyRead, [this](){
        can1RR = true;
        QByteArray response = this->socketCanal1->readAll();
        this->respondNU = response;
        printInProt(QString("%1 NETCL: получено %2 байтов").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(response.length()), "13", textStyle(), true);
        printInProt(QString("\t\t\tПолучено сообщ. port = %2 len = %1").arg(response.length()).arg(portAppcpWriteAndRead, 8, 16, QChar('0')), "23", textStyle(), true);
        QStringList byteList;
        int count = 0;
        for (unsigned char byte: response){
            QString curByte;
            if (count == 0) curByte.append("\t\t\t");
            count += 1;
            curByte.append(QString("0x") + QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
            if (count >= 16){
                curByte += "\n";
                count = 0;
            }
            byteList << curByte;
        }
        byteList.first().prepend(" ");
        if (byteList.last().at(byteList.last().length()-1) == '\n') byteList.last().chop(1);
        printInProt(byteList.join(" "), "23", textStyle(), true);
        if (this->waitNUMessage.load() == 0){
            printInProt("Получен ответ без запроса", "13", textStyle());
        }
        emit this->socketRRMes();
    });
}

bool isParamExists(const QString& block, const QString& name, QString& errorString){
    errorString = "";
    QString queryString = QString("SELECT COUNT(*) FROM RR_PAR WHERE Bl_Name = '%1' AND Par_Name = '%2'").arg(block).arg(name);
    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        errorString.append("ОШИБКА ОБРАЩЕНИЯ К БД РР ПАРАМЕТРОВ: " + query.lastError().text());
        return false;
    }
    if (!query.next()){
        errorString.append("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        return false;
    }
    bool ok{false};
    int count = query.value(0).toInt(&ok);
    if (!ok){
        errorString.append("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        return false;
    }
    if (count == 0) return false;
    else return true;
}

bool isParamArray(const QString& block, const QString& name, QString& errorString){
    errorString = "";
    QString queryString = QString("SELECT MAX(Index) FROM RR_PAR WHERE Bl_Name = '%1' AND Par_Name = '%2'").arg(block).arg(name);
    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        errorString.append("ОШИБКА ОБРАЩЕНИЯ К БД РР ПАРАМЕТРОВ: " + query.lastError().text());
        return false;
    }
    if (!query.next()){
        errorString.append("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        return false;
    }
    if (query.value(0).isNull()) return false;
    bool ok{false};
    int value = query.value(0).toInt(&ok);
    if (!ok){
        errorString.append("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        return false;
    }
    return true;
}

int getParamArrayLength(const QString& block, const QString& name, QString& errorString){
    errorString = "";
    QString queryString = QString("SELECT MAX(Index) FROM RR_PAR WHERE Bl_Name = '%1' AND Par_Name = '%2'").arg(block).arg(name);
    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        errorString.append("ОШИБКА ОБРАЩЕНИЯ К БД РР ПАРАМЕТРОВ: " + query.lastError().text());
        return -1;
    }
    if (!query.next()){
        errorString.append("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        return -1;
    }
    if (query.value(0).isNull()) return false;
    bool ok{false};
    int value = query.value(0).toInt(&ok);
    if (!ok){
        errorString.append("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        return -1;
    }
    return value + 1;
}

float getParamValue(const QString& block, const QString& name, const int index, QString& errorString){
    errorString = "";

    if (!isParamExists(block, name, errorString) || !errorString.isEmpty()){
        if (errorString.isEmpty()) errorString.append("Не удалось найти РР параметр с заданным именем");
        return false;
    }
    if (index != -1){
        if (!isParamArray(block, name, errorString) || !errorString.isEmpty()){
            if (errorString.isEmpty()) errorString.append("РР параметр не является массивом. Обращение по индексу недопустимо");
            return false;
        }
        if (index >= getParamArrayLength(block, name, errorString) || !errorString.isEmpty()){
            if (errorString.isEmpty()) errorString.append("Указанный индекс выходит за границы массива РР параметра");
            return false;
        }
    } else{
        if (isParamArray(block, name, errorString) || !errorString.isEmpty()){
            if (errorString.isEmpty()) errorString = "РР параметр не является массивом";
            return false;
        }
    }

    float value = 0.0;

    QString queryString = QString("SELECT Val FROM RR_PAR WHERE Bl_Name = '%1' AND Par_Name = '%2'").arg(block).arg(name);
    if (index != -1) queryString.append(QString(" AND Index = %3").arg(QString::number(index)));
    else queryString.append(QString(" AND Index IS NULL"));

    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        errorString.append("ОШИБКА ОБРАЩЕНИЯ К БД РР ПАРАМЕТРОВ: " + query.lastError().text());
        return 0;
    }
    if (!query.next()){
        errorString.append("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        return 0;
    }
    bool ok = false;
    value = query.value(0).toFloat(&ok);
    if (!ok){
        errorString.append("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        return 0;
    }
    return value;
}

float getParamValue(const QString& param, QString& errorString){
    errorString = "";
    QRegularExpression regex(R"(^FL.([A-Za-z0-9]{1,3})_?([A-Za-z0-9_\-\/\.]{1,15})?(?:\[(\d+)\])?$)");
    //QRegularExpression regex(R"(FL\.([A-Za-z0-9]{1,3})_?([A-Za-z0-9]{1,8})?(?:\[(\d+)\])?(?:\((\d+)\))?)");
    QRegularExpressionMatch match = regex.match(param);

    if (!match.hasMatch()){
        errorString = "Не удалось распознать имя РР параметра";
        return false;
    }

    QString block = match.captured(1);
    QString name = match.captured(2);
    int index{-1};
    if (!match.captured(3).isEmpty()){
        bool ok{false};
        index = match.captured(3).toInt(&ok);
        if (index < 0 || !ok){
            errorString = "Некорректный индекс РР параметра";
            return false;
        }
    }

    return getParamValue(block, name, index, errorString);
}

bool setParamValue(const QString& block, const QString& name, const int index, const float value, QString& errorString){
    errorString = "";

    if (!isParamExists(block, name, errorString) || !errorString.isEmpty()){
        if (errorString.isEmpty()) errorString.append("Не удалось найти РР параметр с заданным именем");
        return false;
    }
    if (index != -1){
        if (!isParamArray(block, name, errorString) || !errorString.isEmpty()){
            if (errorString.isEmpty()) errorString.append("РР параметр не является массивом. Обращение по индексу недопустимо");
            return false;
        }
        if (index >= getParamArrayLength(block, name, errorString) || !errorString.isEmpty()){
            if (errorString.isEmpty()) errorString.append("Указанный индекс выходит за границы массива РР параметра");
            return false;
        }
    } else{
        if (isParamArray(block, name, errorString) || !errorString.isEmpty()){
            if (errorString.isEmpty()) errorString = "РР параметр не является массивом";
            return false;
        }
    }

    QString queryString;
    if (index == -1) queryString = QString("UPDATE RR_PAR SET Val = %1 WHERE Bl_Name = '%2' AND Par_Name = '%3'").arg(value).arg(block).arg(name);
    else queryString = QString("UPDATE RR_PAR SET Val = %1 WHERE Bl_Name = '%2' AND Par_Name = '%3' AND Index = %4").arg(value).arg(block).arg(name).arg(index);

    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        errorString.append("ОШИБКА ОБРАЩЕНИЯ К БД РР ПАРАМЕТРОВ: " + query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() <= 0){
        errorString.append("ОШИБКА ЗАПИСИ РЕЗУЛЬТАТА В БД");
        return false;
    }
    query.clear();
    return true;
}

bool setParamValue(const QString& param, const float value, QString& errorString){
    errorString = "";
    QRegularExpression regex(R"(^FL.([A-Za-z0-9]{1,3})_?([A-Za-z0-9_\-\/\.]{1,15})?(?:\[(\d+)\])?$)");
    //QRegularExpression regex(R"(FL\.([A-Za-z0-9]{1,3})_?([A-Za-z0-9]{1,8})?(?:\[(\d+)\])?(?:\((\d+)\))?)");
    QRegularExpressionMatch match = regex.match(param);

    if (!match.hasMatch()){
        errorString = "Не удалось распознать имя РР параметра";
        return false;
    }

    QString block = match.captured(1);
    QString name = match.captured(2);
    int index{-1};
    if (!match.captured(3).isEmpty()){
        bool ok{false};
        index = match.captured(3).toInt(&ok);
        if (index < 0 || !ok){
            errorString = "Некорректный индекс РР параметра";
            return false;
        }
    }

    return setParamValue(block, name, index, value, errorString);
}

bool isValidParam(const QString& param, QString& errorString){
    errorString = "";
    QRegularExpression regex(R"(^FL.([A-Za-z0-9]{1,3})_?([A-Za-z0-9_\-\/\.]{1,15})?(?:\[(\d+)\])?$)");
    //QRegularExpression regex(R"(FL\.([A-Za-z0-9]{1,3})_?([A-Za-z0-9]{1,8})?(?:\[(\d+)\])?(?:\((\d+)\))?)");
    QRegularExpressionMatch match = regex.match(param);

    if (!match.hasMatch()){
        errorString = "Не удалось распознать имя РР параметра";
        return false;
    }

    QString block = match.captured(1);
    QString name = match.captured(2);
    int index{-1};
    if (!match.captured(3).isEmpty()){
        bool ok{false};
        index = match.captured(3).toInt(&ok);
        if (index < 0 || !ok){
            errorString = "Некорректный индекс РР параметра";
            return false;
        }
    }

    if (!isParamExists(block, name, errorString) || !errorString.isEmpty()){
        if (errorString.isEmpty()) errorString.append("Не удалось найти РР параметр с заданным именем");
        return false;
    }
    if (index != -1){
        if (!isParamArray(block, name, errorString) || !errorString.isEmpty()){
            if (errorString.isEmpty()) errorString.append("РР параметр не является массивом. Обращение по индексу недопустимо");
            return false;
        }
        if (index >= getParamArrayLength(block, name, errorString) || !errorString.isEmpty()){
            if (errorString.isEmpty()) errorString.append("Указанный индекс выходит за границы массива РР параметра");
            return false;
        }
    } else{
        if (isParamArray(block, name, errorString) || !errorString.isEmpty()){
            if (errorString.isEmpty()) errorString = "РР параметр не является массивом";
            return false;
        }
    }

    return true;
}




const QString ID_TEMPLATE = "(FL.&lt;блок&gt;_&lt;имя&gt; )";

QString findFileRecursive(const QString &directoryPath, const QString& fileName){
    QDirIterator it(directoryPath, QDirIterator::Subdirectories);
    while (it.hasNext()){
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        if (fileInfo.isFile() && fileInfo.fileName() == fileName){
            return filePath;
        }
    }
    return QString();
}

void directRunner::sendMessageToNU(const char *data, int len, bool *status){
    respondNU.clear();
    *status = false;
    if (blockAllIsm) return;
    QByteArray bytes;
    for (int i = 0; i < len; ++i){
        bytes.append(data[i]);
    }
    QStringList byteList;
    int count = 0;
    for (unsigned char byte: bytes){
        count += 1;
        QString curByte = QString("0x") + QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
        if (count >= 16){
            curByte += "\n";
            count = 0;
        }
        byteList << curByte;
    }
    byteList.first().prepend(" ");
    if (byteList.last().at(byteList.last().length()-1) == '\n') byteList.last().chop(1);
    printInProt(QString("%1 Посылаем запрос в НУ kom = 0x%2 len = %3\nЗапрос: %4").arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(data[0], 2, 16, QChar('0')).arg(len).arg(byteList.join(" ")), "27", textStyle(), true);

    int waitTime = 2000;
    if (data[0] == char(0x0D)) waitTime += int(data[1]) * 1000 + 1000; //добавляем 1 сек, так как ПО НУ не справляется за 2 сек. + задержка
    else if (data[0] == char(0x23)){
        if (data[1] == char(1)) waitTime += 120000;
        else if (data[1] == char(2)) waitTime += 120000;
        else if (data[1] == char(3)) waitTime += 360000;
        else if (data[1] == char(4)) waitTime += 120000;
    }
    else if (data[0] == char(0x08)) waitTime += int(data[3]) * 1000;
    else if (data[0] == char(0x20)) waitTime += int(data[2]) * 3000;
    else if (data[0] == char(0x21)){
        waitTime += (int(data[3]) + 2) * 1000 * int(data[4]);
    }
    printInProt(QString("\t\t\tOUTNU:\tЗадержка - %1 msec").arg(waitTime), "0", textStyle(), true, false);
    waitNUMessage.store(1);

    can1RR = false;
    can2RR = false;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QMetaObject::Connection con1 = QObject::connect(this, &directRunner::socketRRMes, [&loop, this](){
        if (this->can1RR /*&& this->can2RR*/) loop.quit();
    });
    QMetaObject::Connection con2 = QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QMetaObject::Connection con3 = QObject::connect(this, &directRunner::stStopRequested, &loop, &QEventLoop::quit);

    socketCanal1->write(data, len);
    socketCanal1->flush();

    timer.start(waitTime);
    loop.exec();

    timer.stop();
    QObject::disconnect(con1);
    QObject::disconnect(con2);
    QObject::disconnect(con3);



    if (!can1RR /*|| !can2RR*/){
        waitNUMessage.store(0);
        QString canInfo;
        //if (!can1RR && !can2RR) canInfo = "по 1 и 2 каналу";
        /*else*/ if (!can1RR) canInfo = "по 1 каналу";
        //else canInfo = "по 2 каналу";
        if (ost_flag.load() == 1) {
            printInProt(QString("%1\tОЖИДАНИЕ ОТВЕТА ОТ НУ ОСТАНОВЛЕНО ПО СРОСТ (=НЕ ПОЛУЧЕН ОТВЕТ ОТ НУ)").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "13", textStyle());
        } else {
            printInProt(QString("%1\tПревышен лимит ожидания ответа от НУ %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(canInfo), "13", textStyle());
        }
        return;
    }
    waitNUMessage.store(0);
    if (this->respondNU.length() < 2 || this->respondNU.at(0) != data[0]){
        printInProt(QString("%1\tОтвет от НУ получен на другую директиву").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "13", textStyle());
        return;
    }
    if (this->respondNU[1] != char(0x00) && this->respondNU[1] != char(0xFF)){
        printInProt(QString("%1\tОшибка в коде заверешения операции в НУ (допустимые коды завершения: 0x00 и 0xFF)").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "13", textStyle());
        return;
    }

    *status = true;
    return;
}
void directRunner::printInProt(const QString& text, const QString &styleName, const textStyle &styleNotUse, bool nuMessage, bool onlyNUFile){
    QString textMessage{text};
    if (text.length() > 0 && text.right(1) == "\n"){
        textMessage.chop(1);
    }
    QString textForWgt;
    QString textForProt;

    textStyle styleTest(-1);
    styleTest.potok = styleName.toInt();
    if (styles.contains(QString("Style") + styleName) && styles.value(QString("Style") + styleName).contains("Color")) styleTest.color = styles.value(QString("Style") + styleName).value("Color");
    else{
        styleTest.color = "clWindowText";
    }

    if (styles.contains(QString("Style") + styleName) && styles.value(QString("Style") + styleName).contains("BackColor")) styleTest.backColor = styles.value(QString("Style") + styleName).value("BackColor");
    else{
        styleTest.backColor = "clWindow";
    }

    if (styles.contains(QString("Style") + styleName) && styles.value(QString("Style") + styleName).contains("FontName")) styleTest.fontName = styles.value(QString("Style") + styleName).value("FontName");
    else{
        styleTest.fontName = "Courier New";
    }

    if (styles.contains(QString("Style") + styleName) && styles.value(QString("Style") + styleName).contains("Size")) styleTest.fontSize = styles.value(QString("Style") + styleName).value("Size").toInt();
    else{
        styleTest.fontSize = 10;
    }

    if (styles.contains(QString("Style") + styleName) && styles.value(QString("Style") + styleName).contains("Charset")) styleTest.charSet = styles.value(QString("Style") + styleName).value("Charset").toInt();
    else{
        styleTest.charSet = 1;
    }

    if (styles.contains(QString("Style") + styleName) && styles.value(QString("Style") + styleName).contains("Bold")) styleTest.bold = styles.value(QString("Style") + styleName).value("Bold").toInt();
    else{
        styleTest.bold = 0;
    }

    if (styles.contains(QString("Style") + styleName) && styles.value(QString("Style") + styleName).contains("Italic")) styleTest.italic = styles.value(QString("Style") + styleName).value("Italic").toInt();
    else{
        styleTest.italic = 0;
    }

    if (styles.contains(QString("Style") + styleName) && styles.value(QString("Style") + styleName).contains("Underline")) styleTest.underLine = styles.value(QString("Style") + styleName).value("Underline").toInt();
    else{
        styleTest.underLine= 0;
    }

    if (styles.contains(QString("Style") + styleName) && styles.value(QString("Style") + styleName).contains("Strikeout")) styleTest.strikeOut = styles.value(QString("Style") + styleName).value("Strikeout").toInt();
    else{
        styleTest.strikeOut= 0;
    }

    bool toScreen{true};
    if (styles.contains(QString("Style") + styleName) && styles.value(QString("Style") + styleName).contains("ToScreen")) toScreen = styles.value(QString("Style") + styleName).value("ToScreen").toInt();
    textForProt = QString("Поток=%1\r\n"
                          "ИмяПотока=Style%1\r\n"
                          "Название=Style%1\r\n"
                          "Color=%2\r\n"
                          "BackColor=%3\r\n"
                          "FontName=%4\r\n"
                          "Size=%5\r\n"
                          "Charset=%6\r\n"
                          "Bold=%7\r\n"
                          "Italic=%8\r\n"
                          "Underline=%9\r\n"
                          "Strikeout=%10\r\n").arg(styleTest.potok).arg(styleTest.color).arg(styleTest.backColor).arg(styleTest.fontName).arg(styleTest.fontSize).arg(styleTest.charSet).arg(styleTest.bold).arg(styleTest.italic).arg(styleTest.underLine).arg(styleTest.strikeOut);
    if (nuMessage){
        ProtManager::instance().writeRecordToNU(textMessage);
    }
    if (!nuMessage || this->trackMode.load() == 1){
        //запись в протокол (файл) стиля текста
        ProtManager::instance().writeRecord(textForProt, -2, styleTest.potok);
        //запись в протокол (файл) текста
        textForProt = textMessage;
        //textForProt.replace("\t", "    ");
        textForProt = replaceTabulation(textForProt);

        QStringList listStringForProt = textForProt.split("\n");
        for (const QString &stringForProt : listStringForProt){
            ProtManager::instance().writeRecord(stringForProt, 3, styleTest.potok);
        }

        if (!nuMessage || !onlyNUFile){
            QString textDecoration;
            if (!styleTest.underLine && !styleTest.strikeOut) textDecoration = "none";
            else if (styleTest.underLine && styleTest.strikeOut) textDecoration = "underline line-through";
            else if (styleTest.underLine) textDecoration = "underline";
            else if (styleTest.strikeOut) textDecoration = "line-through";

            QString textColor;
            QString backgroundColor;

            if (!constValues::colorTranslate.contains(styleTest.color) || !constValues::colorTranslate.contains(styleTest.backColor)){
                textColor = "black";
                backgroundColor = "transparent";
            } else{
                textColor = constValues::colorTranslate.value(styleTest.color);
                backgroundColor = constValues::colorTranslate.value(styleTest.backColor);
            }

            textForWgt = QString(R"(<span style="white-space: pre; color: %1; background-color: %2; font-family: '%3'; font-size: %4px; font-weight: %5; font-style: %6; text-decoration: %7;">)").arg(textColor).arg(backgroundColor).arg(styleTest.fontName)
                    .arg(styleTest.fontSize * 1.5).arg(styleTest.bold ? "bold" : "normal").arg(styleTest.italic ? "italic" : "normal").arg(textDecoration);
            textForWgt.append(textMessage);
            textForWgt.append("</span>");

            QEventLoop loop;
            QObject::connect(this, &directRunner::messageSet, &loop, &QEventLoop::quit);

            emit this->appendMessageToProtocol(textForWgt);
        }
    }
}

void directRunner::runProgram(/*QTextEdit *protocol, QWidget *protocolWgt, QStandardItemModel *programInfomodel*/){
    if (programs.last().blockRun){
        //stopProg = true;
        //emit this->setStopState(programs.last().infoStopMsg);
        return;
    }
    hasRunProg = true;
    programStruct program = /*programs.pop();*/programs.last();
    int rowProgram = -1;
    /*for (int row = programInfomodel->rowCount() - 1; row >= 0; --row){
        if (programInfomodel->data(programInfomodel->index(row, 1)).toString() == program.programName){
            rowProgram = row;
            break;
        }
    }*/
   /* QMap<QString, int> metkaAddr;
    for (int numDir = 0; numDir < program.directList.count(); ++numDir){
        if (!program.directList[numDir]->metka.isEmpty()){
            if (!metkaAddr.contains(program.directList[numDir]->metka)){
                metkaAddr.insert(program.directList[numDir]->metka, numDir);
            } else{
                QString errorMessage = "<span style='color:red; white-space: pre;'>\t#\t\t";
                errorMessage.append(QString("Дубилкатная метка: %1\n").arg(program.directList[numDir]->metka));
                if (numDir > -1){
                    QString numDirect(QString::number(numDir + 1));
                    numDirect.prepend(QString((numDirect.count() < 3) ? 3 - numDirect.count() : 0, QChar('0')));
                    errorMessage.append("\t#" + numDirect + "\t\t" + program.directList[numDir]->directive);
                }
                errorMessage.append("</span>");
                protocol->append(errorMessage);
                programInfomodel->removeRow(rowProgram);
                programs.pop();
                hasRunProg = false;
                return;
            }
        }
    }*/
    if (program.numDir == -1) program.numDir = 0;
    if (programs.last().numDir == -1) programs.last().numDir = 0;
    /*if (!this->metka.isEmpty()){
        int directiveNum;
        if (metkaAddr.contains(this->metka)) directiveNum = metkaAddr.value(this->metka);
        else{ directiveNum = metka.toInt();
            if (directiveNum < 1 || directiveNum > program.directList.count()){
                QString errorMessage = "<span style='color:red; white-space: pre;'>\t#\t\t";
                errorMessage.append(QString("НЕ НАЙДЕНА МЕТКА, ЗАДАННАЯ В ДИРЕКТИВЕ ИЛИ НЕВЕРНЫЙ НОМЕР ПЕРЕХОДА"));
                errorMessage.append("</span>");
                protocol->append(errorMessage);
                programInfomodel->removeRow(rowProgram);
                programs.pop();
                hasRunProg = false;
                this->metka = "";
                return;
            }
        program.numDir = directiveNum - 1;
        this->metka = "";
        }
    }*/
    //int numDir = program.numDir;
    //for (int numDir = program.numDir; numDir < program.directList.count(); ++numDir){
    while (programs.last().numDir < program.directList.count()){
        QApplication::processEvents();
        //programInfomodel->setData(programInfomodel->index(rowProgram, 2), QString::number(programs.last().numDir + 1));
        {
            QEventLoop loop;
            QObject::connect(this, &directRunner::programModelActionAccept, &loop, &QEventLoop::quit);

            emit this->setProgramNumDirInModel(programs.last().numDir + 1, programs.last().directList[programs.last().numDir]->numLine);
            loop.exec();
        }
        program.numDir = programs.last().numDir;
        //QThread::sleep(1);
        int prCount = programs.count();
        bool runNorm{false};
        if (programs.last().numDir == 0 && programs.last().directList[0]->direct != DirectParser::TypeDirect::PROGRAM){
            QString errorMessage;
            errorMessage = "\t#";
            errorMessage.append(QString("\t%1").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")));
            errorMessage.append("\t");
            errorMessage.append("ОТСУТСВТУЕТ ДИРЕКТИВА ПРОГРАММ ИЛИ СООБЩЕНИЕ В НЕЙ");
            //protocol->append(errorMessage);
            {
                printInProt(errorMessage, "13", textStyle());
            }
            errorMessage = "";
            errorMessage.append("\t\t");
            errorMessage.append(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"));
            errorMessage.append("\t");
            errorMessage.append("ЗАВЕРШИТЕ ПРОГРАММУ КОМАНДОЙ ВЫХОД ИЛИ РВЫХОД");
            //protocol->append(errorMessage);
            {
                printInProt(errorMessage, "0", textStyle());
            }
            stopProg = true;
            programs.last().blockRun = true;
            return;
        }
        hasRunProg = true;
        qDebug() << programs.last().numDir;
        runNorm = runDirectFunc(*program.directList[programs.last().numDir]/*, protocol, protocolWgt, programInfomodel*/);
        qDebug() << programs.last().numDir;
        {
            int nextDirNumLine;
            if (programs.last().directList.count() <= programs.last().numDir + 1) nextDirNumLine = programs.last().programText.count();
            else {nextDirNumLine = programs.last().directList[programs.last().numDir + 1]->numLine;}

            QEventLoop loop;
            QObject::connect(this, &directRunner::programModelActionAccept, &loop, &QEventLoop::quit);

            emit this->setProgramNumDirInModel(programs.last().numDir + 1, nextDirNumLine);
            loop.exec();
        }
        if (programs.last().numDir == 0 /*&& programs.last().directList[programs.last().numDir]->direct == DirectParser::TypeDirect::PROGRAM*/ && !runNorm){
            stopProg = true;
            programs.last().blockRun = true;
            return;
        }
        if (prCount > programs.count()){
            if (programs.isEmpty()){
                while (!command.isEmpty()){
                    runDirectFunc(command.dequeue()/*, protocol, protocolWgt, programInfomodel*/);
                }
            }
            return;
        }
        if (prCount < programs.count()){
            programs[prCount].autoRun = true;
            programs[prCount - 1].numDir += 1;
            return;
        }
        if ((programs.last().numDir == programs.last().directList.count() - 1 && programs.last().directList[programs.last().numDir]->direct != DirectParser::TypeDirect::KPROGRAM) ||
            ((programs.last().numDir == programs.last().directList.count() - 1) && (!runNorm))){
            QString errorMessage;
            errorMessage = "\t#";
            errorMessage.append("\t");
            errorMessage.append(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"));
            errorMessage.append("\t");
            errorMessage.append("КОНЕЦ ФАЙЛА, НЕТ КПРОГРАММ");
            //protocol->append(errorMessage);
            {
                printInProt(errorMessage, "13", textStyle());
            }
            errorMessage = "";
            errorMessage.append("\t\t");
            errorMessage.append(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"));
            errorMessage.append("\t");
            errorMessage.append("ЗАВЕРШИТЕ ПРОГРАММУ КОМАНДОЙ ВЫХОД ИЛИ РВЫХОД");
            //protocol->append(errorMessage);
            {
                printInProt(errorMessage, "0", textStyle());
            }
            stopProg = true;
            programs.last().blockRun = true;
            return;
        }
        qDebug() << programs.last().numDir;
        programs.last().numDir += 1;
        qDebug() << programs.last().numDir;
        if (stopProg && programs.last().numDir >= programs.last().directList.count()){
            stopProg = false;
        }
        if (stopProg || !runNorm){
            stopProg = true;
            QString stopInfoMessage;
            stopInfoMessage.append(QString("Имя ПИ = %1\n").arg(programs.last().programName));
            stopInfoMessage.append(QString("Номер директивы = %1\n").arg(programs.last().numDir));
            if (programs.last().directList[programs.last().numDir - 1]->direct == DirectParser::TypeDirect::STOP){
                stopInfoMessage.append(QDateTime::currentDateTime().toString("HH:mm:ss.zzz") + QString(" # ОСТАНОВ ПО ДИРЕКТИВЕ СТОП"));
            }
            if (!runNorm){
                stopInfoMessage.append(QDateTime::currentDateTime().toString("HH:mm:ss.zzz") + QString(" # ОСТАНОВ: НЕШТАТНОЕ ЗАВЕРШЕНИЕ %1").arg(programs.last().directList[programs.last().numDir - 1]->directive));
            }
            if (!stopMessageStr.isEmpty()){
                stopInfoMessage.append("\n" + stopMessageStr);
                stopMessageStr.clear();
            }
            programs.last().infoStopMsg = stopInfoMessage;
            emit this->setStopState(stopInfoMessage);
            return;
        }
        /*if (!this->metka.isEmpty()){
            bool ok{false};
            int numDirMetka = this->metka.toInt(&ok);
            if (metkaAddr.contains(this->metka) || (ok && numDirMetka >= 1 && numDirMetka <= program.directList.count())){
                DirectParser dirPars;
                DirectParser::Direct *direct = dirPars.parseKO(QString("НА %1").arg(this->metka)).at(0);
                direct->numDirect = numDir + 1;
                runDirect(*direct, protocol, protocolWgt, programInfomodel);
                if (ok) numDir = numDirMetka;
                else numDir = metkaAddr.value(this->metka);
                numDir -= 2;
                this->metka = "";
            } else{
                QString errorMessage = "<span style='color:red; white-space: pre;'>\t#\t\t";
                errorMessage.append(QString("НЕ НАЙДЕНА МЕТКА, ЗАДАННАЯ В ДИРЕКТИВЕ ИЛИ НЕВЕРНЫЙ НОМЕР ПЕРЕХОДА"));
                if (numDir > -1){
                    QString numDirect(QString::number(numDir + 1));
                    numDirect.prepend(QString((numDirect.count() < 3) ? 3 - numDirect.count() : 0, QChar('0')));
                    errorMessage.append("\t#" + numDirect + "\t\t" + program.directList[numDir]->directive);
                }
                errorMessage.append("</span>");
                protocol->append(errorMessage);
                programInfomodel->removeRow(rowProgram);
                programs.pop();
                hasRunProg = false;
                this->metka = "";
                return;
            }
        }*/
    }
    //programInfomodel->removeRow(rowProgram);
    {
        QEventLoop loop;
        QObject::connect(this, &directRunner::programModelActionAccept, &loop, &QEventLoop::quit);
        emit this->removeProgramInModel();
        loop.exec();
    }
    bool needStartPr = programs.last().autoRun;
    programs.pop();
    hasRunProg = false;

    if (needStartPr){
        runProgram();
    } else{
        /*if (programs.last().blockRun)*/if (programs.length() >= 1 && !programs.last().infoStopMsg.isEmpty()) emit this->setStopState(programs.last().infoStopMsg);
    }
    if (programs.isEmpty()){
        while (!command.isEmpty()){
            runDirectFunc(command.dequeue()/*, protocol, protocolWgt, programInfomodel*/);
        }
    }
}

directRunner::directRunner(QObject *parent) : QObject(parent)
{
    blockAllIsm = false;
    voltReady = false;
    jsonReceiver = nullptr;
    hasConnectNU.store(0);
    stopMessageStr = "";
    metka = "";
    GL_VAR = "";
    GL_MODE_NEXT = true;
    GL_NORM_STATUS = true;
    //hasRunProg = false;
    //stopProg = false;
    stepMode.store(0);
    reactStopMode.store(0);
    ost_flag.store(0);
    m_ost_flag.store(0);
    v100Mode = false;
    QString errorProtText;
    ProtManager::instance().createProtocol(errorProtText);
    /*if (!errorProtText.isEmpty()) {
        qDebug() << "ERROR OPEN PROT: " << errorProtText;
        //emit this->errorProtValid(errorProtText);
        QTimer::singleShot(0, this, [this, errorProtText]() {
            emit this->errorProtValid(errorProtText);
        });
    }
    qDebug() << "DONE OPEN PROT: " << errorProtText;*/

    if (!errorProtText.isEmpty()) {
        QMessageBox *msgBox = new QMessageBox(QMessageBox::Critical, "Ошибка!", errorProtText, QMessageBox::Ok);
        msgBox->setWindowFlags(msgBox->windowFlags() | Qt::WindowStaysOnTopHint);
        msgBox->exec();
        QTimer::singleShot(1000, qApp, &QCoreApplication::quit);
    }

    //directNumPotok.reserve(static_cast<int>(DirectParser::TypeDirect::NO_DIRECT));    //резервируем место под наше кол-во директив (NO_DIRECT является последним членом перечисления директив и при этом не является директивой, поэтому его порядковый номер может быть использован как количество директив)

    /*const QStringList directNames {"*", "А_КОНТР", "ВАРИАНТК", "ВЫБОР", "ВЫБОР100", "ВЫЗВАТЬ", "ВЫХОД", "ВЫЧИСЛ", "ДИРЕКТ", "ЕСЛИДА", "ЗАПРОС", "КПРОГРАММ", "КСИНОНИМ", "НА", "ПОВТОР", "ПОДКЛ_1М", "ПОДКСОЕД", "ПРОВЕРКА", "ПРОГРАМ", "ПРЦ", "ПСИ", "ПСЦ", "ПСЦ_Р", "ПУСК", "РВЫХОД", "РР_ПАР", "СИНОНИМ", "СООБЩ", "СП", "СТОП", "УВ"};
    if (directNames.length() != static_cast<int>(DirectParser::TypeDirect::NO_DIRECT)) qDebug() << "not equal";
    else qDebug() << "equal";
    */
    QString stylesFilePath = QApplication::applicationDirPath();
    stylesFilePath.append("/Styles.ini");

    if (!QFile::exists(stylesFilePath)){
        QString errorMessage(QString("Файл стилей не найден!"));
        //QMessageBox::critical(nullptr, "Ошибка", errorMessage);
        QMessageBox *msgBox = new QMessageBox(QMessageBox::Critical, "Ошибка!", errorMessage, QMessageBox::Ok);
        msgBox->setWindowFlags(msgBox->windowFlags() | Qt::WindowStaysOnTopHint);
        msgBox->exec();
        QTimer::singleShot(1000, qApp, &QCoreApplication::quit);
    }
    else{
        QFile stylesFile(stylesFilePath);
        if (!stylesFile.open(QIODevice::ReadOnly | QIODevice::Text)){
            QString errorMessage(QString("Не удалось открыть файл стилей!"));
            //QMessageBox::critical(nullptr, "Ошибка", errorMessage);
            QMessageBox *msgBox = new QMessageBox(QMessageBox::Critical, "Ошибка!", errorMessage, QMessageBox::Ok);
            msgBox->setWindowFlags(msgBox->windowFlags() | Qt::WindowStaysOnTopHint);
            msgBox->exec();
            QTimer::singleShot(1000, qApp, &QCoreApplication::quit);
        }
        else{
            QTextStream in(&stylesFile);
            QRegularExpression regex(R"(\[(Style.*)\])");
            while (!in.atEnd()){
                QString line = in.readLine();
                QRegularExpressionMatch match = regex.match(line);
                if (!match.hasMatch()) continue;
                QString styleName = match.captured(1);
                QMap<QString, QString> paramStyle;
                while (!in.atEnd()){
                 line = in.readLine();
                 if (line.isEmpty() || line.startsWith("--")) break;
                 QString styleText = line;
                 QStringList styleTextList = styleText.split("=");
                 if (styleTextList[0].isEmpty() || styleTextList[1].isEmpty()) break;
                 paramStyle[styleTextList[0]] = styleTextList[1];
                }
                //if (styleText.isEmpty()) continue;
                if (paramStyle.isEmpty()) continue;
                styles[styleName] = paramStyle;
            }

            for (const QString& styleName : styles.keys()){
                //QString styleText = styles.value(styleName);
                QMap<QString, QString>styleText = styles.value(styleName);
            }
        }
    }

    socketCanal1 = new QTcpSocket(this);
    socketCanal2 = new QTcpSocket(this);
    voltSocket = new QTcpSocket(this);
}

bool directRunner::runDirectFunc(const DirectParser::Direct &dir){
    if ((dir.numDirect == -1 && hasRunProg && !stopProg)){
        return true;
    }
    stopMessageStr = "";
    GL_NORM_STATUS = true;
    QString printMessage;
    QString errorMessage;
    printMessage = "\t";
    errorMessage = "\t#";
    QString numDirect(QString::number(dir.numDirect));
    numDirect.prepend(QString((numDirect.count() < 3) ? 3 - numDirect.count() : 0, QChar('0')));
    //printMessage.append(QString::number(dir.numDirect));
    if (dir.numDirect > -1){
        printMessage.append(numDirect);
        errorMessage.append(numDirect);
    }
    printMessage.append("\t");
    errorMessage.append("\t");
    printMessage.append(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"));
    errorMessage.append(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"));
    printMessage.append("\t");
    errorMessage.append("\t");
    if (dir.numDirect == -1 && !DirectParser::isOperatorDirect(dir.direct)){
        errorMessage.append(QString("НЕТ ТАКОЙ КОМАНДЫ ОПЕРАТОРА - %1").arg(dir.directive));
        //protocol->append(errorMessage);
        {
            printInProt(errorMessage, "13", textStyle());
        }
        return false;
    } else if (dir.numDirect > -1 && !DirectParser::isTableDirect(dir.direct)){
        errorMessage.append(QString("%1 - НЕТ ТАКОЙ ДИРЕКТИВЫ").arg(dir.directive));
        //protocol->append(errorMessage);
        {
            printInProt(errorMessage, "13", textStyle());
        }
        return false;
    } else if (dir.numDirect == -2 && !DirectParser::isVariantDirect(dir.direct)){
        errorMessage.append(QString("НЕДОПУСТИМАЯ ДИРЕКТИВА ДЛЯ ИСПОЛЬЗОВАНИЯ В ДИРЕКТИВЕ ВЫБОР - %1").arg(dir.directive));
        {
            printInProt(errorMessage, "13", textStyle());
        }
        return false;
    }

    //printMessage.append(dir.directive);
    //printMessage.append(" ");
    /*QStringList dirParams = dir.paramDirect;
    if (!dirParams.isEmpty()) dirParams.removeLast();
    dirParams.removeAll("");
    printMessage.append(dirParams.join(" ").replace("\n ", "\n\t\t\t"));

    protocol->append(printMessage);*/

    /*for (int row = 0; row < dir.testParamDirect.count(); ++row){
        for (int col = 0; col < dir.testParamDirect[row].count(); ++col){
            for (int block = 0; block < dir.testParamDirect[row][col].count(); ++block){
                if (dir.testParamDirect[row][col][block].isEmpty()) continue;
                printMessage.append(dir.testParamDirect[row][col][block] + " ");
            }
        }
        if (row != dir.testParamDirect.count() - 1) printMessage.append("\n\t\t\t");
    }*/
    //protocol->append(printMessage);

    if (dir.direct != DirectParser::TypeDirect::NO_DIRECT && !dir.metka.isEmpty()){
        QString metkaMessage = QString("\t" + numDirect + "\t\t" + "Метка " + dir.metka);
        printInProt(metkaMessage, "22", textStyle());
    }


    switch (dir.direct) {
    case (DirectParser::TypeDirect::A_KONTR) :{
        if (dir.testParamDirect.length() != 1 || dir.testParamDirect[0].length() != 2 || dir.testParamDirect[0][1].length() != 1 || (dir.testParamDirect[0][1][0] != "1" && dir.testParamDirect[0][1][0] != "2" && dir.testParamDirect[0][1][0] != "3" && dir.testParamDirect[0][1][0] != "4")){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНедопустимые параметры директивы А_КОНТР");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        bool ok;
        int kontrMode = dir.testParamDirect[0][1][0].toInt(&ok);
        if (!ok || kontrMode < 1 || kontrMode > 4){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНедопустимый режим директивы А_КОНТР");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if (socketCanal1->state() != QAbstractSocket::ConnectedState || socketCanal2->state() != QAbstractSocket::ConnectedState){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНЕТ СВЯЗИ С НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        printMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" "));
        printInProt(printMessage, "0", textStyle());

        char c[2];
        c[0] = char(0x23);
        c[1] = char(kontrMode);
        bool status;
        sendMessageToNU(c, 2, &status);
        if (!status) return false;
        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на А_КОНТР", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
         break;
    }
    case (DirectParser::TypeDirect::VARIANTK) :{
        if (dir.testParamDirect.count() < 2){
            if (dir.testParamDirect.count() == 1 && dir.testParamDirect[0][1].join(" ") == QString("ОТМЕН")){
                printMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" "));
                printInProt(printMessage, "0", textStyle());
                GL_VAR.clear();
                break;
            }
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("НЕТ СТРОК С ДИРЕКТИВАМИ\n\t\t\tВАРИАНТК: ДИРЕКТИВА НЕ ВЫПОЛНЕНА");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        /*QDialog *varTK = new QDialog();
        varTK->setModal(true);
        varTK->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);

        QTextEdit *textEdit = new QTextEdit(varTK);
        QListWidget *listWgt = new QListWidget(varTK);
        QPushButton *accept = new QPushButton("Принять", varTK);
        QPushButton *acceptStop = new QPushButton("Принять с остановом", varTK);

        QHBoxLayout *hBox = new QHBoxLayout();
        hBox->addWidget(listWgt);
        hBox->addWidget(accept);
        hBox->addWidget(acceptStop);

        QVBoxLayout *vBox = new QVBoxLayout();
        vBox->addWidget(textEdit);
        vBox->addLayout(hBox);

        varTK->setLayout(vBox);*/

        QString text;
        QList<QString> variable;
        printMessage.append(dir.directive + "\n");
        for (int row = 1; row < dir.testParamDirect.count(); ++row){
            if (dir.testParamDirect[row].count() < 4){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕДОСТАТОЧНО ПАРАМЕТРОВ ДИРЕКТИВЫ ВАРИАНТК\n\t\t\tСТРОКА ДИРЕКТИВЫ №%1").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            if ((!dir.testParamDirect[row][0].isEmpty() && !dir.testParamDirect[row][0].join("").isEmpty()) || (!dir.testParamDirect[row][1].isEmpty() && !dir.testParamDirect[row][1].join("").isEmpty())){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("ПОЛЕ ДОЛЖНО БЫТЬ ПУСТЫМ\n\t\t\tСТРОКА ДИРЕКТИВЫ №%1").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            QString curRow;
            for (int block = 0; block < dir.testParamDirect[row][3].count(); ++block){
                curRow.append(dir.testParamDirect[row][3][block] + " ");
            }
            curRow.chop(1);
            curRow.append("\n");

            if (dir.testParamDirect[row][2].isEmpty() || dir.testParamDirect[row][2].join("").isEmpty()){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("ПОЛЕ Т НЕ ДОЖНО БЫТЬ ПУСТЫМ\n\t\t\tСТРОКА ДИРЕКТИВЫ №%1").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            printMessage.append("\t\t\t" + dir.testParamDirect[row][2][0]);
            printMessage.append(" " + curRow);
            if (dir.testParamDirect[row][2][0].isEmpty() && row != 1){
                text.append("\t" + curRow);
                continue;
            }
            QString variatn = dir.testParamDirect[row][2].join("");
            if (variatn.count() > 1 || variatn == "О" || variatn == "К" || variatn == "П"){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("В ПОЛЕ Т УКАЗАН НЕДОПУСТИМЫЙ СИМВОЛ ИЛИ ПОСЛЕДОВАТЕЛЬНОСТЬ СИМВОЛОВ\n\t\t\tСТРОКА ДИРЕКТИВЫ №%1").arg(QString::number(row + 1)));

                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            variable.append(dir.testParamDirect[row][2][0]);
            text.append(dir.testParamDirect[row][2][0] + "\t" + curRow);
        }
        printMessage.chop(1);
        //printMessage.prepend("<span style='color: blue; white-space: pre;'>");
        //printMessage.append("</span>");
        //protocol->append(printMessage);

        {
            printInProt(printMessage, "23", textStyle());
        }
        //textEdit->setText(text);
        if (variable.isEmpty()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("ОШИБКА - НЕДОПУСТИМО ОТСУТСТВИЕ ВАРИАНТОВ ВЫБОРА"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }

        /*for (const auto& var : variable){
            listWgt->addItem(var);
        }

        listWgt->selectionModel()->clearSelection();
        listWgt->setCurrentRow(-1);

        QObject::connect(accept, &QPushButton::clicked, [this, varTK, listWgt, printMessage, protocol, dir](){
            if (listWgt->currentItem() == nullptr || listWgt->currentRow() == -1) return;
            this->GL_VAR = listWgt->currentItem()->text();
            qDebug() << "GL_VAR: " << GL_VAR;
            varTK->accept();
            //QString pMsg = printMessage;
            //pMsg.append(dir.directive);
            //pMsg.append("\n\t\t\t");
            QString pMsg = QString("\t\t\t");
            if (this->GL_VAR.isEmpty()) pMsg.append("ОТКАЗ ОТ ВВОДА ВАРИАНТА");
            else pMsg.append(QString("ПРИЯНТ ВАРИАНТ %1").arg(this->GL_VAR));
            //pMsg.prepend("<span style='color: blue; white-space: pre;'>");
            //pMsg.append("</span>");
            protocol->append(pMsg);
            delete varTK;
        });

        QObject::connect(varTK, &QDialog::rejected, [printMessage, protocol, dir](){
            //QString pMsg = printMessage;
            //pMsg.append(dir.directive);
            //pMsg.append("\n\t\t\t");
            QString pMsg = QString("\t\t\t");
            pMsg.append("ОТКАЗ ОТ ВВОДА ВАРИАНТА");
            //pMsg.prepend("<span style='color: blue; white-space: pre;'>");
            //pMsg.append("</span>");
            protocol->append(pMsg);
        });



        varTK->exec();*/

        emit showVarDialogWindow(text, variable);

        QEventLoop loop;
        QObject::connect(this, &directRunner::varinantkSelectedVar, &loop, &QEventLoop::quit);
        loop.exec();

        printMessage = QString("\t\t\t");
        if (this->GL_VAR.isEmpty()){
            printMessage.append("ОТКАЗ ОТ ВВОДА ВАРИАНТА");
            stopMessageStr = "ОСТАНОВ ПО ОТКАЗУ ОТ ВВОДА ВАРИАНТА В ДИРЕКТИВЕ ВАРИАНТК";
            stopProg = true;
        }
        else {printMessage.append(QString("ПРИНЯТ ВАРИАНТ %1").arg(this->GL_VAR));}

        //protocol->append(printMessage);
        {
            printInProt(printMessage, "3", textStyle());
        }
        printMessage = "";
        break;
    }
    case (DirectParser::TypeDirect::VYBOR) :{
        QString direct;
        if (dir.testParamDirect.count() < 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("НЕТ СТРОК С ДИРЕКТИВАМИ\n\t\t\tВАРИАНТК: ДИРЕКТИВА НЕ ВЫПОЛНЕНА");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (dir.testParamDirect[0].count() > 2 || (!dir.testParamDirect[0][1].isEmpty() && !dir.testParamDirect[0][1].join("").isEmpty())){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append("ПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ");
                printInProt(errorMessage, "13", textStyle());
                return false;
        }
        for (int row = 1; row < dir.testParamDirect.count(); ++row){
            QString var;
            if (dir.testParamDirect[row].count() >= 2 && !dir.testParamDirect[row][0].isEmpty() && !dir.testParamDirect[row][0][0].isEmpty()){
                var = dir.testParamDirect[row][0][0];
                if (var == "О" || var == "К" || var == "П" || var == "!" || var == "$" || var == " "){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("НЕДОПУСТИМЫЙ ВАРИАНТ В ДИРЕКТИВЕ ВЫБОР (СТРОКА ДИРЕКТИВЫ - %1)").arg(QString::number(row + 1)));
                    {
                        printInProt(errorMessage, "13", textStyle());
                    }
                    return false;

                }
                if (GL_VAR == var){
                    printMessage.append(dir.directive + " " + var);
                    //protocol->append(printMessage);
                    {
                        printInProt(printMessage, "0", textStyle());
                    }
                    printMessage = "";
                    direct = "О!";
                    direct.append(dir.testParamDirect[row][1].join(" "));
                    direct.append("!");
                    break;
                }
            } else{
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕДОСТАТОЧНО ПАРАМЕТРОВ ДИРЕКТИВЫ ВЫБОР (СТРОКА ДИРЕКТИВЫ - %1)").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
        }
        if (direct.isEmpty()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("ВЫБОР НЕ НАЙДЕН, ОСТАНОВ"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        } else {
            DirectParser dirParser;
            QList<DirectParser::Direct*> directives = dirParser.parseString(direct);
            if (!directives.isEmpty()){
                directives[0]->numDirect = -2;
                this->runDirectFunc(*directives.at(0)/*, protocol, protocloWgt, programInfomodel*/);
            }
        }
        break;
    }
    case (DirectParser::TypeDirect::VYBOR_100):{
        if (dir.testParamDirect.count() != 1 || dir.testParamDirect[0].count() != 2 || dir.testParamDirect[0][1].isEmpty() || dir.testParamDirect[0][1][0].isEmpty()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("ОШИБКА: НЕ УДАЛОСЬ ПРОЧИТАТЬ ПАРАМЕТРЫ ДИРЕКТИВЫ ВЫБОР100");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (dir.testParamDirect[0][1].count() > 1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("ОШИБКА: ПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (dir.testParamDirect[0][1][0] != "В" && dir.testParamDirect[0][1][0] != "О"){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("ОШИБКА: НЕВЕРНЫЙ ПРИЗНАК ВКЛ/ОТКЛ В ДИРЕКТИВЕ ВЫБОР100");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        printMessage.append(dir.directive + "\n");
        //printMessage.prepend("<span style='background-color: transparent;'>");
        //printMessage.append("</span>");
        if (dir.testParamDirect[0][1][0] == "В"){
            /*protocol->setStyleSheet("background-color: lightblue;");
            protocloWgt->setObjectName("protocolWGT");
            protocloWgt->setStyleSheet("#protocolWGT {border: 2px solid red;}");*/
            emit v100Selected();
            printMessage.append("\t\t\tВКЛЮЧИТЬ");
            v100Mode = 1;
        } else if (dir.testParamDirect[0][1][0] == "О"){
            /*protocol->setStyleSheet("");
            protocloWgt->setStyleSheet("");*/
            emit v100Canceled();
            printMessage.append("\t\t\tОТКЛЮЧИТЬ");
            v100Mode = 0;
        }
        //protocol->append(printMessage);
        {
            /*QEventLoop loop;
            QObject::connect(this, &directRunner::messageSet, &loop, &QEventLoop::quit);
            emit this->appendMessageToProtocol(printMessage);
            loop.exec();*/
            //printInProt(printMessage, "2",  textStyle(0, "clRed", "clWhite"));
            printInProt(printMessage, "0", textStyle());
        }
        break;
    }
    case (DirectParser::TypeDirect::VYSVAT) : {
        if (dir.testParamDirect[0].count() != 2 || dir.testParamDirect[0][1].count() != 1 || dir.testParamDirect[0][1][0].isEmpty()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("ОШИБКА ПОЛУЧЕНИЯ ПАРАМЕТРОВ ДИРЕКТИВЫ ВЫЗВАТЬ");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        QString fileName = dir.testParamDirect[0][1][0];
        if (QFileInfo(fileName).baseName().length() > 30){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("ОШИБКА ИМЯ ПРОГРАММЫ НЕ ДОЛЖНО БЫТЬ БОЛЬШЕ 30 СИМВОЛОВ");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        QStringList catalogs = MainWindow::getProgramCatalog();
        if (QFileInfo(fileName).suffix().isEmpty()) fileName.append(".dip");
        QString fullFilePath = "";
        for (const QString& catalog : catalogs) {
            fullFilePath = findFileRecursive(catalog, fileName);
            if (!fullFilePath.isEmpty())
                break;
        }
        if (fullFilePath.isEmpty() || fullFilePath == ""){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("%1 - ЦГ НЕ НАЙДЕНА").arg(fileName));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        printMessage.append(dir.directive + " " + dir.testParamDirect[0][1][0] + "\n");
        printMessage.append("\t\t\t" + fullFilePath + "\n");
        QFile file(fullFilePath);
        if (!file.open(QIODevice::Text | QIODevice::ReadOnly)){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("ОШИБКА ОТКРЫТИЯ ФАЙЛА");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }

        FileHeaderInfo info = readDipHeader(fullFilePath);

        if (!info.ok) {
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("НЕ УДАЛОСЬ РАСПОЗНАТЬ КОДИРОВКУ ФАЙЛА!");
            {
                printInProt(errorMessage, "13", textStyle());
            }
        return false;
        } else {
            qDebug() << "Encoding:" << info.encoding;
            qDebug() << "First line:" << info.firstLine;
            qDebug() << "Checksum:" << info.checksum;
        }

        QTextStream in(&file);
        QTextCodec *codec = QTextCodec::codecForName(info.encoding.toUtf8());
        if (!codec) {
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("ОШИБКА ПЕРЕКОДИРОВКИ ФАЙЛА!");
            {
                printInProt(errorMessage, "13", textStyle());
            }
        }
        in.setCodec(codec);
        QString KS;
        QStringList programText;
        KS = in.readLine();
        qDebug() << "KS: " << KS;
        if (KS.startsWith("П!КОНТРОЛЬНАЯ СУММА=")){
            KS = KS.mid(20);
        } else{
            KS = "";
        }
        qDebug() << "KS: " << KS;
        /*while (!in.atEnd()){
            QString line = in.readLine();
            if (line.startsWith("П!КОНТРОЛЬНАЯ СУММА=")){
                KS = line.mid(20);
                break;
            }
            if (line.startsWith("О!")) break;
            break;
        }*/
        in.seek(0);
        while (!in.atEnd()){
            QString line = in.readLine();
            if (line.startsWith("П!КОНТРОЛЬНАЯ СУММА=")) continue;
            programText.append(line);
        }
        if (KS.isEmpty() || KS[KS.length() - 1] != "!"){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("НЕ НАЙДЕНА КОНТРОЛЬНАЯ СУММА!");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        KS.chop(1);
        in.seek(0);
        QByteArray fileData = file.readAll();
        if (constValues::isNeedCheckKS.load() == 1 && KS.toUpper() != getKS(fileData).toUpper()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("КОНТРОЛЬНАЯ СУММА В ПАСПОРТЕ НЕ СООТВЕТСТВУЕТ ВЫЧИСЛЕННОЙ");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        /*{
            QByteArray fullTextForKontrol = programText.join("\n").toLocal8Bit();
            QDataStream stream(fullTextForKontrol);
            stream.setByteOrder(QDataStream::LittleEndian);
            qint32 ks{0};
            while (!stream.atEnd()){
                qint32 value1{0};
                qint32 value2{0};
                stream >> value1;
                if (!stream.atEnd()) stream >> value2;
                ks += CirSum(value1, value2);
            }
            QByteArray ksByteArray = QByteArray::fromHex(KS.toLatin1());
            QByteArray check;
            for (int i = 0; i < 4; i++){
                check.append(static_cast<char>((ks >> (8 * i)) & 0xFF));
            }
            qDebug() << "KS File: " << KS;
            qDebug() << "KS File Byte Array: " << ksByteArray[0] << ksByteArray[1] << ksByteArray[2] << ksByteArray[3];
            qDebug() << "Res Calc KS: " << check[0]<< check[1]<< check[2]<< check[3];
            if (check == ksByteArray){
                qDebug() << "KS проверена";
            } else{
                qDebug() << "KS ERROR";
            }

        }*/
        printMessage.append("\t\t\t" + KS);
        //printMessage.prepend("<span style='color: green; white-space: pre;'>");
        //printMessage.append("</span>");
        //protocol->append(printMessage);
        {
            printInProt(printMessage, "2", textStyle());
        }
        printMessage = "";

        /*QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(programInfomodel->rowCount() + 1));
        row << new QStandardItem(QString(fileName));
        programInfomodel->appendRow(row);*/

        directRunner::programStruct program;
        DirectParser dirParser;
        program.programText = programText;
        program.directList = dirParser.parseFile(fullFilePath);
        program.programName = fileName;
        program.numDir = -1;
        program.autoRun = false;
        program.blockRun = false;
        for (int prDirNum = 0; prDirNum < program.directList.count(); ++prDirNum){
            if (!program.directList[prDirNum]->metka.isEmpty()){
                if (program.metkaAddr.contains(program.directList[prDirNum]->metka)){
                    errorMessage = "\t#\t\t";
                    errorMessage.append(QString("Дубилкатная метка: %1\n").arg(program.directList[prDirNum]->metka));
                    if (prDirNum > -1){
                        QString numDirect(QString::number(prDirNum + 1));
                        numDirect.prepend(QString((numDirect.count() < 3) ? 3 - numDirect.count() : 0, QChar('0')));
                        errorMessage.append("\t#" + numDirect + "\t\t" + program.directList[prDirNum]->directive);
                    }
                    {
                        printInProt(errorMessage, "13", textStyle());
                    }
                    return false;
                }
                program.metkaAddr.insert(program.directList[prDirNum]->metka, prDirNum + 1);
            }
        }
        emit this->unsetStopState();
        programs.push(program);
        {
            QEventLoop loop;
            QObject::connect(this, &directRunner::programModelActionAccept, &loop, &QEventLoop::quit);
            emit this->addProgramToModel(QString(fileName), programText);
            loop.exec();
        }
        //если вызвали из программы (или с директивы выбор (dir.numDirect == -2)), то сразу запускаем
        if (dir.numDirect >= 0 || dir.numDirect == -2) runProgram(/*protocol, protocloWgt, programInfomodel*/);
        break;
    }
    case (DirectParser::TypeDirect::VYHOD)  :{
        if (dir.testParamDirect.count() != 1 || dir.testParamDirect[0].count() != 2 || dir.testParamDirect[0][1].count() != 0){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("НЕДОПУСТИМО ИСПОЛЬЗОВАНИЕ ПАРАМЕТРОВ В ДИРЕКТИКЕ ВЫХОД"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (/*!hasRunProg &&*/ programs.isEmpty()){
            printMessage.append(dir.directive + QString(" ИСПОЛЬЗОВАНИЕ ДИРЕКТИВЫ ВЫХОД НЕДОПУСТИМО - НЕТ ЗАПУЩЕННЫХ ЦГ"));
            //protocol->append(printMessage);
            {
                printInProt(printMessage, "0", textStyle());
            }
            printMessage = "";
            return false;
        }
        printMessage.append(dir.directive + " " + programs.last().programName);
        if (programs.count() > 1){
            printMessage.append(" -> " + programs.at(programs.count() - 2).programName);
        }
        //protocol->append(printMessage);
        {
            printInProt(printMessage, "1", textStyle());
        }
        printMessage = "";
        emit this->unsetStopState();
        if (stopProg || !hasRunProg){
            /*for (int row = programInfomodel->rowCount() - 1; row >= 0; --row){
                if (programInfomodel->data(programInfomodel->index(row, 1)).toString() == programs.last().programName){
                    programInfomodel->removeRow(row);
                    break;
                }
            }*/
            {
                QEventLoop loop;
                QObject::connect(this, &directRunner::programModelActionAccept, &loop, &QEventLoop::quit);
                emit this->removeProgramInModel();
                loop.exec();
            }
            programs.pop();
        }
        else programs.last().numDir = programs.last().directList.count() + 1;
        if (programs.isEmpty()){
            while (!command.isEmpty()){
                runDirectFunc(command.dequeue()/*, protocol, protocloWgt, programInfomodel*/);
            }
        }
        break;
    }
    case (DirectParser::TypeDirect::VYCHISL) :{
        if (dir.testParamDirect.count() > 101){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("НЕДОПУСТИМОЕ КОЛИЧЕСТВО СТРОК В ДИРЕКТИВЕ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        for (int row = 0; row < dir.testParamDirect.count(); row++){
        if (dir.testParamDirect[row].count() != 2 || ((dir.testParamDirect[row][1].count() != 3) && (dir.testParamDirect[row][1].count() != 4))){
            if (row == 0 && dir.testParamDirect[row].count() == 2 && dir.testParamDirect[row][1].count() == 0){
                continue;
            }
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("ОШИБКА ЧТЕНИЯ ПАРАМЕТРОВ ДИРЕКТИВЫ РР_ПАР");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        bool ok{false};
        QString codeOperation = dir.testParamDirect[row][1][1];
        int operGroup{-1};
        if (codeOperation == "=" || codeOperation == "+=" || codeOperation == "-=" || codeOperation == "*=" || codeOperation == "/=") operGroup = 1;
        else if (codeOperation == "+" || codeOperation == "-" || codeOperation == "*" || codeOperation == "/") operGroup = 2;
        else if (codeOperation == "min" || codeOperation == "max") operGroup = 3;
        else {
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("НЕВЕРНО ЗАДАНА ОПЕРАЦИЯ В ДИРЕКТИВЕ ВЫЧИСЛ");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            qDebug() << "ERROR CODE OPERATION: " << codeOperation;
            return false;
        }
        if (operGroup == 2 && dir.testParamDirect[row][1].count() < 4){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("ОШИБКА: НЕДОСТАТОЧНО ПАРАМЕТРОВ ДИРЕКТИВЫ РР_ПАР");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (dir.testParamDirect[row].count() > 2 || (dir.testParamDirect[row][1].count() > 4 && operGroup == 2) || (dir.testParamDirect[row][1].count() > 3 && operGroup != 2)){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("ПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        struct resAddr{
            QString block;
            QString idParam;
            int index;
        };

        resAddr resultAddr;
        RRParam resultRRParam("");

        /*qfloat16*/float arg3value;
        dir.testParamDirect[row][1][0].toFloat(&ok);
        if (operGroup != 1 && operGroup != 3 && ok){
            arg3value = dir.testParamDirect[row][1][0].toFloat();
        } else{
            QString param = dir.testParamDirect[row][1][0];
            //QRegularExpression regex(R"(FL\.([A-Za-z0-9]{1,3})_([A-Za-z0-9]{1,8})(?:\[(\d+)\])?)");
            RRParam rrParam(param);
            if (!rrParam.isValid() || (rrParam.isArray() && (rrParam.getIndex() < 0 || rrParam.getLen() > 0))){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕДОПУСТИМОЕ ЗНАЧЕНИЕ РР_ПАРАМЕТРА В АРГ3 %1 (СТРОКА ДИРЕКТИВЫ %2)").arg(param).arg(row + 1));
                if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            if (operGroup == 1 || operGroup == 3){
                resultRRParam = RRParam(param);
            }
            {
                bool status{false};
                arg3value = rrParam.getValue(&status);
                if (!status || rrParam.isHasError()){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЯ РР_ПАРАМЕТРА В АРГ3 %1 (СТРОКА ДИРЕКТИВЫ %2)").arg(param).arg(row + 1));
                    if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
            }
        }
        QList</*qfloat16*/float> arg1Values;
        ok = false;
        dir.testParamDirect[row][1][2].toFloat(&ok);
        if (operGroup != 3 && ok){
            arg1Values.append(dir.testParamDirect[row][1][2].toFloat());
        } else{
            QString param = dir.testParamDirect[row][1][2];
            RRParam rrParam(param);
            if (!rrParam.isValid() ||
                    (operGroup == 3 && (!rrParam.isArray() || rrParam.getIndex() < 0 || rrParam.getLen() <= 0)) ||
                    (operGroup != 3 && rrParam.isArray() && (rrParam.getIndex() < 0 || rrParam.getLen() >= 0))){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕДОПУСТИМОЕ ЗНАЧЕНИЕ РР_ПАРАМЕТРА В АРГ1 %1 (СТРОКА ДИРЕКТИВЫ %2)").arg(param).arg(row + 1));
                if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            if (operGroup != 3){
                arg1Values.append(rrParam.getValue());
            } else{
                while (rrParam.getLen() > 0){
                    arg1Values.append(rrParam.getValue());
                    rrParam.setIndex(rrParam.getIndex() + 1);
                    rrParam.setLen(rrParam.getLen() - 1);
                }
            }
            if (arg1Values.isEmpty() || rrParam.isHasError()){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕУДАЛОСЬ ПОЛУЧИТЬ ЗНАЧЕНИЕ РР_ПАРАМЕТРА В АРГ1 %1 (СТРОКА ДИРЕКТИВЫ %2)").arg(param).arg(row + 1));
                if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
        }

        /*qfloat16*/ //float arg2value;
        if (operGroup == 2){
            QString param = dir.testParamDirect[row][1][3];
            RRParam rrParam(param);
            if (!rrParam.isValid() || (rrParam.isArray() && (rrParam.getIndex() < 0  || rrParam.getLen() >= 0)) || rrParam.isHasError()){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕДОПУСТИМОЕ ЗНАЧЕНИЕ РР_ПАРАМЕТРА В АРГ2 %1 (СТРОКА ДИРЕКТИВЫ %2)").arg(param).arg(row + 1));
                if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                printInProt(errorMessage, "13", textStyle());
                return false;
            }

            resultRRParam = RRParam(param);

        }
        /*qDebug() << "arg1Val: " << arg1Values;
        qDebug() << "arg2Val: " << arg2value;
        qDebug() << "arg3Val: " << arg3value;
        qDebug() << "res addr: FL." << resultAddr.block << "_" << resultAddr.idParam << "[" << resultAddr.index << "]";*/

        float result{0};
        if ((codeOperation == "/=" || codeOperation == "/") && arg1Values.at(0) == 0){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("ДЕЛЕНИЕ НА 0 НЕДОПУСТИМО (СТРОКА ДИРЕКТИВЫ %2)").arg(row + 1));
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if (codeOperation == "="){
            result = arg1Values.at(0);
        } else if (codeOperation == "+="){
            result = arg3value + arg1Values.at(0);
        } else if (codeOperation == "-="){
            result = arg3value - arg1Values.at(0);
        } else if (codeOperation == "*="){
            result = arg3value * arg1Values.at(0);
        } else if (codeOperation == "/="){
            result = arg3value / arg1Values.at(0);
        } else if (codeOperation == "+"){
            result = arg3value + arg1Values.at(0);
        } else if (codeOperation == "-"){
            result = arg3value - arg1Values.at(0);
        } else if (codeOperation == "*"){
            result = arg3value * arg1Values.at(0);
        } else if (codeOperation == "/"){
            result = arg3value / arg1Values.at(0);
        } else if (codeOperation == "min"){
            float min = arg1Values.at(0);
            for (const auto curVal : arg1Values){
                if (min > curVal) min = curVal;
            }
            result = min;
        } else if (codeOperation == "max"){
            float max = arg1Values.at(0);
            for (const auto curVal : arg1Values){
                if (max < curVal) max = curVal;
            }
            result = max;
        }
        //qDebug() << "result: " << result;

        resultRRParam.setValue(result);
        if (resultRRParam.isHasError()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("ОШИБКА ЗАПИСИ РЕЗУЛЬТАТА В РР_ПАРАМЕТР %1 (СТРОКА ДИРЕКТИВЫ %2)").arg(resultRRParam.getFullParamName()).arg(row + 1));
            errorMessage.append("\n\t\t\t" + resultRRParam.getErrorText());
            printInProt(errorMessage, "13", textStyle());
            return false;
        }

        QString resultString;
        if (!printMessage.isEmpty()){
            printMessage.append(dir.directive + " " + dir.testParamDirect[row][1].join(" "));
            //printMessage = "";
        } else{
            printMessage.append("\t\t\t " + dir.testParamDirect[row][1].join(" "));
        }
        //printMessage.prepend("<span style='color:blue; white-space: pre;'>");
        //printMessage.append("</span>");
        //protocol->append(printMessage);
        {
            printInProt(printMessage, "12", textStyle());
        }
        printMessage = "";
        if (resultAddr.index == -1) resultString = QString("-> FL." + resultAddr.block + "_" + resultAddr.idParam + " =\t" + QString::number(result));
        else resultString = QString("\t\t\t\t\t-> " + resultRRParam.getFullParamName() + " =   " + QString::number(result));
        //resultString.prepend("<span style='color: #FF00FF; white-space: pre;'>\t\t\t\t");
        //resultString.append("</span>");
        //protocol->append(resultString);
        {
            printInProt(resultString, "4", textStyle());
        }
        }

        break;
    }
    case (DirectParser::TypeDirect::DIRECT) : {
        if (dir.testParamDirect.count() > 100){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНЕДОПУСТИМОЕ КОЛИЧЕСТВО СТРОК В ДИРЕКТИВЕ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        QString textDirect;
        printMessage.append(dir.directive + "\n");
        for (int row = 0; row < dir.testParamDirect.count(); ++row){
            QString curRowText = "";
            //for (int col = 0; col < dir.testParamDirect[row].count(); ++col){
            if (dir.testParamDirect[row].count() < 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append("НЕДОСТАТОЧНО ПАРАМЕТРОВ ДИРЕКТИВЫ ДИРЕКТ");
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                qDebug() << "ERROR WRITE RESULT";
                return false;
            } else if (dir.testParamDirect[row].count() > 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append("ПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ ДИРЕКТИВЫ ДИРЕКТ");
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                qDebug() << "ERROR WRITE RESULT";
                return false;
            }
                for (int block = 0; block < dir.testParamDirect[row][1/*col*/].count(); ++block){
                    if (dir.testParamDirect[row][1/*col*/][block].isEmpty()) continue;
                    curRowText.append(dir.testParamDirect[row][1/*col*/][block] + " ");
                }
            //}
            if (curRowText.isEmpty()) continue;
            printMessage.append("\t\t\t" + curRowText + "\n");
            textDirect.append(curRowText + "\n");
        }
        //printMessage.append(textDirect);
        printMessage.chop(1);
        //printMessage.prepend("<span style='color: blue; white-space: pre;'>");
        //printMessage.append("</span>");
        //protocol->append(printMessage);
        {
            printInProt(printMessage, "23", textStyle());
        }
        printMessage = "";

       {
            QEventLoop loop;
            QObject::connect(this, &directRunner::directSelectedVar, &loop, &QEventLoop::quit);
            emit this->showDirectWindow(textDirect);
            loop.exec();
       }


        break;
    }
    case (DirectParser::TypeDirect::ESLI_DA): {
        if (dir.testParamDirect.count() > 100){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("НЕДОПУСТИМОЕ КОЛИЧЕСТВО СТРОК В ДИРЕКТИВЕ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        QStringList result;
        if (dir.testParamDirect.count() < 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("НЕДОСТАТОЧНО СТРОК ДИРЕКТИВЫ ЕСЛИДА");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (dir.testParamDirect[0].count() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            if (dir.testParamDirect[0].count() < 2) errorMessage.append("НЕДОСТАТОЧНО ПАРАМЕТРОВ ДИРЕКТИВЫ ЕСЛИДА (СТРОКА 1)");
            else errorMessage.append("ПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ (СТРОКА 1)");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (dir.testParamDirect[0][1].count() != 1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            if (dir.testParamDirect[0][1].count() < 1) errorMessage.append("НЕДОСТАТОЧНО ПАРАМЕТРОВ ДИРЕКТИВЫ ЕСЛИДА (СТРОКА 1)");
            else errorMessage.append("ПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ (СТРОКА 1)");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        QRegularExpression regMetka(R"(^:[A-Za-zА-Яа-яЁё0-9]{1,8}$)");
        QRegularExpressionMatch regMetkaMatch = regMetka.match(dir.testParamDirect[0][1][0]);
        if (!regMetkaMatch.hasMatch() || dir.testParamDirect[0][1][0][0] == "G" || dir.testParamDirect[0][1][0][0] == "L"){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("ОШИБКА: НЕУДАЛОСЬ РАСПОЗНАТЬ МЕТКУ В ДИРЕКТИВЕ ЕСЛИДА (СТРОКА 1)");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }

        printMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" "));
        //printMessage.prepend("<span style='color: #FF00FF; white-space: pre;'>");
        //printMessage.append("</span>");
        //protocol->append(printMessage);
        {
            printInProt(printMessage, "8", textStyle());
        }
        printMessage = "";
        for (int row = 1; row < dir.testParamDirect.count(); ++row){
            printMessage = "\t\t\t";
            if (dir.testParamDirect[row].count() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                if (dir.testParamDirect[0].count() < 2) errorMessage.append(QString("НЕДОСТАТОЧНО ПАРАМЕТРОВ ДИРЕКТИВЫ ЕСЛИДА (СТРОКА %1)").arg(QString::number(row + 1)));
                else errorMessage.append(QString("ПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ (СТРОКА %1)").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            if (dir.testParamDirect[row][1].count() < 1 || (dir.testParamDirect[row][1][0] == "ИЛИ" && dir.testParamDirect[row][1].count() != 1) || (dir.testParamDirect[row][1][0] != "ИЛИ" && dir.testParamDirect[row][1].count() != 3)){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕ УДАЛОСЬ ИЗВЛЕЧЬ ПАРАМЕТРЫ ДИРЕКТИВЫ ЕСЛИДА (СТРОКА %1)").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            if (dir.testParamDirect[row][1][0] == "ИЛИ"){
                if (row == 1 || dir.testParamDirect[row - 1][1][0] == "ИЛИ" || row == dir.testParamDirect.count() - 1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("НЕДОПУСТИМОЕ ИСПОЛЬЗОВАНИЕ ИЛИ В ДИРЕКТИВЕ ЕСЛИДА (СТРОКА %1)").arg(QString::number(row + 1)));
                    {
                        printInProt(errorMessage, "13", textStyle());
                    }
                    return false;
                }
                result.append(dir.testParamDirect[row][1][0]);
                printMessage.append("ИЛИ");
                //printMessage.prepend("<span style='color: #FF00FF; white-space: pre;'>");
                //printMessage.append("</span>");
                //protocol->append(printMessage);
                {
                    printInProt(printMessage, "8", textStyle());
                }
                continue;
            }
            if (row >= 2 && result.last() != "И" && result.last() != "ИЛИ"){
                result.append("И");
            }
            QString arg1 = dir.testParamDirect[row][1][0];
            QString arg2 = dir.testParamDirect[row][1][2];
            bool ok{false};
            arg1.toFloat(&ok);
            if (!ok)
            {
                QString param = arg1;
                RRParam rrParam(param);
                if (!rrParam.isValid() || rrParam.getLen() >= 0){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("НЕДОПУСТИМОЕ ЗНАЧЕНИЕ РР_ПАРАМЕТРА %1  (ОП1 СТРОКА %2)").arg(rrParam.getFullParamName()).arg(QString::number(row + 1)));
                    if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                arg1 = QString::number(rrParam.getValue(&ok));
                if (!ok || rrParam.isHasError()){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЯ РР_ПАРАМЕТРА %1  (ОП1 СТРОКА %2)").arg(rrParam.getFullParamName()).arg(QString::number(row + 1)));
                    if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
            }
            ok = false;
            arg2.toFloat(&ok);
            if (!ok){
                QString param = arg2;
                RRParam rrParam(param);
                if (!rrParam.isValid() || rrParam.getLen() >= 0){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("НЕДОПУСТИМОЕ ЗНАЧЕНИЕ РР_ПАРАМЕТРА %1  (ОП2 СТРОКА %2)").arg(rrParam.getFullParamName()).arg(QString::number(row + 1)));
                    if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                arg2 = QString::number(rrParam.getValue(&ok));
                if (!ok || rrParam.isHasError()){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЯ РР_ПАРАМЕТРА %1  (ОП2 СТРОКА %2)").arg(rrParam.getFullParamName()).arg(QString::number(row + 1)));
                    if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
            }

            ok = false;
            float val1 = arg1.toFloat(&ok);
            if (!ok){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕВАЛИДНОЕ ЗНАЧЕНИЯ ПАРАМЕТРА (%1) В БД").arg(dir.testParamDirect[0][1][0]));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            ok = false;
            float val2 = arg2.toFloat(&ok);
            if (!ok){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕВАЛИДНОЕ ЧТЕНИЯ ЗНАЧЕНИЯ ПАРАМЕТРА (%1) В БД").arg(dir.testParamDirect[0][1][2]));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }

            QString operation = dir.testParamDirect[row][1][1];
            if (operation == "=="){
                result.append((val1 == val2) ? "1" : "0");
            } else if (operation == "/="){
                result.append((val1 != val2) ? "1" : "0");
            } else if (operation == ">="){
                result.append((val1 >= val2) ? "1" : "0");
            } else if (operation == "<="){
                result.append((val1 <= val2) ? "1" : "0");
            } else if (operation == ">"){
                result.append((val1 > val2) ? "1" : "0");
            } else if (operation == "<"){
                result.append((val1 < val2) ? "1" : "0");
            } else{
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕТ ОПЕРАЦИИ ДИРЕКТИВЫ ЕСЛИДА (СТРОКА %1)").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                qDebug() << "QUERY ERROR";
                return false;
            }
            QString resStr;
            if (result.last() == "1"){
                resStr = "ИСТИНА";
                //printMessage.prepend("<span style='color: #FF00FF; white-space: pre;'>");
                //printMessage.prepend("<span style='color: #FF00FF; white-space: pre;'>");
            }
            else if (result.last() == "0"){
                resStr = "ЛОЖЬ";
                //printMessage.prepend("<span style='color: #A52A2A; white-space: pre;'>");
            } else{
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("ОШИБКА ВЫПОЛНЕНИЯ ДИРЕКТИВЫ ЕСЛИДА"));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                qDebug() << "QUERY ERROR";
                return false;
            }
            printMessage.append(dir.testParamDirect[row][1][0] + QString(" ( %1 )").arg(QString::number(val1)) + " " + QString(operation).replace("<", "&lt;").replace(">", "&gt;") + " " + dir.testParamDirect[row][1][2] + QString(" ( %2 )").arg(QString::number(val2)) + "\t" + resStr);
            //printMessage.append("</span>");
            //protocol->append(printMessage);
            {
                if (resStr == "ИСТИНА") printInProt(printMessage, "8", textStyle());
                else printInProt(printMessage, "51", textStyle());
            }
        }

        qDebug() << result;
        for (int block = 1; block < result.count(); block+=2){
            bool ok{false};
            int arg1 = result.at(block-1).toInt(&ok);
            if (!ok){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("ОШИБКА ВЫПОЛНЕНИЯ СРАВНЕНИЯ"));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            ok = false;
            int arg2 = result.at(block+1).toInt(&ok);
            if (!ok){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("ОШИБКА ВЫПОЛНЕНИЯ СРАВНЕНИЯ"));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            int res;
            if (result.at(block) == "И"){
                if (arg1 == 0 || arg2 == 0) res = 0;
                else res = 1;
            } else if (result.at(block) == "ИЛИ"){
                if (arg1 == 1 || arg2 == 1) res = 1;
                else res = 0;
            } else{
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("ОШИБКА ВЫПОЛНЕНИЯ СРАВНЕНИЯ"));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            result.removeAt(block+1);
            result.removeAt(block);
            result[block-1] = QString::number(res);
            block -= 2;
            //qDebug() << result;
        }
        /*qDebug() << result;
        for (int block = 1; block < result.count(); block+=2){
            bool ok{false};
            int arg1 = result.at(block-1).toInt(&ok);
            if (!ok){
                qDebug() << "Error value";
                return;
            }
            ok = false;
            int arg2 = result.at(block+1).toInt(&ok);
            if (!ok){
                qDebug() << "Error Value";
                return;
            }
            int res;
            if (arg1 == 1 || arg2 == 1) res = 1;
            else res = 0;
            result.removeAt(block+1);
            result.removeAt(block);
            result[block-1] = QString::number(res);
            block -= 2;
            qDebug() << result;
        }*/
        qDebug() << result;
        if (result[0] == "1"){
            this->metka = dir.testParamDirect[0][1][0];
            DirectParser dirPars;
            DirectParser::Direct *direct = dirPars.parseKO(QString("НА %1").arg(this->metka)).at(0);
            direct->numDirect = dir.numDirect;
            runDirectFunc(*direct/*, protocol, protocloWgt, programInfomodel*/);
            //qDebug() << "metka: " << this->metka;
        }
        break;
    }
    case (DirectParser::TypeDirect::ZAPROS): {
        if (dir.testParamDirect.count() > 101){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("НЕДОПУСТИМОЕ КОЛИЧЕСТВО СТРОК В ДИРЕКТИВЕ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        printMessage.append(dir.directive);
        if (dir.testParamDirect.count() == 1 && dir.testParamDirect[0].count() == 2) printMessage.append(" " + dir.testParamDirect[0][1].join(" "));
        int rowNum{0};
        QString resultString;
        do{
        if (dir.testParamDirect[rowNum].count() != 2 || dir.testParamDirect[rowNum][1].count() != 1){
            if (rowNum == 0 && dir.testParamDirect[rowNum].count() == 2 && dir.testParamDirect[rowNum][1].count() == 0){
                rowNum+=1;
                continue;
            }
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("НЕ УДАЛОСЬ ИЗВЛЕЧЬ ПАРАМЕТРЫ ДИРЕКТИВЫ ЗАПРОС (СТРОКА %1)").arg(QString::number(rowNum + 1)));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        QList<RRParam> rrParamsList;
        if (rowNum == 0 && dir.testParamDirect[0][1][0] == "ВСЕ"){
            if (dir.testParamDirect.count() > 1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕДОПУСТИМО НАЛИЧИЕ ДОПОЛНИТЕЛЬНЫХ СТРОК ПРИ ИСПОЛЬЗОВАНИИ ПАРАМЕТРА ВСЕ"));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            QString errorString;
            QStringList blocks = RRParam::getAllBlockName(&errorString);
            if (!errorString.isEmpty()){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(errorString);
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            if (blocks.isEmpty()){
                printInProt(printMessage, "0", textStyle());
                printInProt("\t\t\t\tВ БД РР_ПАРАМЕТРОВ НЕТ ЗАПИСЕЙ", "0", textStyle());
            }
            for (const auto& blockName : blocks){
                        QStringList paramNames = RRParam::getAllParamInBlock(blockName, &errorString);
                        if (!errorString.isEmpty()){
                            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                            if (dir.numDirect > -1){
                                errorMessage.append("\t#" + numDirect + "\t\t");
                            } else errorMessage.append("\t\t\t");
                            errorMessage.append(errorString);
                            printInProt(errorMessage, "13", textStyle());
                            return false;
                        }
                        //resultString.append("\t\t\tFL." + blockName + "\n");
                        for (const auto& paramName : paramNames){
                            RRParam rrParam("");
                            rrParam.setBlockName(blockName);
                            rrParam.setParamName(paramName);
                            rrParam.setIndex(-1);
                            rrParam.setLen(-1);
                            rrParam.resetError();
                            int p_index{0};
                            int p_len{1};
                            if (rrParam.isArray()){
                                //p_index = rrParam.getIndex();
                                p_len = rrParam.getLength();
                                resultString.append(QString("\t\t\t" + rrParam.getFullParamName(RRParam::FORMAT_PARAM::BLOCK_NAME) + "\n"));
                            }
                            for (int i = p_index; i < p_len; i++){
                                if (rrParam.isArray()) rrParam.setIndex(i);
                                double value = rrParam.getValue();
                                if (rrParam.isHasError()){
                                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                                if (dir.numDirect > -1){
                                        errorMessage.append("\t#" + numDirect + "\t\t");
                                    } else errorMessage.append("\t\t\t");
                                    errorMessage.append(rrParam.getErrorText());
                                    printInProt(errorMessage, "13", textStyle());
                                    return false;
                                }
                                QString otstup;
                                if (rrParam.isArray()) otstup = "\t\t\t\t";
                                else otstup = "\t\t\t";
                                resultString.append(QString(otstup + rrParam.getFullParamName(RRParam::FORMAT_PARAM::BLOCK_NAME_INDEX) + " =\t" + QString::number(value)));
                                resultString.append("\n");
                            }
                        }
                        resultString.chop(1);
                        if (!printMessage.isEmpty()){
                            printInProt(printMessage, "0", textStyle());
                            printMessage = "";
                        }
                        printInProt(resultString, "0", textStyle());
                        resultString = "";
                    }
        } else{
            QString paramName = dir.testParamDirect[rowNum][1][0];
            RRParam::FORMAT_PARAM format = RRParam::getFormatParam(paramName);
            if (format == RRParam::FORMAT_PARAM::EMPTY){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("Недопустимое имя РР_ПАРАМЕТРА %1 (строка %2)").arg(paramName).arg(rowNum + 1));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (format == RRParam::FORMAT_PARAM::BLOCK){
                QStringList paramParsed = RRParam::parseParam(paramName);
                QString errorText;
                QStringList paramNames = RRParam::getAllParamInBlock(paramParsed[0], &errorText);
                if (!errorText.isEmpty()){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(errorText);
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                if (paramNames.isEmpty()){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("В БД НЕТ РР_ПАРАМЕТРОВ С ИМЕНЕМ БЛОКА %1 (строка директивы %2)").arg(paramParsed[0]).arg(rowNum + 1));
                    errorMessage.append(QString("\n\t\t\tОшибка чтения РР_ПАРАМЕТРА"));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                printInProt(QString("\t\t\t" + paramName + ":"), "0", textStyle());
                for (const auto& paramName : paramNames){
                    RRParam rrParam("");
                    rrParam.setBlockName(paramParsed[0]);
                    rrParam.setParamName(paramName);
                    rrParam.setIndex(-1);
                    rrParam.setLen(-1);
                    rrParam.resetError();
                    int p_index{0};
                    int p_len{1};
                    if (rrParam.isArray()){
                        p_len = rrParam.getLength();
                    }
                    for (int i = p_index; i < p_len; i++){
                        if (rrParam.isArray()) rrParam.setIndex(i);
                        double value = rrParam.getValue();
                        if (rrParam.isHasError()){
                            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        if (dir.numDirect > -1){
                                errorMessage.append("\t#" + numDirect + "\t\t");
                            } else errorMessage.append("\t\t\t");
                            errorMessage.append(rrParam.getErrorText());
                            printInProt(errorMessage, "13", textStyle());
                            return false;
                        }
                        resultString.append(QString("\t\t\t\t" + rrParam.getFullParamName(RRParam::FORMAT_PARAM::BLOCK_NAME_INDEX) + " =\t" + QString::number(value)));
                        resultString.append("\n");
                    }
                }
                resultString.chop(1);
                if (!printMessage.isEmpty()){
                    //printMessage.append("\n\t\t\t" + paramName + ":");
                    printInProt(printMessage, "0", textStyle());
                    printMessage = "";
                }
                printInProt(resultString, "0", textStyle());
                resultString = "";
            } else{
                QStringList parsedParam = RRParam::parseParam(paramName);
                RRParam rrParam("");
                rrParam.setBlockName(parsedParam[0]);
                rrParam.setParamName(parsedParam[1]);
                if (!parsedParam[2].isEmpty()) rrParam.setIndex(parsedParam[2].toInt());
                else rrParam.setIndex(-1);
                if (!parsedParam[3].isEmpty()) rrParam.setLen(parsedParam[3].toInt());
                else rrParam.setLen(-1);
                rrParam.resetError();
                if (!rrParam.isValid()){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("Ошибка чтения РР_ПАРАМЕТРА %1 (строка директивы %2)").arg(paramName).arg(rowNum + 1));
                    if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                int p_index{0};
                int p_len{1};
                if (format == RRParam::FORMAT_PARAM::FULL || format == RRParam::FORMAT_PARAM::BLOCK_NAME_INDEX) p_index = rrParam.getIndex();
                if (format == RRParam::FORMAT_PARAM::FULL || format == RRParam::FORMAT_PARAM::BLOCK_NAME_LEN) p_len = rrParam.getLen();

                if (rrParam.isArray() && parsedParam[2].isEmpty()){
                    rrParam.setIndex(0);
                }
                if (rrParam.isArray() && parsedParam[3].isEmpty()){
                    rrParam.setLen(rrParam.getLength() - rrParam.getIndex());
                    if (parsedParam[2].isEmpty()) p_len = rrParam.getLen();
                    else p_len = 1;
                }
                while (p_len > 0){
                    double value = rrParam.getValue();
                    if (rrParam.isHasError()){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                            errorMessage.append("\t#" + numDirect + "\t\t");
                        } else errorMessage.append("\t\t\t");
                        errorMessage.append(rrParam.getErrorText());
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                    resultString.append(QString("\t\t\t\t" + rrParam.getFullParamName(RRParam::FORMAT_PARAM::BLOCK_NAME_INDEX) + " =\t" + QString::number(value)));
                    resultString.append("\n");
                    rrParam.setIndex(++p_index);
                    p_len -= 1;
                }
                resultString.chop(1);
                if (!printMessage.isEmpty()){
                    printInProt(printMessage, "0", textStyle());
                    printMessage = "";
                }
                printInProt(resultString, "0", textStyle());
                resultString = "";
            }
        }

        rowNum += 1;
        } while(rowNum < dir.testParamDirect.count());
        break;
    }
    case (DirectParser::TypeDirect::COMMENT) : {
        if (dir.testParamDirect.count() != 1 || dir.testParamDirect[0].count() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("ОШИБКА ЧТЕНИЯ СТРОКИ КОММЕНТАРИЯ"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        printMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" "));
        //printMessage.prepend("<span style='color: blue; white-space: pre;'>");
        //printMessage.append("</span>");
        //protocol->append(printMessage);
        {
            printInProt(printMessage, "23", textStyle());
        }
        printMessage = "";
        break;
    }
    case (DirectParser::TypeDirect::KPROGRAM) :{
        if (dir.testParamDirect.count() != 1 || dir.testParamDirect[0].count() != 2 || !dir.testParamDirect[0][1].join("").isEmpty()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("ИСПОЛЬЗОВАНИЕ ПАРАМЕТРОВ В ДИРЕКТИВЕ КПРОГРАМ НЕДОПУСТИМО"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        printMessage.append(dir.directive + " " + programs.last().programName);
        if (programs.count() > 1){
            printMessage.append(" -> " + programs.at(programs.count() - 2).programName);
        }
        //protocol->append(printMessage);
        {
            printInProt(printMessage, "1", textStyle());
        }
        printMessage = "";

        if (stopProg || !hasRunProg){
            /*for (int row = programInfomodel->rowCount() - 1; row >= 0; --row){
                if (programInfomodel->data(programInfomodel->index(row, 1)).toString() == programs.last().programName){
                    programInfomodel->removeRow(row);
                    break;
                }
            }*/
            {
                QEventLoop loop;
                QObject::connect(this, &directRunner::programModelActionAccept, &loop, &QEventLoop::quit);
                emit this->removeProgramInModel();
                loop.exec();
            }
            programs.pop();
        }
        else programs.last().numDir = programs.last().directList.count() + 1;
        break;
    }
    case (DirectParser::TypeDirect::KSINONIM) : qDebug() << "пока не реализовано"; break;
    case (DirectParser::TypeDirect::NA) :{
        if (dir.testParamDirect.count() != 1 || dir.testParamDirect[0].count() != 2 || dir.testParamDirect[0][1].count() != 1 || dir.testParamDirect[0][1][0].isEmpty()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("НЕ УДАЛОСЬ ПРОЧИТАТЬ ПАРАМЕТРЫ ДИРЕКТИВЫ НА"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (programs.isEmpty() || programs.last().numDir < 0){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("ДИРЕКТИВА НЕДОПУСТИМА - НЕТ ЗАПУЩЕННЫХ ЦГ"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        QString metka = dir.testParamDirect[0][1][0];
        bool ok{false};
        int numDir = metka.toInt(&ok);
        if (ok && dir.numDirect >= 0){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("ПЕРЕХОД ПО НОМЕРУ ДИРЕКТИВЫ ДОПУСТИМ ТОЛЬКО ЧЕРЕЗ КО"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (!ok || numDir < 1){
            QRegularExpression regMetka(R"(^:[A-Za-zА-Яа-яЁё0-9]{1,8}$)");
            QRegularExpressionMatch regMetkaMatch = regMetka.match(metka);
            if (!regMetkaMatch.hasMatch()){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕДОПУСТИМАЯ МЕТКА ИЛИ НОМЕР ДИРЕКТИВЫ"));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
        }
        if ((ok && (numDir <= 1 || numDir > programs.last().directList.count())) || (!ok && !programs.last().metkaAddr.contains(metka))){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("НЕ НАЙДЕНА МЕТКА, ЗАДАННАЯ В ДИРЕКТИВЕ ИЛИ НЕВЕРНЫЙ НОМЕР ПЕРЕХОДА"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (!ok){
            numDir = programs.last().metkaAddr.value(metka);
        }
        if (dir.numDirect > 0) programs.last().numDir = numDir - 2;
        else programs.last().numDir = numDir - 1;
        //this->metka = metka;
        printMessage.append(dir.directive + " " + metka);
        //printMessage.prepend("<span style='color: green; white-space: pre;'>");
        //printMessage.append("</span>");
        //protocol->append(printMessage);
        {
            printInProt(printMessage, "2", textStyle());
        }
        printMessage = "";
        break;
    }
    case (DirectParser::TypeDirect::POVTOR) : {
        if (dir.testParamDirect.count() != 1 || dir.testParamDirect[0].count() != 2 || dir.testParamDirect[0][1].count() != 0){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("НЕДОПУСТИМО ИСПОЛЬЗОВАНИЕ ПАРАМЕТРОВ В ДИРЕКТИКЕ ПОВТОР"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (!hasRunProg || programs.isEmpty()){
            printMessage.append(dir.directive + QString(" ИСПОЛЬЗОВАНИЕ ДИРЕКТИВЫ ПОВТОР НЕДОПУСТИМО - НЕТ ЗАПУЩЕННЫХ ЦГ"));
            //protocol->append(printMessage);
            {
                printInProt(printMessage, "0", textStyle());
            }
            printMessage = "";
            return false;
        }
        if (programs.last().numDir <= 1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("ИСПОЛЬЗОВАНИЕ ДИРЕКТИВЫ ПОВТОР НЕДОПУСТИМО ДО ВСТРЕЧИ 2 ДИРЕКТИВЫ ЦГ"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        printMessage.append(dir.directive);
        //protocol->append(printMessage);
        {
            printInProt(printMessage, "0", textStyle());
        }
        printMessage = "";
        programs.last().numDir -= 1;
        stopProg = false;
        emit this->unsetStopState();
        runProgram(/*protocol, protocloWgt, programInfomodel*/);
        break;
    }
    case (DirectParser::TypeDirect::PODKL_1M) : {
        if (dir.testParamDirect.length() != 1 || dir.testParamDirect[0].length() != 2 || dir.testParamDirect[0][1].length() != 1 || (dir.testParamDirect[0][1][0] != "В" && dir.testParamDirect[0][1][0] != "О")){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНедопустимые параметры директив ПОДК_1М");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if (socketCanal1->state() != QAbstractSocket::ConnectedState || socketCanal2->state() != QAbstractSocket::ConnectedState){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНЕТ СВЯЗИ С НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        printMessage.append(dir.directive + "\n");
        if (dir.testParamDirect[0][1][0] == "В") printMessage.append("\t\t\tВКЛЮЧИТЬ");
        else printMessage.append("\t\t\tОтключить");
        printInProt(printMessage, "0", textStyle());

        char c[2];
        c[0] = char(0x22);
        if (dir.testParamDirect[0][1][0] == "В") c[1] = char(1);
        else c[1] = char(0);

        bool status;
        sendMessageToNU(c, 2, &status);
        if (!status) return false;
        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПОДК_1М", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        break;
    }
    case (DirectParser::TypeDirect::PODKSOED) : {
        if (dir.testParamDirect.length() != 1 || dir.testParamDirect[0].length() != 2 || dir.testParamDirect[0][1].length() != 1 || dir.testParamDirect[0][1][0].isEmpty()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНедопустимое количество параметров директивы ПОДКСОЕД");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if (socketCanal1->state() != QAbstractSocket::ConnectedState || socketCanal2->state() != QAbstractSocket::ConnectedState){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНЕТ СВЯЗИ С НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        bool ok;
        int delayTime = dir.testParamDirect[0][1][0].toInt(&ok);
        if (!ok){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tЗадержка должна быть целым числом");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        printMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" "));
        printInProt(printMessage, "0", textStyle());
        char c[2];// = [0x0D, 0x01];
        c[0] = char(0x0A);
        //socketCanal1->write(c, 1);
        //socketCanal1->flush();
        bool status;
        sendMessageToNU(c, 1, &status);
        if (!status) return false;
        if (ost_flag.load() == 1){
            //if (dir.numDirect != -1) programs.last().numDir -= 1;
            QByteArray cBAReset;
            cBAReset.clear();
            cBAReset.append(char(0x0a));
            bool statusReset{false};
            sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
            //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
            goto end_metka;
        }
        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПОДКСОЕД", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        c[0] = char(0x0D);
        c[1] = char(delayTime);
        sendMessageToNU(c, 2, &status);
        if (!status) return false;
        if (ost_flag.load() == 1){
            //if (dir.numDirect != -1) programs.last().numDir -= 1;
            QByteArray cBAReset;
            cBAReset.clear();
            cBAReset.append(char(0x0a));
            bool statusReset{false};
            sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
            //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
            goto end_metka;
        }

        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПОДКСОЕД", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        //qDebug() << "пока не реазиовано";
        break;
    }
    case (DirectParser::TypeDirect::PROVERKA) :{
        if (dir.testParamDirect.count() > 101){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНЕДОПУСТИМОЕ КОЛИЧЕСТВО СТРОК В ДИРЕКТИВЕ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if ((dir.numDirect >= 0 && (dir.testParamDirect.count() < 2 || dir.testParamDirect[0].count() != 2 || dir.testParamDirect[0][1].count() > 1))
                ||
            ((dir.numDirect < 0) && (dir.testParamDirect.count() != 1 || dir.testParamDirect[0].count() != 2 || dir.testParamDirect[0][1].count() < 2 || dir.testParamDirect[0][1].count() > 3))){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("НЕ УДАЛОСЬ ПРОЧИТАТЬ ПАРАМЕТРЫ ДИРЕКТИВЫ ПРОВЕРКА (СТРОКА 1)"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        QString react;
        //bool stopFlag = false;
        reactType reactMode = reactType::NO_REACT;
        if (dir.numDirect >= 0){
            if (!dir.testParamDirect[0][1].isEmpty()) react = dir.testParamDirect[0][1][0];
            if (react == "СТОП" || react == "СЛЕД"){
                if (dir.numDirect < 0){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("НЕДОПУСТИМО ИСПОЛЬЗОВАНИЕ РЕАК В КО"));
                    {
                        printInProt(errorMessage, "13", textStyle());
                    }
                    return false;
                }
                if (react == "СТОП") reactMode = reactType::STOP;
                else reactMode = reactType::STOP;
                //stopFlag = true;
                qDebug() << "СТОП";
            } else if (!react.isEmpty() /*&& dir.numDirect > 0*/){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕДОПУСТИМЫЙ ПАРАМЕТР РЕАК ДИРЕКТИВЫ ПРОВЕРКА"));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
        }
        int startRow{1};
        if (dir.numDirect < 0) startRow = 0;
        for (int row = startRow; row < dir.testParamDirect.count(); ++row){
            if (dir.testParamDirect[row].count() != 2 || dir.testParamDirect[row][1].count() < 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕ УДАЛОСЬ ПРОЧИТАТЬ ПАРАМЕТРЫ ДИРЕКТИВЫ ПРОВЕРКА (СТРОКА %1)").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            QString param = dir.testParamDirect[row][1][0];
            bool ok{false};
            //QRegularExpression regex(R"(FL\.([A-Za-z0-9]{1,3})_([A-Za-z0-9]{1,8})(?:\[(\d+)\])?)");
            QRegularExpression regex(R"(^FL.([A-Za-z0-9]{1,3})_([A-Za-z0-9_\-\/\.]{1,15})(?:\[(\d+)\])?$)");
            QRegularExpressionMatch regMatch = regex.match(param);


            if (!regMatch.hasMatch()){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕВЕРНЫЙ ФОРМАТ РР_ПАРАМЕТРА (СТРОКА %1)").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            QString block = regMatch.captured(1);
            QString par = regMatch.captured(2);
            int index = -1;
            if (!regMatch.captured(3).isEmpty()){
                ok = false;
                index = regMatch.captured(3).toInt(&ok);
                if (!ok){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("НЕДОПУСТИМЫЙ ИНДЕКС РР_ПАРАМЕТРА (СТРОКА %1)").arg(QString::number(row + 1)));
                    {
                        printInProt(errorMessage, "13", textStyle());
                    }
                    return false;
                }
            }
            /*QString errorMsg;
            if (!isParamExists(block, par, errorMsg)){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                if (!errorMsg.isEmpty()) errorMessage.append(errorMsg);
                else errorMessage.append(QString("РР_ПАРАМЕТР НЕ НАЙДЕН В БД (СТРОКА %1)").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            if (index != -1 && !isParamArray(block, par, errorMsg)){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                if (!errorMsg.isEmpty()) errorMessage.append(errorMsg);
                else errorMessage.append(QString("РР_ПАРАМЕТР НЕ МАССИВ (СТРОКА %1)").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            QString queryString(QString("SELECT Val FROM RR_PAR WHERE Bl_Name = '%1' AND Par_Name = '%2'").arg(block).arg(par));
            if (index != -1) queryString.append(QString(" AND Index = %1").arg(QString::number(index)));
            QSqlQuery query = MainWindow::getQueryRRDB(queryString);
            if (!query.isActive() || !query.next()){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                                if (dir.numDirect > -1){
                                    errorMessage.append("\t#" + numDirect + "\t\t");
                                } else errorMessage.append("\t\t\t");
                                errorMessage.append("ОШИБКА ОБРАЩЕНИЯ К БД: " + query.lastError().text());
                                {
                                    printInProt(errorMessage, "13", textStyle());
                                }
                                qDebug() << "QUERY ERROR";
                                return false;
            }*/
            RRParam rrParam(param);
            ok = false;
            float val = rrParam.getValue(&ok);/*query.value(0).toFloat(&ok);*/
            if (!ok){ 
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("ОШИБКА ЧТЕНИЯ ЗНАЧЕНИЯ РР_ПАРАМЕТРА ИЗ БД (СТРОКА %1)").arg(QString::number(row + 1)));
                if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            bool test{true};
            QString dopusk("НД=ND ВД=VD");
            if (dir.testParamDirect[row][1].count() >= 2 && !dir.testParamDirect[row][1][1].isEmpty()){
            float nd = dir.testParamDirect[row][1][1].toFloat(&ok);
                if (!ok){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("НЕДОПУСТИМЫЙ ФОРМАТ ЗНАЧЕНИЯ НИЖНЕГО ДОПУСКА (СТРОКА %1)").arg(QString::number(row + 1)));
                    {
                        printInProt(errorMessage, "13", textStyle());
                    }
                    return false;
                }
                dopusk = dopusk.replace("ND", QString::number(nd));
                if (val < nd) test = false;
                if (dir.testParamDirect[row][1].count() >= 3 && !dir.testParamDirect[row][1][2].isEmpty()){
                    float vd = dir.testParamDirect[row][1][2].toFloat(&ok);
                    if (!ok){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        if (dir.numDirect > -1){
                            errorMessage.append("\t#" + numDirect + "\t\t");
                        } else errorMessage.append("\t\t\t");
                        errorMessage.append(QString("НЕДОПУСТИМЫЙ ФОРМАТ ЗНАЧЕНИЯ ВЕРХНЕГО ДОПУСКА (СТРОКА %1)").arg(QString::number(row + 1)));
                        {
                            printInProt(errorMessage, "13", textStyle());
                        }
                        return false;
                    }
                    if (nd > vd){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        if (dir.numDirect > -1){
                            errorMessage.append("\t#" + numDirect + "\t\t");
                        } else errorMessage.append("\t\t\t");
                        errorMessage.append(QString("НИЖНИЙ ДОПУСК НЕ ДОЛЖЕН БЫТЬ БОЛЬШЕ ВЕРХНЕГО ДОПУСКА (СТРОКА %1)").arg(QString::number(row + 1)));
                        {
                            printInProt(errorMessage, "13", textStyle());
                        }
                        return false;
                    }
                    dopusk = dopusk.replace("VD", QString::number(vd));
                    if (val > vd) test = false;
                } else dopusk = dopusk.replace("VD", "");
            } else{
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕТ ДОПУСКОВ В ДИРЕКТИВЕ (СТРОКА %1)").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            if (!printMessage.isEmpty()){
                printMessage.append(dir.directive);
                if (!react.isEmpty()) printMessage.append(" " + react);
                printMessage.append("\n");
            }
            printMessage.append("\t\t\t" + param + " =\t" + QString::number(val) + "\n");
            printMessage.append(QString("\t\t\t    ") + dopusk);
            if (constValues::isImitMode.load() != 1 && !test){
                if (reactMode == reactType::STOP) {
                    stopProg = true;
                    stopMessageStr = "ДИРЕКТИВА ПРОВЕРКА: ПАРАМЕТР НЕ В ДОПУСКЕ";
                }
                if (reactMode != reactType::SLED) this->GL_NORM_STATUS = false;
                printMessage.append("\n\t\t\tПараметр не в допуске");
                //printMessage.prepend("<span style='color: red; white-space: pre;'>");
                //printMessage.append("</span>");
            } else GL_NORM_STATUS = true;
            //protocol->append(printMessage);
            {
                if (GL_NORM_STATUS) printInProt(printMessage, "0", textStyle());
                else printInProt(printMessage, "13", textStyle());
            }
            printMessage = "";
        }
        break;
    }
    case (DirectParser::TypeDirect::PROGRAM) :{
        if (programs.count() == 0 || dir.numDirect != 1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("НЕОЖИДАЕМАЯ ДИРЕКТИВА ПРОГРАМ"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        for (int row = 0; row < dir.testParamDirect.count(); ++row){
            if (dir.testParamDirect[row].count() != 2 || (row == 0 && dir.testParamDirect[row][1].join(" ").isEmpty())){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕ УДАЛОСЬ ПРОЧИТАТЬ ПАРАМЕТРЫ ДИРЕКТИВЫ ПРОГРАМ\n"));
                errorMessage.append("\t\t\tЗАВЕРШИТЕ ПРОГРАММУ КОМАНДОЙ ВЫХОД ИЛИ РВЫХОД");
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            if (row == 0) printMessage.append(dir.directive + " " + dir.testParamDirect[row][1].join(" "));
            else {printMessage.append("\t\t\t" + dir.testParamDirect[row][1].join(" "));}
            //protocol->append(printMessage);
            {
                printInProt(printMessage, "1", textStyle());
            }
            printMessage = "";
        }
        break;
    }
    case (DirectParser::TypeDirect::PSI) : {
        qDebug() << "------------------------------------------------------------------";
        if (socketCanal1->state() != QAbstractSocket::ConnectedState || socketCanal2->state() != QAbstractSocket::ConnectedState){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНЕТ СВЯЗИ С НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        emit v100Selected();
        v100Mode = 1;

        DirectParser::Direct prc_dir(dir);
        prc_dir.direct = DirectParser::TypeDirect::PRC;
        prc_dir.directive = "ПСИ";
        bool res = runDirectFunc(prc_dir);

        emit v100Canceled();
        v100Mode = 0;
        if (!res) return false;
        qDebug() << "пока не реализовано"; break;
    }
    case (DirectParser::TypeDirect::PRC) : {
        if (dir.testParamDirect.count() > 102){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНЕДОПУСТИМОЕ КОЛИЧЕСТВО СТРОК В ДИРЕКТИВЕ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if (dir.testParamDirect.length() < 2 || dir.testParamDirect[0].length() != 2 || dir.testParamDirect[0][1].length() > 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append(QString("\t\t\tНедопустимое количество параметров директивы %1 (строка 1)").arg(dir.directive));
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if (dir.testParamDirect[0][1].length() == 2 && dir.testParamDirect[0][1][1] != "СТОП" && dir.testParamDirect[0][1][1] != "СЛЕД"){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append(QString("\t\t\tНедопустимый параметр РЕАК директивы %1 (строка 1)").arg(dir.directive));
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if (dir.testParamDirect[1].length() != 2 || dir.testParamDirect[1][1].length() < 2 || dir.testParamDirect[1][1].length() > 3){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append(QString("\t\t\tНедопустимое количество параметров директивы %1 (строка 2)").arg(dir.directive));
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        for (int i = 2; i < dir.testParamDirect.length(); ++i){
            if (dir.testParamDirect[2].length() != 2 || dir.testParamDirect[i][1].length() < 1 || dir.testParamDirect[i][1].length() > 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("\t\t\tНедопустимое количество параметров директивы %1 (строка %2)").arg(dir.directive).arg(i + 1));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
        }
        if (socketCanal1->state() != QAbstractSocket::ConnectedState || socketCanal2->state() != QAbstractSocket::ConnectedState){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНЕТ СВЯЗИ С НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        int zdr{1};
        bool ok{false};
        //bool stopReactFlag{false};
        reactType reactMode = reactType::NO_REACT;
        QStringList param = dir.testParamDirect[0][1];
        if (param.length() >= 1){
            param[0].toInt(&ok);
            if (ok){
                zdr = param[0].toInt();
                param.removeFirst();
            }
        }
        if (param.length() >= 1){
            if (param[0] != "СТОП" && param[0] != "СЛЕД"){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\tНедопустимое значение параметра РЕАК");
                printInProt(errorMessage, "13", textStyle());
                return false;
            } else{
                //stopReactFlag = true;
                if (param[0] == "СТОП") reactMode = reactType::STOP;
                else reactMode = reactType::SLED;
            }
        }

        param = dir.testParamDirect[1][1];

        double ndop{0};
        ndop = param[1].toDouble(&ok);
        if (!ok || ndop < 0){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append(QString("\t\t\tНедопустимое значение нижнего допуска"));
            printInProt(errorMessage, "13", textStyle());
            return false;
        }

        QStringList contacts;
        contacts << param[0];
        QStringList rrPars;
        QString rrPar;
        QString errorString;

        if (param.length() == 3 && !param[2].isEmpty()){
            rrPar = param[2];
            if (rrPar[0] != "=" && !rrPar.isEmpty()){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("\t\tПеред идентификатором РР параметра должен стоять знак =\n").arg(rrPar));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            rrPar = rrPar.mid(1);
            RRParam rrParam(rrPar);
            if (!rrParam.isValid()){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("\t\tРР параметр %1 не является валидным!\n").arg(rrPar));
                if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
        } else rrPar = "";
        rrPars << rrPar;
        for (int i = 2; i < dir.testParamDirect.length(); ++i){
            if (contacts.contains(dir.testParamDirect[i][1][0])){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("\t\tНедопустимо повторное подклюение точки (%1)").arg(dir.testParamDirect[i][1][0]));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            contacts << dir.testParamDirect[i][1][0];
            if (dir.testParamDirect[i][1].length() == 2 && !dir.testParamDirect[i][1][1].isEmpty()){
                rrPar = dir.testParamDirect[i][1][1];
                if (rrPar[0] != "=" && !rrPar.isEmpty()){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\tПеред идентификатором РР параметра должен стоять знак =\n").arg(rrPar));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                rrPar = rrPar.mid(1);
                RRParam rrParam(rrPar);
                if (!rrParam.isValid()){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\tРР параметр %1 не является валидным!").arg(rrPar));
                    if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
            }
            else rrPar = "";
            rrPars << rrPar;
        }
        if (contacts.count() <= 1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append(QString("\t\t\tДолжно быть использовано хотя бы 2 контакта"));
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        QList<int> numContacts;
        for (const QString& contact : contacts){
            if (!appcpParam.contains(contact)){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("\t\t\tНет идентификатора в БД (%1)").arg(contact));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            int numCont;
            if (appcpParam.value(contact).kont != 101) numCont = (appcpParam.value(contact).raz - 1) * 50 + appcpParam.value(contact).kont - 1;
            else numCont = 100;
            numContacts << numCont;
        }

        printMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" "));
        printInProt(printMessage, "0", textStyle());
        char c[4];// = [0x0D, 0x01];
        c[0] = char(0x0A);
        bool status;
        sendMessageToNU(c, 1, &status);
        if (!status) return false;
        if (ost_flag.load() == 1){
            //if (dir.numDirect != -1) programs.last().numDir -= 1;
            QByteArray cBAReset;
            cBAReset.clear();
            cBAReset.append(char(0x0a));
            bool statusReset{false};
            sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
            //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
            goto end_metka;
        }
        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }

        QByteArray cBA;

        cBA.append(char(0x20));
        cBA.append(char(1));
        cBA.append(char(numContacts.length()));
        cBA.append(char(0));


        for (int i = 0; i < numContacts.length(); ++i){
            cBA.append(char(numContacts[i]));
        }

        sendMessageToNU(cBA.data(), cBA.length(), &status);
        if (!status) return false;
        if (ost_flag.load() == 1){
            //if (dir.numDirect != -1) programs.last().numDir -= 1;
            QByteArray cBAReset;
            cBAReset.clear();
            cBAReset.append(char(0x0a));
            bool statusReset{false};
            sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
            //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
            goto end_metka;
        }
        if ((respondNU.length() == 2 || respondNU.length() == 3) && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПСИ", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2 && respondNU.length() != 3){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 или 3 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append(QString("\t\t\t\tОшибка в аппаратуре НУ. Не удалось подключить точку: %3").arg(respondNU[2]));
            printInProt(errorMessage, "13", textStyle());
            return false;
        }

        cBA.clear();

        cBA.append(char(0x21));
        //передаем diap и napr;
        int diap;
        if (ndop < 1000.0) diap = 1;
        else if (ndop < 5000.0) diap = 2;
        else diap = 3;

        cBA.append(char(diap));
        cBA.append(char(this->v100Mode));
        cBA.append(char(zdr));
        cBA.append(char(contacts.count()));

        sendMessageToNU(cBA.data(), cBA.length() - 1, &status);
        if (!status) return false;
        if (ost_flag.load() == 1){
            //if (dir.numDirect != -1) programs.last().numDir -= 1;
            QByteArray cBAReset;
            cBAReset.clear();
            cBAReset.append(char(0x0a));
            bool statusReset{false};
            sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
            //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
            goto end_metka;
        }

        if (respondNU.length() < 4){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось как минимум 4 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        qint16 count_zamer{0};
        QByteArray data = respondNU.mid(2, 2);
        std::memcpy(&count_zamer, data.constData(), sizeof(count_zamer));

        if (count_zamer != numContacts.length()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (количество полученных замеров не соотвествует количеству запрошенных замеров)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if (respondNU.length() < 4 + count_zamer * 6){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (количество полученных байт не соответствует количество проведенных измерений)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        QMap<short, float> results;
        for (int i = 0; i < count_zamer; ++i){
            data = respondNU.mid(4 + 6 * i, 6);
            QByteArray numPointData = data.left(2);
            qint16 numPoint{0};
            std::memcpy(&numPoint, numPointData.constData(), sizeof(numPoint));
            QByteArray resData = data.mid(2);
            float res{0};
            std::memcpy(&res, resData.constData(), sizeof(res));
            if (std::isnan(res) || std::isinf(res)){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (неудалось получить значение)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            results.insert(numPoint, res);
        }

        if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append(QString("\t\t\t\tОшибка в аппаратуре НУ. Не удалось подключить точку: %3").arg(respondNU[2]));
            printInProt(errorMessage, "13", textStyle());
            return false;
        }

        cBA.clear();

        for (const int contact : results.keys()){
            int index = -1;
            for (int i = 0; i < numContacts.length(); ++i){
                if (contact == numContacts[i]){
                    index = i;
                    break;
                }
            }
            if (index == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\tПолучем замер на контакт, который не запрашивался");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            if (!rrPars.at(index).isEmpty()){
                //qDebug() << rrPars.at(index + 1);
                RRParam rrParam(rrPars.at(index));
                if (!rrParam.setValue(results.value(contact))){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\tНе удалось записать значение РР параметра!").arg(rrPar));
                    if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
            }
        }

        QString tableSplit = QString("|----------------|-----------|-----------|-------------------------|\n");
        QString table;
        table.append("|----------------|-----------|-----------|-------------------------|\n");
        table.append("|      ИД1(-)    | знач.(КОм)|  н.доп.   |           ПАР           |\n");
        table.append("|----------------|-----------|-----------|-------------------------|\n");
        printInProt(table, "0", textStyle());
        table.clear();

        QList<int> failureContact;
        for (int row = 0; row < contacts.length(); ++row){
            QStringList infoSections;
            infoSections << contacts[row] << QString::number(results.value(numContacts[row]), 'f', 4);
            if (ndop != -1) infoSections << QString::number(ndop);
            else infoSections << QString();
            infoSections << rrPars[row];
            QString tableRow = "|";
            for (int i = 0; i < infoSections.length(); i++){
                QString infoSection = infoSections[i];
                if (infoSection.length() > tableSplit.split("\n")[0].split("|")[i + 1].length()){
                    infoSection = infoSection.left(tableSplit.split("\n")[0].split("|")[i + 1].length());
                }
                int needSpace = tableSplit.split("\n")[0].split("|")[i + 1].length() - infoSection.length();
                int needSpaceLeft = needSpace / 2 + needSpace % 2;
                int needSpaceRight = needSpace / 2;
                infoSection.prepend(QString(needSpaceLeft, QChar(' ')));
                infoSection.append(QString(needSpaceRight, QChar(' ')));
                tableRow.append(infoSection + "|");
            }
            table.append(tableRow + "\n");
            if (constValues::isImitMode.load() != 1 && results.value(numContacts[row]) < ndop){
                failureContact.append(numContacts[row]);
                printInProt(table, "13", textStyle());
                table.clear();
            }
            table.append("|----------------|-----------|-----------|-------------------------|\n");
            printInProt(table, "0", textStyle());
            table.clear();
        }
        if (!failureContact.isEmpty()){
            if (failureContact.count() == contacts.count()){
                if (/*stopReactFlag*/ reactMode == reactType::STOP) {
                    stopProg = true;
                    stopMessageStr = QString("ДИРЕКТИВА %1: ЕСТЬ ТОЧКИ НЕ В ДОПУСКЕ").arg(dir.directive);
                }
                if (reactMode != reactType::SLED) this->GL_NORM_STATUS = false;
                printInProt("ВСЕ ТОЧКИ НЕ В ДОПУСКЕ", "13", textStyle());
                break;
            }
            printInProt("ЕСТЬ ТОЧКИ НЕ В ДОПУСКЕ, ПОДКЛЮЧАЕМ ИХ ПО ОДНОЙ", "13", textStyle());
            cBA.clear();
            cBA.append(char(0x20));
            cBA.append(char(0));
            cBA.append(char(failureContact.length()));
            cBA.append(char(0));
            for (int i = 0; i < failureContact.length(); ++i){
                cBA.append(char(failureContact[i]));
            }
            sendMessageToNU(cBA.data(), cBA.length(), &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if ((respondNU.length() == 2 || respondNU.length() == 3) && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСИ", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2 && respondNU.length() != 3){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 или 3 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("\t\t\t\tОшибка в аппаратуре НУ. Не удалось подключить точку: %3").arg(respondNU[2]));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }

            cBA.clear();

            //for (int contIndex = 0; contIndex < failureContact.length(); ++contIndex){
            bool endRes{true};
            while (!failureContact.isEmpty()){
                cBA.clear();
                cBA.append(char(0x20));
                cBA.append(1);
                cBA.append(1);
                cBA.append(char(0));
                cBA.append(failureContact[0]);
                sendMessageToNU(cBA.data(), cBA.length(), &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
                if ((respondNU.length() == 2 || respondNU.length() == 3) && respondNU.at(1) == 0){
                    //printInProt("NET: получили ответ на ПСИ", "30", textStyle());
                    printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                    printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
                }
                else if (respondNU.length() != 2 && respondNU.length() != 3){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 или 3 байта)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                else if (respondNU.at(1) == -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\t\t\tОшибка в аппаратуре НУ. Не удалось подключить точку: %3").arg(respondNU[2]));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }

                cBA.clear();

                cBA.append(char(0x21));

                cBA.append(char(diap));
                cBA.append(char(this->v100Mode));
                cBA.append(char(zdr));
                cBA.append(char(contacts.count() - failureContact.count() + 1));

                sendMessageToNU(cBA.data(), cBA.length() - 1, &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }

                if (respondNU.length() < 4){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось как минимум 4 байта)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                qint16 count_zamer{0};
                QByteArray data = respondNU.mid(2, 2);
                std::memcpy(&count_zamer, data.constData(), sizeof(count_zamer));

                if (count_zamer != contacts.count() - failureContact.count() + 1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (количество полученных замеров не соотвествует количеству запрошенных замеров)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                if (respondNU.length() < 4 + count_zamer * 6){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (количество полученных байт не соответствует количество проведенных измерений)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                QMap<short, float> results;
                for (int i = 0; i < count_zamer; ++i){
                    data = respondNU.mid(4 + 6 * i, 6);
                    QByteArray numPointData = data.left(2);
                    qint16 numPoint{0};
                    std::memcpy(&numPoint, numPointData.constData(), sizeof(numPoint));
                    QByteArray resData = data.mid(2);
                    float res{0};
                    std::memcpy(&res, resData.constData(), sizeof(res));
                    if (std::isnan(res) || std::isinf(res)){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (неудалось получить значение)");
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                    results.insert(numPoint, res);
                }

                if (respondNU.at(1) == -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\t\t\tОшибка в аппаратуре НУ. Не удалось подключить точку: %3").arg(respondNU[2]));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }

                cBA.clear();

                for (const int contact : results.keys()){
                    int index = -1;
                    for (int i = 0; i < numContacts.length(); ++i){
                        if (contact == numContacts[i]){
                            index = i;
                            break;
                        }
                    }
                    if (index == -1){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        errorMessage.append("\t\tПолучем замер на контакт, который не запрашивался");
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                    if (!rrPar.isEmpty()){
                        RRParam rrParam(rrPars.at(index));
                        if (!rrParam.setValue(results.value(contact))){
                            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                            errorMessage.append(QString("\t\tНе удалось записать значение РР параметра!").arg(rrPar));
                            if (rrParam.isHasError()) errorMessage.append("\n\t\t\t" + rrParam.getErrorText());
                            printInProt(errorMessage, "13", textStyle());
                            return false;
                        }
                    }
                }

                QString tableSplit = QString("|----------------|-----------|-----------|-------------------------|\n");
                QString table;
                table.append("|----------------|-----------|-----------|-------------------------|\n");
                table.append("|      ИД1(-)    | знач.(КОм)|  н.доп.   |           ПАР           |\n");
                table.append("|----------------|-----------|-----------|-------------------------|\n");
                printInProt(table, "0", textStyle());
                table.clear();

                //QList<int> failureContact;
                for (int row = 0; row < contacts.length(); ++row){
                    if (failureContact.contains(numContacts[row]) && failureContact[0] != numContacts[row]) continue;
                    QStringList infoSections;
                    infoSections << contacts[row] << QString::number(results.value(numContacts[row]), 'f', 4);
                    if (ndop != -1) infoSections << QString::number(ndop);
                    else infoSections << QString();
                    infoSections << rrPars[row];
                    QString tableRow = "|";
                    for (int i = 0; i < infoSections.length(); i++){
                        QString infoSection = infoSections[i];
                        if (infoSection.length() > tableSplit.split("\n")[0].split("|")[i + 1].length()){
                            infoSection = infoSection.left(tableSplit.split("\n")[0].split("|")[i + 1].length());
                        }
                        int needSpace = tableSplit.split("\n")[0].split("|")[i + 1].length() - infoSection.length();
                        int needSpaceLeft = needSpace / 2 + needSpace % 2;
                        int needSpaceRight = needSpace / 2;
                        infoSection.prepend(QString(needSpaceLeft, QChar(' ')));
                        infoSection.append(QString(needSpaceRight, QChar(' ')));
                        tableRow.append(infoSection + "|");
                    }
                    table.append(tableRow + "\n");
                    if (results.value(numContacts[row]) < ndop){
                        //failureContact.append(numContacts[row]);
                        endRes = false;
                        printInProt(table, "13", textStyle());
                        table.clear();
                    }
                    table.append("|----------------|-----------|-----------|-------------------------|\n");
                    printInProt(table, "0", textStyle());
                    table.clear();
                }

                failureContact.removeFirst();
            }
            if (endRes){
                this->GL_NORM_STATUS = true;
                printInProt(QString("%1: НОРМА ОПЕРАЦИИ").arg(dir.directive), "0", textStyle());
            } else{
                if (/*stopReactFlag*/ reactMode == reactType::STOP){
                    stopProg = true;
                    stopMessageStr = QString("ДИРЕКТИВА %1: ЕСТЬ ТОЧКИ НЕ В ДОПУСКЕ").arg(dir.directive);
                }
                if (reactMode != reactType::SLED) this->GL_NORM_STATUS = false;
                printInProt(QString("%1: НЕНОРМА ОПЕРАЦИИ").arg(dir.directive), "13", textStyle());
            }
        } else{
            this->GL_NORM_STATUS = true;
            printInProt(QString("%1: НОРМА ОПЕРАЦИИ").arg(dir.directive), "0", textStyle());
        }
        //table.chop(1);

        //printInProt(table, "0", textStyle());

        cBA.append(0x0A);
        sendMessageToNU(cBA.data(), cBA.length(), &status);
        if (!status) return false;
        if (ost_flag.load() == 1){
            //if (dir.numDirect != -1) programs.last().numDir -= 1;
            QByteArray cBAReset;
            cBAReset.clear();
            cBAReset.append(char(0x0a));
            bool statusReset{false};
            sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
            //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
            goto end_metka;
        }
        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }




        break;
    }
    case (DirectParser::TypeDirect::PSC) : {
        if (dir.testParamDirect.count() > 101){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНЕДОПУСТИМОЕ КОЛИЧЕСТВО СТРОК В ДИРЕКТИВЕ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if (dir.testParamDirect.length() < 3 || dir.testParamDirect[1][1].length() != 1 || dir.testParamDirect[2][1].length() < 1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append(QString("\t\t\tНедопустимое количество параметров директивы %1").arg(dir.directive));
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if (socketCanal1->state() != QAbstractSocket::ConnectedState || socketCanal2->state() != QAbstractSocket::ConnectedState){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНЕТ СВЯЗИ С НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }

        int zdr = 1;
        QStringList param = dir.testParamDirect[0][1];
        if (param.length() > 0){
            bool ok{false};
            param[0].toInt(&ok);
            if (ok){
                zdr = param[0].toInt();
                param.removeFirst();
            }
        }
        /*bool stopFlag = false;*/
        reactType reactMode = reactType::NO_REACT;
        if (param.length() > 0){
            if (param[0] == "СТОП"){
                //stopFlag = true;
                reactMode = reactType::STOP;
            } else if (param[0] == "СЛЕД"){
                reactMode = reactType::SLED;
            }
            else{
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\tНЕДОПУСТИМОЕ ЗНАЧЕНИЕ ДЛЯ РЕАК");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
        }

        printMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" "));
        printInProt(printMessage, "0", textStyle());

        QString point1 = dir.testParamDirect[1][1][0];
        QStringList points;
        QList<double> ndops;
        QList<double> vdops;
        QStringList rrPars;
        points << point1;
        ndops << -1;
        vdops << -1;
        rrPars << "";
        for (int i = 2; i < dir.testParamDirect.length(); ++i){
            param = dir.testParamDirect[i][1];
            if (param.length() > 4){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("\t\t\tНЕДОПУСТИМОЕ КОЛИЧЕСТВО АРГУМЕНТОВ В СТРОКЕ %1").arg(i + 1));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            if (points.contains(param[0])){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("\t\tНедопустимо повторное подключение точки (%1)").arg(param[0]));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            points << param[0];
            ndops << -1;
            vdops << -1;
            rrPars << "";
            param.removeFirst();
            if (param.length() > 0){
                bool ok{false};
                param[0].toDouble(&ok);
                if (ok){
                    ndops[i - 1] = param[0].toDouble();
                    param.removeFirst();
                }
                if (param.length() > 0){
                    param[0].toDouble(&ok);
                    if (ok){
                        vdops[i - 1] = param[0].toDouble();
                        if (ndops[i - 1] > vdops[i - 1]){
                            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                            errorMessage.append(QString("Нижний допуск не должен превышвать верхний (точка: %1)").arg(points.last()));
                            printInProt(errorMessage, "13", textStyle());
                            return false;
                        }
                        param.removeFirst();
                    }
                }
                if (param.length() > 0 && param[0][0] == "="){
                    rrPars[i - 1] = param[0].mid(1);
                    param.removeFirst();
                }
                if (param.length() > 0){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\t\tПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ (СТРОКА %1)").arg(i + 1));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
            }
        }

        QList<int> numContacts;
        for (int i = 0; i < points.length(); ++i){
            if (!appcpParam.contains(points[i])){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("\t\t\tНет идентификатора в БД (%1)").arg(points[i]));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            int numCont;
            if (appcpParam.value(points[i]).kont != 101) numCont = (appcpParam.value(points[i]).raz - 1) * 50 + appcpParam.value(points[i]).kont - 1;
            else numCont = 100;
            numContacts << numCont;

            if (!rrPars[i].isEmpty()){
                RRParam rrPar(rrPars[i]);
                if (!rrPar.isValid()){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\tРР параметр %1 не является валидным!").arg(rrPars[i]));
                    errorMessage.append(rrPar.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
            }
        }

        QByteArray cBA;
        cBA.clear();
        cBA.append(char(0x0a));
        bool status{false};
        sendMessageToNU(cBA.constData(), cBA.length(), &status);
        if (!status) return false;
        if (ost_flag.load() == 1){
            //if (dir.numDirect != -1) programs.last().numDir -= 1;
            QByteArray cBAReset;
            cBAReset.clear();
            cBAReset.append(char(0x0a));
            bool statusReset{false};
            sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
            //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
            goto end_metka;
        }
        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        cBA.clear();

        cBA.append(char(0x01));
        cBA.append(char(1));
        cBA.append(char(numContacts[0]));
        sendMessageToNU(cBA.constData(), cBA.length(), &status);
        if (!status) return false;
        if (ost_flag.load() == 1){
            //if (dir.numDirect != -1) programs.last().numDir -= 1;
            QByteArray cBAReset;
            cBAReset.clear();
            cBAReset.append(char(0x0a));
            bool statusReset{false};
            sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
            //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
            goto end_metka;
        }
        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        cBA.clear();
        QList<float> results;
        QList<int> diap;
        QString tableSplit("|----------------|----------------|-----------|-----------|-----------|-------------------------|\n");
        QString table;
        table.append("|----------------|----------------|-----------|-----------|-----------|-------------------------|\n");
        table.append("|      ИД1(-)    |      ИД2(+)    | знач.(КОм)|  н.доп.   |   в.доп.  |           ПАР           |\n");
        table.append("|----------------|----------------|-----------|-----------|-----------|-------------------------|\n");
        printInProt(table, "0", textStyle());
        table.clear();

        for (int i = 1; i < numContacts.length(); ++i){
            cBA.append(char(0x02));
            cBA.append(char(1));
            cBA.append(char(numContacts[i]));
            sendMessageToNU(cBA.constData(), cBA.length(), &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if (respondNU.length() == 2 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            cBA.clear();

            int diapVal;

            /*if (ndops[i] == -1 || (int(ndops[i] * 100000) <= 0.05 * 100000 && int(vdops[i]*100000) <= 0.05 *100000)) diap = 0;
            else if (ndops[i] <= 1000 && vdops[i] <= 1000) diap = 1;
            else if (ndops[i] <= 5000 && vdops[i] <= 5000) diap = 2;
            else diap = 3;*/
            if (ndops[i] == -1 || (ndops[i] <= 0.05)) diapVal = 0;
            else if (ndops[i] <= 1000.0) diapVal = 1;
            else if (ndops[i] <= 5000.0) diapVal = 2;
            else diapVal = 3;

            diap.append(diapVal);

            if (diapVal == 0){
                if (this->v100Mode) {
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\t\tДИРЕКТИВА НЕВЫПОЛНИМА nd &lt; 1КОм ЗАМЕР НЕВОЗМОЖЕН ПРИ 100В"));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                cBA.append(char(0x09));
                QThread::sleep(zdr);
                sendMessageToNU(cBA.constData(), cBA.length(), &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
            } else{
                cBA.append(char(0x08));
                cBA.append(char(diapVal));
                cBA.append(char(v100Mode));
                cBA.append(char(zdr));
                sendMessageToNU(cBA.constData(), cBA.length(), &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
            }
            if (!status) return false;
            if (respondNU.length() == 2 + 4 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                QByteArray floatData = respondNU.mid(2);
                float result;
                std::memcpy(&result, floatData.constData(), sizeof(result));
                if (std::isnan(result) || std::isinf(result)){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (неудалось получить значение)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                results << result;
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2\n   res = %3").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])).arg(result), "35", textStyle(), true, true);

                if (!rrPars[i].isEmpty()){
                    qDebug() << rrPars[i];
                    RRParam rrPar(rrPars[i]);
                    if (!rrPar.setValue(result)){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        errorMessage.append(QString("\t\tНе удалось записать значение РР параметра!").arg(rrPar.getFullParamName()));
                        if (rrPar.isHasError()) errorMessage.append("\t\t\t" + rrPar.getErrorText());
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                }
            }
            else if (respondNU.length() != 2 + 4){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 6 байт)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }

            cBA.clear();

            cBA.append(char(0x02));
            cBA.append(char(0));
            cBA.append(char(numContacts[i]));

            sendMessageToNU(cBA.constData(), cBA.length(), &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if (respondNU.length() == 2 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            cBA.clear();

            int row = i - 1;
            QStringList infoSections;
            infoSections << points[0] << points[row + 1] /*<< QString::number(results[row], 'f', 6)*/;
            if (ndops[row + 1] == -1 || (ndops[row + 1] <= 0.05)) infoSections << QString::number(results[row], 'f', 6);
            else infoSections << QString::number(results[row], 'f', 4);
            if (ndops[row + 1] >= 0) infoSections << QString::number(ndops[row + 1]);
            else infoSections << "";
            if (vdops[row + 1] >= 0) infoSections << QString::number(vdops[row + 1]);
            else infoSections << "";
            infoSections << rrPars[row + 1];
            QString tableRow = "|";
            for (int i = 0; i < infoSections.length(); i++){
                QString infoSection = infoSections[i];
                if (infoSection.length() > tableSplit.split("\n")[0].split("|")[i + 1].length()){
                    infoSection = infoSection.left(tableSplit.split("\n")[0].split("|")[i + 1].length());
                }
                int needSpace = tableSplit.split("\n")[0].split("|")[i + 1].length() - infoSection.length();
                int needSpaceLeft = needSpace / 2 + needSpace % 2;
                int needSpaceRight = needSpace / 2;
                infoSection.prepend(QString(needSpaceLeft, QChar(' ')));
                infoSection.append(QString(needSpaceRight, QChar(' ')));
                tableRow.append(infoSection + "|");
            }
            table.append(tableRow);
            if (diap[row] == 0){
                QString tempTableSplit = tableSplit;
                tempTableSplit = tempTableSplit.replace("-", " ");
                QStringList tableSplitList = tempTableSplit.split("|");
                int lenCol = tableSplitList[2 + 1].length();
                tableSplitList[2 + 1] = QString("(") + QString::number(infoSections[2].toDouble() * 1000, 'f', 2) + " Ом)";
                int needSpace = lenCol - tableSplitList[2 + 1].length();
                int needSpaceLeft = needSpace / 2 + needSpace % 2;
                int needSpaceRight = needSpace / 2;
                tableSplitList[2+1].prepend(QString(needSpaceLeft, QChar(' ')));
                tableSplitList[2+1].append(QString(needSpaceRight, QChar(' ')));
                tableRow = tableSplitList.join("|");
                table.append("\n" + tableRow);
            }
            if (constValues::isImitMode.load() != 1 && ((results.value(/*numContacts[row]*/row) < ndops[row + 1] && ndops[row + 1] != -1) || (results.value(/*numContacts[row]*/row) > vdops[row + 1] && vdops[row + 1] != -1))){
                if (reactMode == reactType::STOP) {
                    stopProg = true;
                    stopMessageStr = QString("ДИРЕКТИВА %1: ЕСТЬ ТОЧКИ НЕ В ДОПУСКЕ").arg(dir.directive);
                }
                /*if (reactMode != reactType::SLED)*/ this->GL_NORM_STATUS = false;
                printInProt(table, "13", textStyle());
                table.clear();
            } else{
                printInProt(table, "0", textStyle());
                table.clear();
            }
            table.append(tableSplit);
            printInProt(table, "0", textStyle());
            table.clear();
        }

        cBA.append(char(0x01));
        cBA.append(char(0));
        cBA.append(char(numContacts[0]));

        sendMessageToNU(cBA.constData(), cBA.length(), &status);
        if (!status) return false;
        if (ost_flag.load() == 1){
            //if (dir.numDirect != -1) programs.last().numDir -= 1;
            QByteArray cBAReset;
            cBAReset.clear();
            cBAReset.append(char(0x0a));
            bool statusReset{false};
            sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
            //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
            goto end_metka;
        }
        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }

        cBA.clear();
        cBA.append(char(0x0a));
        sendMessageToNU(cBA.constData(), cBA.length(), &status);
        if (!status) return false;
        if (ost_flag.load() == 1){
            //if (dir.numDirect != -1) programs.last().numDir -= 1;
            QByteArray cBAReset;
            cBAReset.clear();
            cBAReset.append(char(0x0a));
            bool statusReset{false};
            sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
            //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
            goto end_metka;
        }
        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        cBA.clear();


        /*for (int row = 0; row < results.count(); ++row){

        }*/

        if (GL_NORM_STATUS){
            printInProt(QString("\t\t\t%1 норма операции").arg(dir.directive), "0", textStyle());
        } else{
            printInProt(QString("\t\t\t%1 ненорма операции").arg(dir.directive), "13", textStyle());
            if (reactMode == reactType::SLED) GL_NORM_STATUS = true;
        }
        break;
    }
    case (DirectParser::TypeDirect::PSC_R) :{
            if (dir.testParamDirect.length() != 1 || dir.testParamDirect[0].length() != 2 || dir.testParamDirect[0][1].length() < 2 || dir.testParamDirect[0][1].length() > 6){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("\t\t\tНедопустимое количество параметров директивы %1").arg(dir.directive));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            if (socketCanal1->state() != QAbstractSocket::ConnectedState || socketCanal2->state() != QAbstractSocket::ConnectedState){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\tНЕТ СВЯЗИ С НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            QStringList param = dir.testParamDirect[0][1];
            bool ok{false};
            int zdr = 1;
            param[0].toInt(&ok);
            if (dir.testParamDirect[0][1].length() > 2 && ok){
                zdr = dir.testParamDirect[0][1][0].toInt();
                param.removeFirst();
            }
            QString par1;
            QString par2;

            par1 = param[0];
            par2 = param[1];

            if (!appcpParam.contains(par1) || !appcpParam.contains(par2)){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\tНет идентификатора в БД");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            if (par1 == par2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("Недопустимо повторное подключение точик (%1)").arg(par1));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            int numCont1;
            int numCont2;
            if (appcpParam.value(par1).kont != 101) numCont1 = (appcpParam.value(par1).raz - 1) * 50 + appcpParam.value(par1).kont - 1;
            else numCont1 = 100;
            if (appcpParam.value(par2).kont != 101) numCont2 = (appcpParam.value(par2).raz - 1) * 50 + appcpParam.value(par2).kont - 1;
            else numCont2 = 100;

            if (numCont1 == numCont2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\tДля замера необходимо использовать два РАЗЛИЧНЫХ контакта");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }

            /*qDebug() << "CONT1: " << numCont1;
            qDebug() << "CONT2: " << numCont2;
            qDebug() << appcpParam.value("ZP_KORPUS").raz;
            qDebug() << appcpParam.value("ZP_KORPUS").kont;*/

            double nDop{-1};
            double vDop{-1};
            if (param.length() > 2){
                param[2].toFloat(&ok);
                if (ok){
                    nDop = param[2].toFloat();
                    if (param.length() > 3){
                        param[3].toFloat(&ok);
                        if (ok){
                            vDop = param[3].toFloat(&ok);
                            param.removeAt(3);
                        }
                    }
                    param.removeAt(2);
                }
            }

            if (nDop > vDop && vDop != -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\tНижний допуск не должен превышать верхний!");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            //qDebug() << "NDOP: " << nDop;
            //qDebug() << "VDOP: " << vDop;


            int diap;
            /*if (nDop == -1 || (int(nDop * 100000) <= 0.05 * 100000 && int(vDop*100000) <= 0.05 *100000)) diap = 0;
            else if (nDop <= 1000 && vDop <= 1000) diap = 1;
            else if (nDop <= 5000 && vDop <= 5000) diap = 2;
            else diap = 3;*/
            if (nDop == -1 || (nDop <= 0.05)) diap = 0;
            else if (nDop <= 0.1) diap = 4;
            else if (nDop <= 1) diap = 5;
            else if (nDop <= 10) diap = 6;
            else if (nDop <= 100) diap = 7;
            else if (nDop <= 1000.0) diap = 1;
            else if (nDop <= 5000.0) diap = 2;
            else diap = 3;

            float result{0};

            //bool voltReady = false;

            //if (diap >= 4 && diap <=6) voltReady = haveVolt();


            printMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" "));
            printInProt(printMessage, "0", textStyle());
            char c[4];// = [0x0D, 0x01];
            c[0] = char(0x0A);
            bool status;
            sendMessageToNU(c, 1, &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if (respondNU.length() == 2 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            c[0] = char(0x01);
            c[1] = char(1);
            c[2] = char(numCont1);
            sendMessageToNU(c, 3, &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if (respondNU.length() == 2 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }

            c[0] = char(0x02);
            c[1] = char(1);
            c[2] = char(numCont2);
            sendMessageToNU(c, 3, &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if (respondNU.length() == 2 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }



            if (diap == 0){
                if (this->v100Mode) {
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\t\tДИРЕКТИВА НЕВЫПОЛНИМА nd &lt; 1КОм ЗАМЕР НЕВОЗМОЖЕН ПРИ 100В"));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                c[0] = char(0x09);
                QThread::sleep(zdr);
                sendMessageToNU(c, 1, &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
            } else{
                /*if (diap >= 4 && !voltReady) {
                    if (hasVoltMode) printInProt("\t\t\tВольтметр не подключен. Замеры выполняются с большой погрешностью", "13");
                    diap = 1;
                }
                if (diap >= 4) {
                    result = getRWithVolt(diap-2);
                    if (result == -1) {
                        status = false;
                        if (!resetVolt()) {
                            QMessageBox::critical(nullptr, "Критическая ошибка", "Не удалось перевести вольтметр в защищенный режим. Все измерения заблокированы! Отключите вольтметр и перезагрузите ПО ВУ");
                            blockAllIsm = true;
                        }
                    }
                }*/
                if (diap >= 4 && !hasVoltMode) {
                    diap = 1;
                }
                if (diap >=4) {
                    printInProt("\t\t\tВольтметр не подключен. Замеры выполняются с большой погрешностью", "13");
                    /*
                     * код для вольтметра
                    */
                } else {
                c[0] = char(0x08);
                c[1] = char(diap);
                c[2] = char(v100Mode);
                c[3] = char(zdr);
                sendMessageToNU(c, 4, &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
            }
            }
            if (!status) return false;
            if (diap < 4) {
            if (respondNU.length() == 2 + 4 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                QByteArray floatData = respondNU.mid(2);
                std::memcpy(&result, floatData.constData(), sizeof(result));
                if (std::isnan(result) || std::isinf(result)){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (неудалось получить значение)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }

                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2\n   res = %3").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])).arg(result), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2 + 4){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 6 байт)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            }

            QString rrParName;
            if (param.length() > 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n\t\t\t");
                if (param[2][0] != "="){
                    errorMessage.append("РР параметр должен записываться со знаком =");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                rrParName = param[2].mid(1);

                RRParam rrPar(rrParName);
                if (rrPar.isHasError()){
                    errorMessage.append(rrPar.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                if (!rrPar.isExist()){
                    if (rrPar.isHasError()) errorMessage.append(rrPar.getErrorText());
                    else errorMessage.append("РР ПАРАМЕТР ДЛЯ ЗАПИСИ РЕЗУЛЬТАТА ЕЩЕ НЕ СОЗДАН");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                rrPar.setValue(result);
                if (rrPar.isHasError()){
                    errorMessage.append(rrPar.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                /*QRegularExpression regex(R"(^FL.([A-Za-z0-9]{1,3})_?([A-Za-z0-9_\-\/\.]{1,15})?(?:\[(\d+)\])?$)");
                QRegularExpressionMatch match = regex.match(rrPar);
                if (!match.hasMatch()){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\tНедопустимое имя РР параметра для записи результата");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                QString errorString;
                if (!isParamExists(match.captured(1), match.captured(2), errorString)){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (errorString.isEmpty()) errorMessage.append("\t\t\tРР параметра для записи результата не создан");
                    else errorMessage.append(errorString);
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                bool isArray = isParamArray(match.captured(1), match.captured(2), errorString);
                if ((!match.captured(3).isEmpty() && !isArray) || (match.captured(3).isEmpty() && isArray)){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (errorString.isEmpty()) {
                        if (!isArray) errorMessage.append("\t\t\tРР параметр для записи результата не является массивом");
                        else errorMessage.append("\t\t\tРР параметр для записи результата является массивом. Требуется указать индекс!");
                    }
                    else errorMessage.append(errorString);
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }

                QString queryString;
                if (match.captured(3).isEmpty()) queryString = QString("UPDATE RR_PAR SET Val = %1 WHERE Bl_Name = '%2' AND Par_Name = '%3'").arg(result).arg(match.captured(1)).arg(match.captured(2));
                else queryString = QString("UPDATE RR_PAR SET Val = %1 WHERE Bl_Name = '%2' AND Par_Name = '%3' AND Index = %4").arg(result).arg(match.captured(1)).arg(match.captured(2)).arg(match.captured(3));
                QSqlQuery query = MainWindow::getQueryRRDB(queryString);
                if (!query.isActive()){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append("ОШИБКА ОБРАЩЕНИЯ К БД: " + query.lastError().text());
                    {
                        printInProt(errorMessage, "13", textStyle());
                    }
                    qDebug() << "QUERY ERROR";
                    return false;
                }
                if (query.numRowsAffected() <= 0){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append("ОШИБКА ЗАПИСИ РЕЗУЛЬТАТА В БД");
                    {
                        printInProt(errorMessage, "13", textStyle());
                    }
                    qDebug() << "ERROR WRITE RESULT";
                    return false;
                }
                query.clear();*/

            }

            QString table;
            QString tableSplit("|----------------|----------------|-----------|-----------|-----------|-------------------------|");
            table.append(tableSplit + "\n");
            table.append("|      ИД1(-)    |      ИД2(+)    | знач.(КОм)|  н.доп.   |   в.доп.  |           ПАР           |\n");
            table.append(tableSplit);
            printInProt(table, "0", textStyle());
            table = QString();

            QStringList infoSections;
            infoSections << par1 << par2 /*<< QString::number(result, 'f', 6)*/;
            if (nDop == -1 || (nDop <= 0.05)) infoSections << QString::number(result, 'f', 6);
            else infoSections << QString::number(result, 'f', 4);
            if (nDop != -1) infoSections << QString::number(nDop);
            else infoSections << QString();
            if (vDop != -1) infoSections << QString::number(vDop);
            else infoSections << QString();
            infoSections << rrParName;
            QString tableRow = "|";
            for (int i = 0; i < 6; i++){
                QString infoSection = infoSections[i];
                if (infoSection.length() > tableSplit.split("|")[i + 1].length()){
                    infoSection = infoSection.left(tableSplit.split("\n")[0].split("|")[i + 1].length());
                }
                int needSpace = tableSplit.split("|")[i + 1].length() - infoSection.length();
                int needSpaceLeft = needSpace / 2 + needSpace % 2;
                int needSpaceRight = needSpace / 2;
                infoSection.prepend(QString(needSpaceLeft, QChar(' ')));
                infoSection.append(QString(needSpaceRight, QChar(' ')));
                tableRow.append(infoSection + "|");
            }
            table.append(tableRow);
            if (diap == 0){
                QString tempTableSplit = tableSplit;
                tempTableSplit = tempTableSplit.replace("-", " ");
                QStringList tableSplitList = tempTableSplit.split("|");
                int lenCol = tableSplitList[2 + 1].length();
                tableSplitList[2 + 1] = QString("(") + QString::number(infoSections[2].toDouble() * 1000, 'f', 2) + " Ом)";
                int needSpace = lenCol - tableSplitList[2 + 1].length();
                int needSpaceLeft = needSpace / 2 + needSpace % 2;
                int needSpaceRight = needSpace / 2;
                tableSplitList[2+1].prepend(QString(needSpaceLeft, QChar(' ')));
                tableSplitList[2+1].append(QString(needSpaceRight, QChar(' ')));
                tableRow = tableSplitList.join("|");
                table.append("\n" + tableRow);
            }
            if (constValues::isImitMode.load() != 1 && ((nDop != 1 && result < nDop) || (vDop != -1 && result > vDop))){
                printInProt(table, "13", textStyle());
                this->GL_NORM_STATUS = false;
            } else{
                printInProt(table, "0", textStyle());
            }
            table = QString();
            table.append(tableSplit);
            table.chop(1);

            printInProt(table, "0", textStyle());
            if (!this->GL_NORM_STATUS){
                printInProt(QString("\t\t%1: НЕНОРМА ОПЕРАЦИИ").arg(dir.directive), "13", textStyle());
            } else{
                printInProt(QString("\t\t%1: НОРМА ОПЕРАЦИИ").arg(dir.directive), "0", textStyle());
            }

            c[0] = char(0x02);
            c[1] = char(0);
            c[2] = char(numCont2);
            sendMessageToNU(c, 3, &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if (respondNU.length() == 2 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }

            c[0] = char(0x01);
            c[1] = char(0);
            c[2] = char(numCont1);
            sendMessageToNU(c, 3, &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if (respondNU.length() == 2 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }




            break;
        }
    case (DirectParser::TypeDirect::PUSK) :{
        if (dir.testParamDirect.count() != 1 || dir.testParamDirect[0].count() != 2 || !dir.testParamDirect[0][1].join("").isEmpty()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("НЕ УДАЛОСЬ ПРОЧИТАТЬ ПАРАМЕТРЫ ДИРЕКТИВЫ ПУСК"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        ost_flag.store(0);
        m_ost_flag.store(0);
        if (programs.count() == 0 || programs.last().blockRun){
            printMessage.append(dir.directive + " ДИРЕКТИВА НЕДОПУСТИМА");
            //protocol->append(printMessage);
            {
                printInProt(printMessage, "0", textStyle());
            }
            break;
        } else{
            stopProg = false;
            printMessage.append(dir.directive + " " + programs.last().programName);
        }
        //protocol->append(printMessage);
        {
            printInProt(printMessage, "0", textStyle());
        }
        if (programs.length() >= 1) programs.last().infoStopMsg.clear();
        emit this->unsetStopState();
        runProgram(/*protocol, protocloWgt, programInfomodel*/);
        break;
    }
    case (DirectParser::TypeDirect::RVYHOD) : {
        if (dir.testParamDirect.count() != 1 || dir.testParamDirect[0].count() != 2 || dir.testParamDirect[0][1].count() != 0){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("НЕДОПУСТИМО ИСПОЛЬЗОВАНИЕ ПАРАМЕТРОВ В ДИРЕКТИКЕ РВЫХОД"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (/*!hasRunProg &&*/ programs.isEmpty()){
            printMessage.append(dir.directive + QString(" ИСПОЛЬЗОВАНИЕ ДИРЕКТИВЫ РВЫХОД НЕДОПУСТИМО - НЕТ ЗАПУЩЕННЫХ ЦГ"));
            //protocol->append(printMessage);
            {
                printInProt(printMessage, "0", textStyle());
            }
            printMessage = "";
            return false;
        }

        stopProg = true;
        if (stopProg || !hasRunProg){
            while(!programs.isEmpty()){
                printMessage = "\t\t";
                printMessage.append(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"));
                printMessage.append("\t");

                printMessage.append(dir.directive + " " + programs.last().programName);
                if (programs.count() > 1){
                    printMessage.append(" -> " + programs.at(programs.count() - 2).programName);
                }
                //protocol->append(printMessage);
                {
                    printInProt(printMessage, "1", textStyle());
                }
                printMessage = "";
                /*for (int row = programInfomodel->rowCount() - 1; row >= 0; --row){
                    if (programInfomodel->data(programInfomodel->index(row, 1)).toString() == programs.last().programName){
                        programInfomodel->removeRow(row);
                        break;
                    }
                }*/
                {
                    QEventLoop loop;
                    QObject::connect(this, &directRunner::programModelActionAccept, &loop, &QEventLoop::quit);
                    emit this->removeProgramInModel();
                    loop.exec();
                }
                programs.pop();
            }
        }
        emit this->unsetStopState();
        if (programs.isEmpty()){
            while (!command.isEmpty()){
                runDirectFunc(command.dequeue()/*, protocol, protocloWgt, programInfomodel*/);
            }
        }
        break;
    }
    case (DirectParser::TypeDirect::RR_PAR) :{
        if (dir.testParamDirect.count() > 101){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            errorMessage.append("\t\t\tНЕДОПУСТИМОЕ КОЛИЧЕСТВО СТРОК В ДИРЕКТИВЕ");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
        if (dir.testParamDirect.isEmpty() || dir.testParamDirect[0].count() < 2 || dir.testParamDirect[0][1].isEmpty()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append("ОШИБКА: НЕТ ПАРАМЕТРОВ ДИРЕКТИВЫ РР_ПАР");
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        //QRegularExpression regex(R"(FL\.([A-Za-z0-9]{1,3})_([A-Za-z0-9]{1,8})(?:\[(\d+)\])?)");
        QList<QString> strList = dir.testParamDirect[0][1];

        if (strList[0] == "-") strList.removeAt(0);
        /*if ((strList.count() < 1) || (strList[0] == "+" && strList.count() < 2)){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("ОШИБКА В ИДЕНТИФИКАТОРЕ ") + ID_TEMPLATE);
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }*/
        /*int paramLen{0};
        if (param == "+"){
            paramLen = strList[1].count();
        } else paramLen = param.count();
        if (paramLen > 20){
            if (param == "+") param = strList[1];
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("Ошибка: Длина ИД_РА РР параметра > 20 (%1)").arg(param));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }*/
        if (strList.count() >= 2 && strList[1] == "-БЛ"){
            QString param = strList[0];
            if (strList.count() > 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("ПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ"));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            QString errorText;
            if (!RRParam::removeBlock(param, &errorText)){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(errorText);
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            printMessage.append(dir.directive + " " + param + " -БЛ");
            //protocol->append(printMessage);
            {
                printInProt(printMessage, "0", textStyle());
            }
            printMessage = "";
        } else{
            enum MODES{
                DELETE_MODE,
                INSERT_MODE
            };
            MODES mode;
            if (dir.testParamDirect[0][1][0] != "-" && dir.testParamDirect[0][1][0] != "+"){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append("ОШИБКА: НЕ ЗАДАН РЕЖИМ В ДИРЕКТИВЕ (+, -, -БЛ)");
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            if (dir.testParamDirect[0][1][0] == "-"){
                mode = MODES::DELETE_MODE;
            } else mode = MODES::INSERT_MODE;

            printMessage.append(dir.directive);
            if (mode == MODES::INSERT_MODE) printMessage.append(" + ");
            else printMessage.append(" - ");

            strList.clear();
            for (int row = 0; row < dir.testParamDirect.count(); ++row){
                if (row == 0 && dir.testParamDirect[row].count() == 2 && dir.testParamDirect[row][1].count() == 1 && (dir.testParamDirect[row][1][0] == "+" || dir.testParamDirect[row][1][0] == "-")){
                    continue;
                }
                if (dir.testParamDirect[row].count() < 2 || dir.testParamDirect[row][1].isEmpty()){
                    errorMessage.append(QString("\t\t\t") + "ОШИБКА: НЕТ ПАРАМЕТРОВ ДИРЕКТИВЫ РР_ПАР");
                    {
                        printInProt(errorMessage, "13", textStyle());
                    }
                }
                int maxCountInsert{2};
                int maxCountDelete{1};
                if (row == 0){
                    maxCountDelete+=1;
                    maxCountInsert+=1;
                }
                if ((mode == MODES::INSERT_MODE && dir.testParamDirect[row][1].count() > maxCountInsert) || (mode == MODES::DELETE_MODE && dir.testParamDirect[row][1].count() > maxCountDelete)){
                    errorMessage.append(dir.directive + "\n\t\t\t" + dir.testParamDirect[row][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(QString("ПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ"));
                    {
                        printInProt(errorMessage, "13", textStyle());
                    }
                    return false;
                }
                strList = dir.testParamDirect[row][1];
                if (row == 0) strList.removeAt(0);
                QString param = strList[0];
                bool hasVal{false};
                float val = 0;
                if (strList.count() == 2){
                    val = strList[1].toFloat(&hasVal);
                    if (!hasVal){
                        errorMessage.append(dir.directive + "\n\t\t\t" + dir.testParamDirect[row][1].join(" ") + "\n");
                        if (dir.numDirect > -1){
                            errorMessage.append("\t#" + numDirect + "\t\t");
                        } else errorMessage.append("\t\t\t");
                        errorMessage.append("ПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ");
                        {
                            printInProt(errorMessage, "13", textStyle());
                        }
                        qDebug() << "Error value";
                        return false;
                    }
                }
                RRParam rrPar(param);
                if (rrPar.isHasError()){
                    errorMessage.append(dir.directive + "\n\t\t\t" + dir.testParamDirect[row][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    errorMessage.append(rrPar.getErrorText());
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }

                if (mode == MODES::DELETE_MODE){
                    if (!rrPar.remove() && rrPar.getErrorCode() != RRParam::ERROR_CODE::CONTAINS_ERROR){
                        errorMessage.append(dir.directive + "\n\t\t\t" + dir.testParamDirect[row][1].join(" ") + "\n");
                        if (dir.numDirect > -1){
                            errorMessage.append("\t#" + numDirect + "\t\t");
                        } else errorMessage.append("\t\t\t");
                        errorMessage.append(rrPar.getErrorText());
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                } else if (mode == MODES::INSERT_MODE){
                    errorMessage.append(dir.directive + "\n\t\t\t   " + dir.testParamDirect[row][1].join(" ") + "\n");
                    if (dir.numDirect > -1){
                        errorMessage.append("\t#" + numDirect + "\t\t");
                    } else errorMessage.append("\t\t\t");
                    RRParam rrPar(param);
                    if (rrPar.isHasError()){
                        errorMessage.append(rrPar.getErrorText());
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }

                    if (rrPar.isExist()){
                        if (rrPar.isArray()){
                            int len;
                            len = rrPar.getLength();
                            if (rrPar.isHasError()){
                                errorMessage.append(rrPar.getErrorText());
                                printInProt(errorMessage, "13", textStyle());
                                return false;
                            }
                            //printMessage.append(QString("\t\t\tРР ПАРАМЕТР %1 УЖЕ СУЩЕСТВУЕТ (В БД ДЛИНА МАССИВА - %2)").arg(rrPar.getFullParamName()).arg(len));
                            printInProt(printMessage + QString("\t\t\tРР ПАРАМЕТР %1 УЖЕ СУЩЕСТВУЕТ (В БД ДЛИНА МАССИВА - %2)").arg(rrPar.getFullParamName()).arg(len), "0", textStyle());
                            continue;
                        }
                        rrPar.setValue(val);
                        if (rrPar.isHasError()){
                            errorMessage.append(rrPar.getErrorText());
                            printInProt(errorMessage, "13", textStyle());
                            return false;
                        }
                        if (!printMessage.isEmpty()) printMessage.append("\n");
                        printMessage.append("\t\t\t" + param);
                        printMessage.append("\t\t" + QString::number(val));
                        printInProt(printMessage, "0", textStyle());
                        printMessage = "";
                        continue;
                    } else if (rrPar.isHasError()){
                        errorMessage.append(rrPar.getErrorText());
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }

                    rrPar.create(val);
                    if (rrPar.isHasError()){
                        errorMessage.append(rrPar.getErrorText());
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                }
                if (!printMessage.isEmpty()) printMessage.append("\n");
                printMessage.append("\t\t\t" + param);
                if (mode == MODES::INSERT_MODE){
                    printMessage.append("\t\t" + QString::number(val));
                }
                {
                    printInProt(printMessage, "0", textStyle());
                }
                printMessage = "";
            }
        }

        break;
    }
    case (DirectParser::TypeDirect::SINONIM) : qDebug() << "пока не реализовано"; break;
    case (DirectParser::TypeDirect::SOOBCH): {
        printMessage.append(dir.directive + " ");
        for (int row = 0; row < dir.testParamDirect.count(); ++row){
            if (dir.testParamDirect[row].count() < 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[row][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("НЕДОСТАТОЧНО ПАРАМЕТРОВ ДИРЕКТИВЫ СООБЩ (СТРОКА %1)").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            if (dir.testParamDirect[row].count() > 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[row][1].join(" ") + "\n");
                if (dir.numDirect > -1){
                    errorMessage.append("\t#" + numDirect + "\t\t");
                } else errorMessage.append("\t\t\t");
                errorMessage.append(QString("ПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ (СТРОКА %1)").arg(QString::number(row + 1)));
                {
                    printInProt(errorMessage, "13", textStyle());
                }
                return false;
            }
            QString str = dir.testParamDirect[row][1].join(" ");
            if (!printMessage.isEmpty()) printMessage.append(str);
            else printMessage.append("\t\t\t" + str);
            //printMessage.prepend("<span style='color: blue; white-space: pre;'>");
            //printMessage.append("</span>");
            //protocol->append(printMessage);
            {
                printInProt(printMessage, "23", textStyle());
            }
            printMessage = "";
        }
        break;
    }
    case (DirectParser::TypeDirect::SP) :{
        //if (dir.testParamDirect.count() != 1)
        if (dir.testParamDirect[0][1].join(" ").isEmpty()){
            //QMessageBox::critical(nullptr, "Ошибка", "Неверное имя файла для сохранения протокола");
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("Недопустимое имя файла для сохранения протокола"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        QString fileUserName = dir.testParamDirect[0][1].join(" ");
        //QString catalog = MainWindow::getCurCatalog();
        QString catalog = MainWindow::getOnParam("ПРОТОКОЛ");
        QDir directory(catalog);
        if (!directory.exists()){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("Не удалось открыть текущий активный каталог"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        QString traf = MainWindow::getOnParam("ТРАФАРЕТ").toUpper();
        int numTraf = traf.right(4).toInt();

        QRegularExpression regex(QString(".*%1-(\\d{4})\\.").arg(traf.left(4)));
        QStringList files = directory.entryList(QDir::Files);

        for (const QString& fileName : files){
            QRegularExpressionMatch match = regex.match(fileName);
            QString numberStr = match.captured(1);
            int number = numberStr.toInt();
            if (number > numTraf) numTraf = number;
        }

        QString baseFileName =
                QString("%1_%2-%3.PCP")
                    .arg(fileUserName)
                    .arg(traf.left(4))
                    .arg(numTraf + 1, 4, 10, QChar('0'));

        QString fullSaveFilePath = QDir(catalog).filePath(baseFileName);

        QFileInfo saveFileInfo(fullSaveFilePath);
        QString fullNUSaveFilePath =
                QDir(saveFileInfo.absoluteDir().filePath("НУ"))
                    .filePath(saveFileInfo.completeBaseName() + ".txt");

        QStringList timeWork;
        {
            m_timeWorkAppcp.clear();
            QEventLoop loop;
            QTimer timer;
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(this, &directRunner::haveTimeWorkAppcp, &loop, &QEventLoop::quit);
            emit requestTimeWorkAppcp();
            timer.start(3500);
            loop.exec();

            timeWork = m_timeWorkAppcp;

        }
        {
            QString tempPrintMessage = printMessage;
            printMessage.append(dir.directive + " " + fullSaveFilePath);
            printInProt(printMessage, "6", textStyle());
            printMessage = tempPrintMessage;
            printMessage.append(dir.directive + " " + fullNUSaveFilePath);
            printInProt(printMessage, "6", textStyle());
            printMessage = tempPrintMessage;
        }
        if (timeWork.isEmpty()){
            printInProt(QString("\t\t\tНе удалось получить время работы АППЦП"), "13", directRunner::textStyle());
        } else{
            printInProt(QString("\t\t\tВремя работы АППЦП:\n"
                                           "\t\t\tЗа текущий день: %1\n"
                                           "\t\t\tЗа текущий месяц: %2\n").arg(timeWork[0]).arg(timeWork[1]), "0", directRunner::textStyle());
        }
        QString errorProt;
        if (!ProtManager::instance().saveFile(fullSaveFilePath, fullNUSaveFilePath, errorProt)){
            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
            if (dir.numDirect > -1){
                errorMessage.append("\t#" + numDirect + "\t\t");
            } else errorMessage.append("\t\t\t");
            errorMessage.append(QString("Неизвестная ошибка при сохранении файла протокола"));
            {
                printInProt(errorMessage, "13", textStyle());
            }
            return false;
        }
        if (!errorProt.isEmpty()) {
            emit this->errorProtValid(errorProt);
        }
        printStartMessage();
        QString tempPrintMessage = printMessage;
        printMessage.append(dir.directive + " " + fullSaveFilePath);
        printInProt(printMessage, "6", textStyle());
        printMessage = tempPrintMessage;
        printMessage.append(dir.directive + " " + fullNUSaveFilePath);
        printInProt(printMessage, "6", textStyle());
        break;
    }
    case (DirectParser::TypeDirect::STOP) :{
        stopProg = true;
        stopMessageStr = "ОСТАНОВ ПО ДИРЕКТИВЕ СТОП";
        printMessage.append(dir.directive + " ");
        for (int row = 0; row < dir.testParamDirect.count(); ++row){
            QString str = "";
            for (int col = 0; col < dir.testParamDirect[row].count(); ++col){
                str.append(dir.testParamDirect[row][col].join(" "));
                if (!str.isEmpty()) str.append(" ");
            }
            if (!str.isEmpty()) printMessage.append(str);
            if (row != dir.testParamDirect.count() - 1) printMessage.append("\n\t\t\t");
        }
        //printMessage.prepend("<span style='color: blue; white-space: pre;'>");
        //printMessage.append("</span>");
        //protocol->append(printMessage);
        {
            printInProt(printMessage, "23", textStyle());
        }
        printMessage = "";
        break;
    }
    case (DirectParser::TypeDirect::UV) : qDebug() << "пока не реализовано"; break;
    case (DirectParser::TypeDirect::PNC) : {
            if (dir.testParamDirect.count() > 101){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\tНЕДОПУСТИМОЕ КОЛИЧЕСТВО СТРОК В ДИРЕКТИВЕ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            if (dir.testParamDirect.length() < 3 || dir.testParamDirect[1][1].length() != 1 || dir.testParamDirect[2][1].length() < 1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append(QString("\t\t\tНедопустимое количество параметров директивы %1").arg(dir.directive));
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            if (socketCanal1->state() != QAbstractSocket::ConnectedState || socketCanal2->state() != QAbstractSocket::ConnectedState){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\tНЕТ СВЯЗИ С НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }

            int zdr = 1;
            QStringList param = dir.testParamDirect[0][1];
            if (param.length() > 0){
                bool ok{false};
                param[0].toInt(&ok);
                if (ok){
                    zdr = param[0].toInt();
                    param.removeFirst();
                }
            }
            /*bool stopFlag = false;*/
            reactType reactMode = reactType::NO_REACT;
            if (param.length() > 0){
                if (param[0] == "СТОП"){
                    //stopFlag = true;
                    reactMode = reactType::STOP;
                } else if (param[0] == "СЛЕД"){
                    reactMode = reactType::SLED;
                }
                else{
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\tНЕДОПУСТИМОЕ ЗНАЧЕНИЕ ДЛЯ РЕАК");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
            }

            printMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" "));
            printInProt(printMessage, "0", textStyle());

            QString point1 = dir.testParamDirect[1][1][0];
            QStringList points;
            QList<double> ndops;
            QList<double> vdops;
            QStringList rrPars;
            points << point1;
            ndops << -1;
            vdops << -1;
            rrPars << "";
            for (int i = 2; i < dir.testParamDirect.length(); ++i){
                param = dir.testParamDirect[i][1];
                if (param.length() > 4){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\t\tНЕДОПУСТИМОЕ КОЛИЧЕСТВО АРГУМЕНТОВ В СТРОКЕ %1").arg(i + 1));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                if (points.contains(param[0])){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\tНедопустимо повторное подключение точки (%1)").arg(param[0]));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                points << param[0];
                ndops << -1;
                vdops << -1;
                rrPars << "";
                param.removeFirst();
                if (param.length() > 0){
                    bool ok{false};
                    param[0].toDouble(&ok);
                    if (ok){
                        ndops[i - 1] = param[0].toDouble();
                        param.removeFirst();
                    }
                    if (param.length() > 0){
                        param[0].toDouble(&ok);
                        if (ok){
                            vdops[i - 1] = param[0].toDouble();
                            if (ndops[i - 1] > vdops[i - 1]){
                                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                                errorMessage.append(QString("Нижний допуск не должен превышвать верхний (точка: %1)").arg(points.last()));
                                printInProt(errorMessage, "13", textStyle());
                                return false;
                            }
                            param.removeFirst();
                        }
                    }
                    if (param.length() > 0 && param[0][0] == "="){
                        rrPars[i - 1] = param[0].mid(1);
                        param.removeFirst();
                    }
                    if (param.length() > 0){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        errorMessage.append(QString("\t\t\tПОСТОРОННЯЯ ИНФОРМАЦИЯ В КОНЦЕ СТРОКИ (СТРОКА %1)").arg(i + 1));
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                }
            }

            QList<int> numContacts;
            for (int i = 0; i < points.length(); ++i){
                if (!appcpParam.contains(points[i])){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\t\tНет идентификатора в БД (%1)").arg(points[i]));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                int numCont;
                if (appcpParam.value(points[i]).kont != 101) numCont = (appcpParam.value(points[i]).raz - 1) * 50 + appcpParam.value(points[i]).kont - 1;
                else numCont = 100;
                numContacts << numCont;

                if (!rrPars[i].isEmpty()){
                    RRParam rrPar(rrPars[i]);
                    if (!rrPar.isValid()){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        errorMessage.append(QString("\t\tРР параметр %1 не является валидным!").arg(rrPars[i]));
                        errorMessage.append(rrPar.getErrorText());
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                }
            }

            QByteArray cBA;
            cBA.clear();
            cBA.append(char(0x0a));
            bool status{false};
            sendMessageToNU(cBA.constData(), cBA.length(), &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if (respondNU.length() == 2 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            cBA.clear();

            cBA.append(char(0x01));
            cBA.append(char(1));
            cBA.append(char(numContacts[0]));
            sendMessageToNU(cBA.constData(), cBA.length(), &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if (respondNU.length() == 2 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            cBA.clear();
            QList<float> results;
            //QList<int> diap;
            QString tableSplit("|----------------|----------------|-----------|-----------|-----------|-------------------------|\n");
            QString table;
            table.append("|----------------|----------------|-----------|-----------|-----------|-------------------------|\n");
            table.append("|      ИД1(-)    |      ИД2(+)    |  знач.(В) |  н.доп.   |   в.доп.  |           ПАР           |\n");
            table.append("|----------------|----------------|-----------|-----------|-----------|-------------------------|\n");
            printInProt(table, "0", textStyle());
            table.clear();

            for (int i = 1; i < numContacts.length(); ++i){
                cBA.append(char(0x02));
                cBA.append(char(1));
                cBA.append(char(numContacts[i]));
                sendMessageToNU(cBA.constData(), cBA.length(), &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
                if (respondNU.length() == 2 && respondNU.at(1) == 0){
                    //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
                    printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                    printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
                }
                else if (respondNU.length() != 2){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                else if (respondNU.at(1) == -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                cBA.clear();

                /*int diapVal;

                if (ndops[i] == -1 || (ndops[i] <= 0.05)) diapVal = 0;
                else if (ndops[i] <= 1000.0) diapVal = 1;
                else if (ndops[i] <= 5000.0) diapVal = 2;
                else diapVal = 3;

                diap.append(diapVal);*/


                    cBA.append(char(0x06));
                    QThread::sleep(zdr);
                    sendMessageToNU(cBA.constData(), cBA.length(), &status);
                    if (!status) return false;
                    if (ost_flag.load() == 1){
                        //if (dir.numDirect != -1) programs.last().numDir -= 1;
                        QByteArray cBAReset;
                        cBAReset.clear();
                        cBAReset.append(char(0x0a));
                        bool statusReset{false};
                        sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                        //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                        goto end_metka;
                    }
                if (respondNU.length() == 3 + 4 && respondNU.at(1) == 0){
                    //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
                    printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                    QByteArray floatData = respondNU.mid(3);
                    float result;
                    std::memcpy(&result, floatData.constData(), sizeof(result));
                    if (std::isnan(result) || std::isinf(result)){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (неудалось получить значение)");
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                    results << result;
                    printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2\n  Un = %4   res = %3").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])).arg(result).arg(int(respondNU[2])), "35", textStyle(), true, true);
                    if (int(respondNU[2]) == 0){
                        printInProt(QString("НЕТ НАПРЯЖЕНИЯ НА ВХОДАХ АППЦП"), "13", textStyle());
                        if (constValues::isImitMode.load() != 1 && reactMode == reactType::STOP){
                            stopProg = true;
                        }
                            stopMessageStr = "НЕТ НАПРЯЖЕНИЯ НА ВХОДАХ АППЦП";
                            GL_NORM_STATUS = false;
                    }

                    if (!rrPars[i].isEmpty()){
                        qDebug() << rrPars[i];
                        RRParam rrPar(rrPars[i]);
                        if (!rrPar.setValue(result)){
                            errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                            errorMessage.append(QString("\t\tНе удалось записать значение РР параметра!").arg(rrPar.getFullParamName()));
                            if (rrPar.isHasError()) errorMessage.append("\t\t\t" + rrPar.getErrorText());
                            printInProt(errorMessage, "13", textStyle());
                            return false;
                        }
                    }
                }
                else if (respondNU.length() != 3 + 4){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 6 байт)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                else if (respondNU.at(1) == -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }

                cBA.clear();

                cBA.append(char(0x02));
                cBA.append(char(0));
                cBA.append(char(numContacts[i]));

                sendMessageToNU(cBA.constData(), cBA.length(), &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
                if (respondNU.length() == 2 && respondNU.at(1) == 0){
                    //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
                    printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                    printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
                }
                else if (respondNU.length() != 2){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                else if (respondNU.at(1) == -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                cBA.clear();

                int row = i - 1;
                QStringList infoSections;
                infoSections << points[0] << points[row + 1] /*<< QString::number(results[row], 'f', 6)*/;
                infoSections << QString::number(results[row], 'f', 4);
                if (ndops[row + 1] >= 0) infoSections << QString::number(ndops[row + 1]);
                else infoSections << "";
                if (vdops[row + 1] >= 0) infoSections << QString::number(vdops[row + 1]);
                else infoSections << "";
                infoSections << rrPars[row + 1];
                QString tableRow = "|";
                for (int i = 0; i < infoSections.length(); i++){
                    QString infoSection = infoSections[i];
                    if (infoSection.length() > tableSplit.split("\n")[0].split("|")[i + 1].length()){
                        infoSection = infoSection.left(tableSplit.split("\n")[0].split("|")[i + 1].length());
                    }
                    int needSpace = tableSplit.split("\n")[0].split("|")[i + 1].length() - infoSection.length();
                    int needSpaceLeft = needSpace / 2 + needSpace % 2;
                    int needSpaceRight = needSpace / 2;
                    infoSection.prepend(QString(needSpaceLeft, QChar(' ')));
                    infoSection.append(QString(needSpaceRight, QChar(' ')));
                    tableRow.append(infoSection + "|");
                }
                table.append(tableRow);
                /*if (diap[row] == 0){
                    QString tempTableSplit = tableSplit;
                    tempTableSplit = tempTableSplit.replace("-", " ");
                    QStringList tableSplitList = tempTableSplit.split("|");
                    int lenCol = tableSplitList[2 + 1].length();
                    tableSplitList[2 + 1] = QString("(") + QString::number(infoSections[2].toDouble() * 1000, 'g', 2) + " Ом)";
                    int needSpace = lenCol - tableSplitList[2 + 1].length();
                    int needSpaceLeft = needSpace / 2 + needSpace % 2;
                    int needSpaceRight = needSpace / 2;
                    tableSplitList[2+1].prepend(QString(needSpaceLeft, QChar(' ')));
                    tableSplitList[2+1].append(QString(needSpaceRight, QChar(' ')));
                    tableRow = tableSplitList.join("|");
                    table.append("\n" + tableRow);
                }*/
                if (constValues::isImitMode.load() != 1 && ((results.value(/*numContacts[row]*/row) < ndops[row + 1] && ndops[row + 1] != -1) || (results.value(/*numContacts[row]*/row) > vdops[row + 1] && vdops[row + 1] != -1))){
                    if (reactMode == reactType::STOP) {
                        stopProg = true;
                        stopMessageStr = QString("ДИРЕКТИВА %1: ЕСТЬ ТОЧКИ НЕ В ДОПУСКЕ").arg(dir.directive);
                    }
                    /*if (reactMode != reactType::SLED)*/ this->GL_NORM_STATUS = false;
                    printInProt(table, "13", textStyle());
                    table.clear();
                } else{
                    printInProt(table, "0", textStyle());
                    table.clear();
                }
                table.append(tableSplit);
                printInProt(table, "0", textStyle());
                table.clear();
            }

            cBA.append(char(0x01));
            cBA.append(char(0));
            cBA.append(char(numContacts[0]));

            sendMessageToNU(cBA.constData(), cBA.length(), &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if (respondNU.length() == 2 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }

            cBA.clear();
            cBA.append(char(0x0a));
            sendMessageToNU(cBA.constData(), cBA.length(), &status);
            if (!status) return false;
            if (ost_flag.load() == 1){
                //if (dir.numDirect != -1) programs.last().numDir -= 1;
                QByteArray cBAReset;
                cBAReset.clear();
                cBAReset.append(char(0x0a));
                bool statusReset{false};
                sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                goto end_metka;
            }
            if (respondNU.length() == 2 && respondNU.at(1) == 0){
                //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
                printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
                printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
            }
            else if (respondNU.length() != 2){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            else if (respondNU.at(1) == -1){
                errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                printInProt(errorMessage, "13", textStyle());
                return false;
            }
            cBA.clear();


            /*for (int row = 0; row < results.count(); ++row){

            }*/

            if (GL_NORM_STATUS){
                printInProt(QString("\t\t\t%1 норма операции").arg(dir.directive), "0", textStyle());
            } else{
                printInProt(QString("\t\t\t%1 ненорма операции").arg(dir.directive), "13", textStyle());
                if (reactMode == reactType::SLED) GL_NORM_STATUS = true;
            }
            break;
        }
        case (DirectParser::TypeDirect::PNC_R) :{
                if (dir.testParamDirect.length() != 1 || dir.testParamDirect[0].length() != 2 || dir.testParamDirect[0][1].length() < 2 || dir.testParamDirect[0][1].length() > 6){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("\t\t\tНедопустимое количество параметров директивы %1").arg(dir.directive));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                if (socketCanal1->state() != QAbstractSocket::ConnectedState || socketCanal2->state() != QAbstractSocket::ConnectedState){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\tНЕТ СВЯЗИ С НУ");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                QStringList param = dir.testParamDirect[0][1];
                bool ok{false};
                int zdr = 1;
                param[0].toInt(&ok);
                if (dir.testParamDirect[0][1].length() > 2 && ok){
                    zdr = dir.testParamDirect[0][1][0].toInt();
                    param.removeFirst();
                }
                QString par1;
                QString par2;

                par1 = param[0];
                par2 = param[1];

                if (!appcpParam.contains(par1) || !appcpParam.contains(par2)){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\tНет идентификатора в БД");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                if (par1 == par2){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append(QString("Недопустимо повторное подключение точик (%1)").arg(par1));
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                int numCont1;
                int numCont2;
                if (appcpParam.value(par1).kont != 101) numCont1 = (appcpParam.value(par1).raz - 1) * 50 + appcpParam.value(par1).kont - 1;
                else numCont1 = 100;
                if (appcpParam.value(par2).kont != 101) numCont2 = (appcpParam.value(par2).raz - 1) * 50 + appcpParam.value(par2).kont - 1;
                else numCont2 = 100;

                if (numCont1 == numCont2){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\tДля замера необходимо использовать два РАЗЛИЧНЫХ контакта");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }

                /*qDebug() << "CONT1: " << numCont1;
                qDebug() << "CONT2: " << numCont2;
                qDebug() << appcpParam.value("ZP_KORPUS").raz;
                qDebug() << appcpParam.value("ZP_KORPUS").kont;*/

                double nDop{-1};
                double vDop{-1};
                if (param.length() > 2){
                    param[2].toFloat(&ok);
                    if (ok){
                        nDop = param[2].toFloat();
                        if (param.length() > 3){
                            param[3].toFloat(&ok);
                            if (ok){
                                vDop = param[3].toFloat(&ok);
                                param.removeAt(3);
                            }
                        }
                        param.removeAt(2);
                    }
                }

                if (nDop > vDop && vDop != -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\tНижний допуск не должен превышать верхний!");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                //qDebug() << "NDOP: " << nDop;
                //qDebug() << "VDOP: " << vDop;




                float result{0};


                printMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" "));
                printInProt(printMessage, "0", textStyle());
                char c[4];// = [0x0D, 0x01];
                c[0] = char(0x0A);
                bool status;
                sendMessageToNU(c, 1, &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
                if (respondNU.length() == 2 && respondNU.at(1) == 0){
                    //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                    printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                    printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
                }
                else if (respondNU.length() != 2){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                else if (respondNU.at(1) == -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                c[0] = char(0x01);
                c[1] = char(1);
                c[2] = char(numCont1);
                sendMessageToNU(c, 3, &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
                if (respondNU.length() == 2 && respondNU.at(1) == 0){
                    //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                    printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                    printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
                }
                else if (respondNU.length() != 2){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                else if (respondNU.at(1) == -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }

                c[0] = char(0x02);
                c[1] = char(1);
                c[2] = char(numCont2);
                sendMessageToNU(c, 3, &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
                if (respondNU.length() == 2 && respondNU.at(1) == 0){
                    //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                    printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                    printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
                }
                else if (respondNU.length() != 2){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                else if (respondNU.at(1) == -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }

                    c[0] = char(0x06);
                    QThread::sleep(zdr);
                    sendMessageToNU(c, 1, &status);
                    if (!status) return false;
                    if (ost_flag.load() == 1){
                        //if (dir.numDirect != -1) programs.last().numDir -= 1;
                        QByteArray cBAReset;
                        cBAReset.clear();
                        cBAReset.append(char(0x0a));
                        bool statusReset{false};
                        sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                        //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                        goto end_metka;
                    }
                if (respondNU.length() == 3 + 4 && respondNU.at(1) == 0){
                    //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                    printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                    QByteArray floatData = respondNU.mid(3);
                    std::memcpy(&result, floatData.constData(), sizeof(result));
                    if (std::isnan(result) || std::isinf(result)){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (неудалось получить значение)");
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }

                    printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2\n   res = %3").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])).arg(result), "35", textStyle(), true, true);
                    if (int(respondNU[2]) == 0){
                        printInProt(QString("НЕТ НАПРЯЖЕНИЯ НА ВХОДАХ АППЦП"), "13", textStyle());
                        if (constValues::isImitMode.load() != 1){
                            stopProg = true;
                        }
                            stopMessageStr = "НЕТ НАПРЯЖЕНИЯ НА ВХОДАХ АППЦП";
                            //GL_NORM_STATUS = false;
                    }
                }
                else if (respondNU.length() != 3 + 4){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 6 байт)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                else if (respondNU.at(1) == -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }

                QString rrParName;
                if (param.length() > 2){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n\t\t\t");
                    if (param[2][0] != "="){
                        errorMessage.append("РР параметр должен записываться со знаком =");
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                    rrParName = param[2].mid(1);

                    RRParam rrPar(rrParName);
                    if (rrPar.isHasError()){
                        errorMessage.append(rrPar.getErrorText());
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                    if (!rrPar.isExist()){
                        if (rrPar.isHasError()) errorMessage.append(rrPar.getErrorText());
                        else errorMessage.append("РР ПАРАМЕТР ДЛЯ ЗАПИСИ РЕЗУЛЬТАТА ЕЩЕ НЕ СОЗДАН");
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                    rrPar.setValue(result);
                    if (rrPar.isHasError()){
                        errorMessage.append(rrPar.getErrorText());
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                    /*QRegularExpression regex(R"(^FL.([A-Za-z0-9]{1,3})_?([A-Za-z0-9_\-\/\.]{1,15})?(?:\[(\d+)\])?$)");
                    QRegularExpressionMatch match = regex.match(rrPar);
                    if (!match.hasMatch()){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        errorMessage.append("\t\t\tНедопустимое имя РР параметра для записи результата");
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                    QString errorString;
                    if (!isParamExists(match.captured(1), match.captured(2), errorString)){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        if (errorString.isEmpty()) errorMessage.append("\t\t\tРР параметра для записи результата не создан");
                        else errorMessage.append(errorString);
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }
                    bool isArray = isParamArray(match.captured(1), match.captured(2), errorString);
                    if ((!match.captured(3).isEmpty() && !isArray) || (match.captured(3).isEmpty() && isArray)){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        if (errorString.isEmpty()) {
                            if (!isArray) errorMessage.append("\t\t\tРР параметр для записи результата не является массивом");
                            else errorMessage.append("\t\t\tРР параметр для записи результата является массивом. Требуется указать индекс!");
                        }
                        else errorMessage.append(errorString);
                        printInProt(errorMessage, "13", textStyle());
                        return false;
                    }

                    QString queryString;
                    if (match.captured(3).isEmpty()) queryString = QString("UPDATE RR_PAR SET Val = %1 WHERE Bl_Name = '%2' AND Par_Name = '%3'").arg(result).arg(match.captured(1)).arg(match.captured(2));
                    else queryString = QString("UPDATE RR_PAR SET Val = %1 WHERE Bl_Name = '%2' AND Par_Name = '%3' AND Index = %4").arg(result).arg(match.captured(1)).arg(match.captured(2)).arg(match.captured(3));
                    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
                    if (!query.isActive()){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        if (dir.numDirect > -1){
                            errorMessage.append("\t#" + numDirect + "\t\t");
                        } else errorMessage.append("\t\t\t");
                        errorMessage.append("ОШИБКА ОБРАЩЕНИЯ К БД: " + query.lastError().text());
                        {
                            printInProt(errorMessage, "13", textStyle());
                        }
                        qDebug() << "QUERY ERROR";
                        return false;
                    }
                    if (query.numRowsAffected() <= 0){
                        errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                        if (dir.numDirect > -1){
                            errorMessage.append("\t#" + numDirect + "\t\t");
                        } else errorMessage.append("\t\t\t");
                        errorMessage.append("ОШИБКА ЗАПИСИ РЕЗУЛЬТАТА В БД");
                        {
                            printInProt(errorMessage, "13", textStyle());
                        }
                        qDebug() << "ERROR WRITE RESULT";
                        return false;
                    }
                    query.clear();*/

                }

                QString table;
                QString tableSplit("|----------------|----------------|-----------|-----------|-----------|-------------------------|");
                table.append(tableSplit + "\n");
                table.append      ("|      ИД1(-)    |      ИД2(+)    |  знач.(В) |  н.доп.   |   в.доп.  |           ПАР           |\n");
                table.append(tableSplit);
                printInProt(table, "0", textStyle());
                table = QString();

                QStringList infoSections;
                infoSections << par1 << par2 /*<< QString::number(result, 'f', 6)*/;
                infoSections << QString::number(result, 'f', 4);
                if (nDop != -1) infoSections << QString::number(nDop);
                else infoSections << QString();
                if (vDop != -1) infoSections << QString::number(vDop);
                else infoSections << QString();
                infoSections << rrParName;
                QString tableRow = "|";
                for (int i = 0; i < 6; i++){
                    QString infoSection = infoSections[i];
                    if (infoSection.length() > tableSplit.split("|")[i + 1].length()){
                        infoSection = infoSection.left(tableSplit.split("\n")[0].split("|")[i + 1].length());
                    }
                    int needSpace = tableSplit.split("|")[i + 1].length() - infoSection.length();
                    int needSpaceLeft = needSpace / 2 + needSpace % 2;
                    int needSpaceRight = needSpace / 2;
                    infoSection.prepend(QString(needSpaceLeft, QChar(' ')));
                    infoSection.append(QString(needSpaceRight, QChar(' ')));
                    tableRow.append(infoSection + "|");
                }
                table.append(tableRow);
                if (constValues::isImitMode.load() != 1 && ((nDop != 1 && result < nDop) || (vDop != -1 && result > vDop))){
                    printInProt(table, "13", textStyle());
                    this->GL_NORM_STATUS = false;
                } else{
                    printInProt(table, "0", textStyle());
                }
                table = QString();
                table.append(tableSplit);
                table.chop(1);

                printInProt(table, "0", textStyle());
                if (!this->GL_NORM_STATUS){
                    printInProt(QString("\t\t%1: НЕНОРМА ОПЕРАЦИИ").arg(dir.directive), "13", textStyle());
                } else{
                    printInProt(QString("\t\t%1: НОРМА ОПЕРАЦИИ").arg(dir.directive), "0", textStyle());
                }

                c[0] = char(0x02);
                c[1] = char(0);
                c[2] = char(numCont2);
                sendMessageToNU(c, 3, &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
                if (respondNU.length() == 2 && respondNU.at(1) == 0){
                    //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                    printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                    printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
                }
                else if (respondNU.length() != 2){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                else if (respondNU.at(1) == -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }

                c[0] = char(0x01);
                c[1] = char(0);
                c[2] = char(numCont1);
                sendMessageToNU(c, 3, &status);
                if (!status) return false;
                if (ost_flag.load() == 1){
                    //if (dir.numDirect != -1) programs.last().numDir -= 1;
                    QByteArray cBAReset;
                    cBAReset.clear();
                    cBAReset.append(char(0x0a));
                    bool statusReset{false};
                    sendMessageToNU(cBAReset.constData(), cBAReset.length(), &statusReset);
                    //printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
                    goto end_metka;
                }
                if (respondNU.length() == 2 && respondNU.at(1) == 0){
                    //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
                    printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(c[0])), "30", textStyle(), true, false);
                    printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
                }
                else if (respondNU.length() != 2){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }
                else if (respondNU.at(1) == -1){
                    errorMessage.append(dir.directive + " " + dir.testParamDirect[0][1].join(" ") + "\n");
                    errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
                    printInProt(errorMessage, "13", textStyle());
                    return false;
                }




                break;
            }
    }
    end_metka:
    if (stepMode.load() == 1) {
        stopProg = true;
        stopMessageStr = "ОСТАНОВ ПО РЕЖИМУ ШАГ";
    }
    //qDebug() << "GL_NORM_STATUS" << GL_NORM_STATUS;
    if (reactStopMode.load() == 1 && !GL_NORM_STATUS) {
        stopProg = true;
        stopMessageStr = "ОСТАНОВ ПО НЕНОРМЕ ОПЕРАЦИИ";
    }
    if ((ost_flag.load() == 1 || m_ost_flag.load() == 1) && hasRunProg){
        stopProg = true;
        stopMessageStr = "ОСТАНОВ ПО ЗАПРОСУ ОПЕРАТОРА ";
        if (ost_flag.load() == 1){
            stopMessageStr.append("(CРОСТ)");
            printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ", "13", textStyle());
        } else{
            stopMessageStr.append("(CРОСТ_М)");
            printInProt("ЦИКЛОГРАММА ОСТАНОВЛЕНА ПО СРОСТ_М", "13", textStyle());
        }
        ost_flag.store(0);
        m_ost_flag.store(0);

    }
    //protocol->append(printMessage);
    return true;
}

/*
            while (query->next()){
                QString blockName = query->value(0).toString();
                QString parName = query->value(1).toString();
                bool ok{false};
                int index{-1};
                if (!query->value(2).isNull()){
                    index = query->value(2).toInt(&ok);
                    if (!ok){
                        qDebug() << "Error convertation index";
                        return;
                    }
                }
                ok = false;
                float value = query->value(3).toFloat(&ok);
                if (!ok){
                    qDebug() << "ERROR VALUE CONVERTATION";
                    return;
                }

                QString printStr = "";
                if (index <= 0){
                    printStr.append(QString("\t\t\tFL.%1_%2").arg(blockName).arg(parName));
                }
                if (index == -1){
                    printStr.append(QString(" =\t%1").arg(value));
                } else{
                    if (index == 0){
                        printStr.append("\n");
                    }
                    printStr.append(QString("\t\t\t\tFL.%1=%2[%3] =\t%4").arg(blockName).arg(parName).arg(index).arg(value));
                }
                protocol->append(printStr);
            }
 */

void directRunner::setVarinatkVar(const QString& var, bool stopProg){
    this->GL_VAR = var;
    if (stopProg){
        this->stopProg = true;
        this->stopMessageStr = "ОСТАНОВ ПО ВАРИАНТУ ПРИНЯТЬ С ОСТАНОВОМ ДИРЕКТИВЫ ВАРИАНТК";
    }
    emit this->varinantkSelectedVar();
}

void directRunner::setDirectVar(directRunner::DIRECT_VARIABLE dirVar){
    this->dirVar = dirVar;
    switch (dirVar) {
    case (directRunner::DIRECT_VARIABLE::OK_GO) : {
        //this->GL_MODE_NEXT = true;
        //this->GL_NORM_STATUS = true;
        break;
    }
    case (directRunner::DIRECT_VARIABLE::OK_STOP) : {
        //this->GL_MODE_NEXT = false;
        this->stopProg = true;
        //this->GL_NORM_STATUS = true;
        break;
    }
    case (directRunner::DIRECT_VARIABLE::NOT_OK_GO) : {
        //this->GL_MODE_NEXT = true;
        //this->GL_NORM_STATUS = false;
        break;
    }
    case (directRunner::DIRECT_VARIABLE::NOT_OK_STOP) : {
        //this->GL_MODE_NEXT = false;
        this->stopProg = true;
        //this->GL_NORM_STATUS = false;
        break;
    }
    default:{
        //this->GL_MODE_NEXT = false;
        this->stopProg = true;
        //this->GL_NORM_STATUS = false;
        break;
    }
    }
    emit this->directSelectedVar();
}


directRunner::~directRunner(){
    qDebug() << "destructor direct Runner";
    socketCanal1->disconnectFromHost();
    socketCanal1->waitForDisconnected(3000);

    socketCanal2->disconnectFromHost();
    socketCanal2->waitForDisconnected(3000);
}

void directRunner::connectNU(){

    if (socketCanal1->state() != QAbstractSocket::ConnectedState || socketCanal2->state() != QAbstractSocket::ConnectedState){
        /*socketCanal1->connectToHost("127.0.0.1", 0x4567);
        if (!socketCanal1->waitForConnected(3000)){
            printInProt(QString("Ошибка установки связи с НУ по каналу 1"), "13", textStyle());
        } else{
            printInProt(QString("Связь установлена с НУ по каналу 1"), "23", textStyle());
        }
        socketCanal2->connectToHost("127.0.0.1", 0x4568);
        if (!socketCanal2->waitForConnected(3000)){
            printInProt(QString("Ошибка установки связи с НУ по каналу 2"), "13", textStyle());
        } else{
            printInProt(QString("Связь установлена с НУ по каналу 2"), "23", textStyle());
        }*/
        printInProt(QString("%1\tУстанавливаем связь с НУ").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "23", textStyle());
        printInProt(QString("**********************************************"), "30", textStyle());
        qDebug() << ipAppcpServ;
        socketCanal1->connectToHost(ipAppcpServ, portAppcpWriteAndRead);
        socketCanal2->connectToHost(ipAppcpServ, portAppcpOnlyRead);
        QTimer::singleShot(3000, this, [this](){
           if (this->socketCanal1->state() != QAbstractSocket::ConnectedState) {
                printInProt(QString("%1\tОшибка установки связи с НУ по каналу 1").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "13", textStyle());
           }
           if (this->socketCanal2->state() != QAbstractSocket::ConnectedState){
               printInProt(QString("%2\tОшибка установки связи с НУ по каналу 2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "13", textStyle());
           }
        });
    } else{
        printInProt(QString("%1\tСвязь с НУ уже установлена").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "23", textStyle());
    }

    /*if (MainWindow::getCfgParam("ВН_ПРИБОР") == "ДА") {
        this->hasVoltMode = true;
        if (voltSocket->state() != QAbstractSocket::ConnectedState) {
            this->voltSocket->connectToHost("127.0.0.1", 0x4005);
            printInProt(QString("Устанавливаем связь с Вольтметром"), "23", textStyle());
            printInProt(QString("**********************************************"), "30", textStyle());
            QTimer::singleShot(3000, this, [this](){
               if (this->voltSocket->state() != QAbstractSocket::ConnectedState) {
                    printInProt(QString("Ошибка установки связи с Вольтметром"), "13", textStyle());
               } else {
                   QByteArray cBA;
                   cBA.append(char(0x01)).append(char(0x01)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00));

                   QEventLoop loop;
                   QTimer timer;
                   timer.setSingleShot(true);

                   QMetaObject::Connection con3 = QObject::connect(voltSocket, &QTcpSocket::readyRead, [this](){
            qDebug() << "VOLT";
                               QByteArray answer = voltSocket->readAll();

                               printInProt(QString("%1 NETCL: получено %2 байтов").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(answer.length()), "13", textStyle(), true);
                               printInProt(QString("\t\t\tПолучено сообщ. volt"), "13", textStyle(), true);
                               QStringList byteList;

                               int count = 0;
                               for (unsigned char byte: answer){
                                   QString curByte;
                                   if (count == 0) curByte.append("\t\t\t");
                                   count += 1;
                                   curByte.append(QString("0x") + QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
                                   if (count >= 16){
                                       curByte += "\n";
                                       count = 0;
                                   }
                                   byteList << curByte;
                               }
                               byteList.first().prepend(" ");
                               if (byteList.last().at(byteList.last().length()-1) == '\n') byteList.last().chop(1);
                               printInProt(byteList.join(" "), "23", textStyle(), true);

                               if (answer.length() != 8)
                                   printInProt("Недопустимый размер сообщения от вольтметра", "13");
                               if (answer[0] != char(0x01))
                                   printInProt("Ответ от вольтметра получен на другую команду", "13");
                               if (answer[1] != char(0x00))
                                   printInProt(QString("Сбой в работе вольтметра: %1").arg(voltErrorCode[answer[1]]), "13");
                               else
                                this->voltReady = true;
                           });

                   QMetaObject::Connection con1 = QObject::connect(this, &directRunner::voltAnswer, [&loop, this](){
                       loop.quit();
                   });
                   QMetaObject::Connection con2 = QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

                   voltSocket->write(cBA);
                   voltSocket->flush();

                   timer.start(6000);
                   loop.exec();

                   timer.stop();
                   QObject::disconnect(con1);
                   QObject::disconnect(con2);
                   QObject::disconnect(con3);

                   if (this->voltReady) {
                       printInProt("Вольтметр готов к работе", "23", textStyle());
                       QObject::connect(voltSocket, &QTcpSocket::readyRead, this, &directRunner::voltRR);
                   } else {
                       printInProt("Вольтметр недоступен", "13", textStyle());
                   }
               }
            });
        } else {
            printInProt("Связь с вольтметром уже установлена!", "23", textStyle());
        }
    }*/
}

void directRunner::disconnectNU(){
    if (socketCanal1->state() != QAbstractSocket::UnconnectedState || socketCanal2->state() != QAbstractSocket::UnconnectedState){
        socketCanal1->disconnectFromHost();
        /*socketCanal1->waitForDisconnected(3000);
         printInProt(QString("Связь с НУ по каналу 1 разорвана"), "13", textStyle());*/

        socketCanal2->disconnectFromHost();
        /*socketCanal2->waitForDisconnected(3000);
        printInProt(QString("Связь с НУ по каналу 2 разорвана"), "13", textStyle());*/
        if (socketCanal1->state() == QAbstractSocket::UnconnectedState && socketCanal2->state() == QAbstractSocket::UnconnectedState){
            printInProt(QString("%1\tСвязь с НУ разорвана").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "23", textStyle());
        } else{
        QTimer::singleShot(3000, this, [this](){
                bool status{true};
               if (this->socketCanal1->state() != QAbstractSocket::UnconnectedState){
                   printInProt(QString("%1\tОшибка разрыва связи с НУ по каналу 1").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "13", textStyle());
                   status = false;
               }
               if (this->socketCanal2->state() != QAbstractSocket::UnconnectedState){
                   printInProt(QString("%1\tОшибка разрыва связи с НУ по каналу 2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "13", textStyle());
                   status = false;
               }
               if (status){
                   printInProt(QString("%1\tСвязь с НУ разорвана").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "23", textStyle());
               }
            });
        }
    } else{
        printInProt(QString("%1\tСвязь с НУ не была установлена").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "23", textStyle());
    }
}

//отправлять errorMessage в ManualModeWindow
bool directRunner::runCommandNU(const unsigned char command, int contact, bool setConnect){
    QString printMessage = QString("%1\tРУ:\t\t--> ").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"));
    QString errorMessage = QString("%1\tРУ:\t\t=-> # ").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"));
    if (socketCanal1->state() != QAbstractSocket::ConnectedState || socketCanal2->state() != QAbstractSocket::ConnectedState){
        errorMessage.append("Нет соединения с НУ");
        printInProt(errorMessage, "13", textStyle());
        emit printMessageToManualWindow(errorMessage, "red");
        emit manualCommandComplete(false);
        return false;
    }
    if (command == static_cast<char>(NUCommand::PODKL_M) || command == static_cast<char>(NUCommand::PODKL_P)){
        QString dirName = (command == static_cast<char>(NUCommand::PODKL_M) ? "ПОДКЛ_М" : "ПОДКЛ_П");
        QByteArray cBA;
        cBA.append(command == static_cast<char>(NUCommand::PODKL_M) ? 0x01 : 0x02);
        /*if (setConnect) cBA.append(0x01);
        else cBA.append(char(0x00));*/
        cBA.append(setConnect ? 0x01 : char(0x00));
        cBA.append(contact);
        bool status{false};
        sendMessageToNU(cBA, cBA.length(), &status);
        if (!status){
            errorMessage.append("Ошибка при выполнении команды в НУ");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append("Ошибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append("Ошибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        printMessage.append(QString("Директива %4: контакт %1 %2. <%3>").arg(contact).arg(setConnect ? "вкл" : "выкл").arg(contact == 100 ? "земля" : QString::number(contact + 1)).arg(dirName));
        printInProt(printMessage, "30", textStyle());
        emit printMessageToManualWindow(printMessage, "blue");
    } else if (command == static_cast<char>(NUCommand::SBR_PODKL)){
        QByteArray cBA;
        cBA.append(char(0x0A));
        bool status {false};
        sendMessageToNU(cBA, cBA.length(), &status);
        if (!status){
            errorMessage.append("Ошибка при выполнении команды в НУ");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        if (respondNU.length() == 2 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПОДКСОЕД", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2){
            errorMessage.append("Ошибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append("Ошибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        printMessage.append(QString("Директива СБР_ПОДКЛ: выполнен сброс всех подключений"));
        printInProt(printMessage, "30", textStyle());
        emit printMessageToManualWindow(printMessage, "blue");
    } else if (command == static_cast<char>(NUCommand::ISM_SN) || command == static_cast<char>(NUCommand::ISM_SV)){
        QByteArray cBA;
        if (command == static_cast<char>(NUCommand::ISM_SN)){
            cBA.append(0x09);
        } else{
            cBA.append(0x08);
            cBA.append(char(contact));
            if (setConnect) cBA.append(0x01);
            else cBA.append(char(0x00));
            cBA.append(0x01);
        }
        bool status{false};
        float result{0};
        sendMessageToNU(cBA, cBA.length(), &status);
        if (!status){
            errorMessage.append("Ошибка при выполнении команды в НУ");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        if (respondNU.length() == 2 + 4 && respondNU.at(1) == 0){
            //printInProt("NET: получили ответ на ПСЦ_Р", "30", textStyle());
            printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
            QByteArray floatData = respondNU.mid(2);
            std::memcpy(&result, floatData.constData(), sizeof(result));
            if (std::isnan(result) || std::isinf(result)){
                errorMessage.append("Ошибка в полученном ответе от НУ (не удалось получить значение)");
                printInProt(errorMessage, "13", textStyle());
                emit printMessageToManualWindow(errorMessage, "red");
                emit manualCommandComplete(false);
                return false;
            }
            printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2\n   res = %3").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])).arg(result), "35", textStyle(), true, true);
        }
        else if (respondNU.length() != 2 + 4){
            errorMessage.append("Ошибка в полученном ответе от НУ (ожидалось 6 байт)");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        else if (respondNU.at(1) == -1){
            errorMessage.append("Ошибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        qDebug() << "message send";
        QString message = printMessage + "Замер произведен";
        printInProt(message, "30", textStyle());
        emit printMessageToManualWindow(message, "blue");
        message = "\t\t Сопротивление = " + QString::number(result, 'f', 6) + "КОм";
        printInProt(message, "29", textStyle());
        emit printMessageToManualWindow(message, "pink");
        emit sendResulToManualWindow(result);
    } else if (command == static_cast<char>(NUCommand::ISM_NAPR)){
        QByteArray cBA;
        cBA.append(0x06);
        bool status{false};
        sendMessageToNU(cBA, cBA.length(), &status);
        if (!status){
            errorMessage.append("Ошибка при выполнении команды в НУ");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit sendResulToManualWindow(false);
            return false;
        }
        if (respondNU.length() != 3 + 4){
            errorMessage.append("Ошибка в полученном ответе от НУ (ожидалось 7 байт)");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        if (respondNU.at(1) == -1){
            errorMessage.append("Ошибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        float result{0};
        printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
        QByteArray floatData = respondNU.mid(3);
        std::memcpy(&result, floatData.constData(), sizeof(result));
        if (std::isnan(result) || std::isinf(result)){
            errorMessage.append("Ошибка в полученном ответе от НУ (неудалось получить значение)");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2\n   res = %3").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])).arg(result), "35", textStyle(), true, true);
        //выводим результаты
        QString message;
        message = printMessage + "Замер произведен";
        emit printMessageToManualWindow(message, "blue");
        message = printMessage + QString("Замер напряжения: %1").arg(QString::number(result, 'f', 6));
        printInProt(message, "29", textStyle());
        emit printMessageToManualWindow(message, "blue");
        emit sendResulToManualWindow(result);
    } else if (command == static_cast<char>(NUCommand::PODKL_1M)){
        QByteArray cBA;
        cBA.append(0x22);
        if (contact == 1) cBA.append(1);
        else cBA.append(char(0));
        bool status{false};
        sendMessageToNU(cBA, cBA.length(), &status);
        if (!status){
            errorMessage.append("Ошибка при выполнении команды в НУ");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit sendResulToManualWindow(false);
            return false;
        }
        if (respondNU.length() != 2){
            errorMessage.append("Ошибка в полученном ответе от НУ (ожидалось 2 байта)");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        if (respondNU.at(1) == -1){
            errorMessage.append("Ошибка в аппаратуре НУ");
            printInProt(errorMessage, "13", textStyle());
            emit printMessageToManualWindow(errorMessage, "red");
            emit manualCommandComplete(false);
            return false;
        }
        if (contact == true){
            printMessage.append("Сопртивление 1 МОм подключено к корпусу");
            emit printMessageToManualWindow(printMessage, "blue");
        } else{
            printMessage.append("Сопротивление 1 МОм снято с корпуса");
            emit printMessageToManualWindow(printMessage, "red");
        }
        printInProt(printMessage, "30", textStyle());
    }
    emit manualCommandComplete(true);
    return true;
}

bool directRunner::runDirect(const DirectParser::Direct &direct){
    if (hasRunManualMode.load() == 1){
        printInProt(QString("%1\tВыполнение директивы недопустимо при открытом окне ручного режима!").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "13", textStyle());
        return false;
    }
    if (hasRunDirective.load() == 1) {
        printInProt(QString("%1\tВыполнение директивы недопустимо пока не заврешится предыдущая директика").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "13", textStyle());
        return false;
    }
    hasRunDirective.store(1);
    runDirectFunc(direct);
    hasRunDirective.store(0);

    return true;
}

void directRunner::setManualMode(){
    hasRunManualMode.store(1);
    printInProt(QString("%1\tВключили ручной режим").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "0", textStyle());
}

void directRunner::unSetManualMode(){
    hasRunManualMode.store(0);
    printInProt(QString("%1\tОтключили ручной режим").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")), "0", textStyle());
}


void directRunner::setTimeWorkAppcp(const QStringList &timeWorkAppcp){
    m_timeWorkAppcp = timeWorkAppcp;
    emit haveTimeWorkAppcp();
}

void directRunner::exitEvent() {
    if (socketCanal1->state() != QAbstractSocket::ConnectedState)
        return;

    QByteArray cBA;
    cBA.append(char(0xB0));

    socketCanal1->write(cBA, cBA.length());
    socketCanal1->flush();
    return;
}

bool directRunner::haveVolt() {
    if (!voltReady || voltSocket->state() != QAbstractSocket::ConnectedState)
        return false;

    QByteArray cBA;
    cBA.append(char(0x01)).append(char(0x01)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00));

    voltResponse.clear();



    //voltSocket->write(cBA);
    //voltSocket->flush();


    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QMetaObject::Connection con1 = QObject::connect(this, &directRunner::voltAnswer, [&loop](){
        qDebug() << "signal recived";
        loop.quit();
    });
    QMetaObject::Connection con2 = QObject::connect(&timer, &QTimer::timeout, [&loop](){
        qDebug() << "timer end";
        loop.quit();
    });

    voltSocket->write(cBA);
    voltSocket->flush();

    timer.start(600);
    loop.exec();

    timer.stop();
    QObject::disconnect(con1);
    QObject::disconnect(con2);

    if (voltResponse.count() != 8) {
        qDebug() << "response : " << voltResponse;
        qDebug() << "count: " << voltResponse.count();
        printInProt("Недопустимое количество байт в ответе от вольтметра", "13");
        return false;
    }

    if (voltResponse[0] != cBA[0]) {
        printInProt("Ответ от вольтметра получен на другую команду", "13");
        return false;
    }

    if (voltResponse[1] != char(0x00)) {
        printInProt(QString("Ошибка выполнения операции в вольтметре: %1").arg(voltErrorCode[voltResponse[1]]), "13");
        return false;
    }

    QString errorMessage;
    QString printMessage;


    cBA.clear();
    cBA.append(char(0x0a));
    bool status{false};
    sendMessageToNU(cBA.constData(), cBA.length(), &status);

    if (!status) return false;
    if (respondNU.length() == 2 && respondNU.at(1) == 0){
        //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
        printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
        printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
    }
    else if (respondNU.length() != 2){
        errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
        printInProt(errorMessage, "13", textStyle());
        return false;
    }
    else if (respondNU.at(1) == -1){
        errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
        printInProt(errorMessage, "13", textStyle());
        return false;
    }
    cBA.clear();
    printMessage.clear();
    errorMessage.clear();


    cBA.clear();
    cBA.append(0x22).append(0x01);
    sendMessageToNU(cBA, cBA.length(), &status);
    if (!status){
        errorMessage.append("Ошибка при выполнении команды в НУ");
        printInProt(errorMessage, "13", textStyle());
        return false;
    }
    if (respondNU.length() != 2){
        errorMessage.append("Ошибка в полученном ответе от НУ (ожидалось 2 байта)");
        printInProt(errorMessage, "13", textStyle());
        return false;
    }
    if (respondNU.at(1) == -1){
        errorMessage.append("Ошибка в аппаратуре НУ");
        printInProt(errorMessage, "13", textStyle());
        return false;
    }
    printMessage.append("Сопртивление 1 МОм подключено к корпусу");
    printInProt(printMessage, "30", textStyle());


    cBA.clear();
    printMessage.clear();
    errorMessage.clear();
    cBA.append(char(0x02));
    cBA.append(char(1));
    cBA.append(char(0x100));
    sendMessageToNU(cBA.constData(), cBA.length(), &status);

    if (!status) return false;
    if (respondNU.length() == 2 && respondNU.at(1) == 0){
        //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
        printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
        printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
    }
    else if (respondNU.length() != 2){
        errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
        printInProt(errorMessage, "13", textStyle());
        return false;
    }
    else if (respondNU.at(1) == -1){
        errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
        printInProt(errorMessage, "13", textStyle());
        return false;
    }
    cBA.clear();

    cBA.append(char(0x09));
    sendMessageToNU(cBA.constData(), cBA.length(), &status);
    float result{0};
    if (!status) return false;
    if (respondNU.length() == 2 + 4 && respondNU.at(1) == 0){
    //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
        printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
        QByteArray floatData = respondNU.mid(2);
        std::memcpy(&result, floatData.constData(), sizeof(result));
        if (std::isnan(result) || std::isinf(result)){
            errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (неудалось получить значение)");
            printInProt(errorMessage, "13", textStyle());
            return false;
        }
    }
    cBA.clear();
    printMessage.clear();
    errorMessage.clear();

    cBA.clear();
    cBA.append(char(0x0a));
    sendMessageToNU(cBA.constData(), cBA.length(), &status);

    if (!status) return false;
    if (respondNU.length() == 2 && respondNU.at(1) == 0){
        //printInProt("NET: получили ответ на ПСЦ", "30", textStyle());
        printInProt(QString("%1 NET: получили ответ на %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(constValues::NUDirectives.value(cBA[0])), "30", textStyle(), true, false);
        printInProt(QString("\t\t\t   Print_otv()   kom = 0x%1  kz = %2").arg(respondNU[0], 2, 16, QChar('0')).arg(int(respondNU[1])), "35", textStyle(), true, true);
    }
    else if (respondNU.length() != 2){
        errorMessage.append("\t\t\t\tОшибка в полученном ответе от НУ (ожидалось 2 байта)");
        printInProt(errorMessage, "13", textStyle());
        return false;
    }
    else if (respondNU.at(1) == -1){
        errorMessage.append("\t\t\t\tОшибка в аппаратуре НУ");
        printInProt(errorMessage, "13", textStyle());
        return false;
    }

    if (result >= 0 - 150 && result <= 0 + 150)
        return true;
    else
        return false;
}

double directRunner::getRWithVolt(int diap) {
    if (!voltReady || voltSocket->state() != QAbstractSocket::ConnectedState)
        return -1;

    QByteArray cBA;
    cBA.append(char(0x01)).append(char(0x01)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00));

    voltResponse.clear();



    //voltSocket->write(cBA);
    //voltSocket->flush();


    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QMetaObject::Connection con1 = QObject::connect(this, &directRunner::voltAnswer, [&loop](){
        loop.quit();
        qDebug() << "signal recived";
    });
    QMetaObject::Connection con2 = QObject::connect(&timer, &QTimer::timeout, [&loop](){
        loop.quit();
        qDebug() << "timer end";
    });

    voltSocket->write(cBA);
    voltSocket->flush();

    timer.start(600);
    loop.exec();

    timer.stop();
    QObject::disconnect(con1);
    QObject::disconnect(con2);

    if (voltResponse.count() != 8) {
        qDebug() << voltResponse;
        printInProt("Недопустимое количество байт в ответе от вольтметра", "13");
        return -1;
    }

    if (voltResponse[0] != cBA[0]) {
        printInProt("Ответ от вольтметра получен на другую команду", "13");
        return -1;
    }

    if (voltResponse[1] != char(0x00)) {
        printInProt(QString("Ошибка выполнения операции в вольтметре: %1").arg(voltErrorCode[voltResponse[1]]), "13");
        return -1;
    }


    cBA.clear();
    cBA.append(char(0x02)).append(char(0x01)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(diap)).append(char(0x00)).append(char(0x00));

    voltResponse.clear();



    //voltSocket->write(cBA);
    //voltSocket->flush();


    timer.setSingleShot(true);

    con1 = QObject::connect(this, &directRunner::voltAnswer, [&loop](){
        loop.quit();
    });
    con2 = QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    voltSocket->write(cBA);
    voltSocket->flush();

    timer.start(600);
    loop.exec();

    timer.stop();
    QObject::disconnect(con1);
    QObject::disconnect(con2);

    if (voltResponse.count() != 8) {
        printInProt("Недопустимое количество байт в ответе от вольтметра", "13");
        return -1;
    }

    if (voltResponse[0] != cBA[0]) {
        printInProt("Ответ от вольтметра получен на другую команду", "13");
        return -1;
    }

    if (voltResponse[1] != char(0x00)) {
        printInProt(QString("Ошибка выполнения операции в вольтметре: %1").arg(voltErrorCode[voltResponse[1]]), "13");
        return -1;
    }

    QByteArray floatData = respondNU.mid(2);
    quint32 result{0};
    std::memcpy(&result, floatData.constData(), sizeof(result));
    if (std::isnan(result) || std::isinf(result)){
        printInProt("Ошибка в полученном ответе от вольтметра (неудалось получить значение)", "13", textStyle());
        return -1;
    }


    cBA.clear();
    cBA.append(char(0x03)).append(char(0x01)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00));

    voltResponse.clear();



    //voltSocket->write(cBA);
    //voltSocket->flush();


    timer.setSingleShot(true);

    con1 = QObject::connect(this, &directRunner::voltAnswer, [&loop](){
        loop.quit();
    });
    con2 = QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    voltSocket->write(cBA);
    voltSocket->flush();

    timer.start(600);
    loop.exec();

    timer.stop();
    QObject::disconnect(con1);
    QObject::disconnect(con2);

    if (voltResponse.count() != 8) {
        printInProt("Недопустимое количество байт в ответе от вольтметра", "13");
        return -1;
    }

    if (voltResponse[0] != cBA[0]) {
        printInProt("Ответ от вольтметра получен на другую команду", "13");
        return -1;
    }

    if (voltResponse[1] != char(0x00)) {
        printInProt(QString("Ошибка выполнения операции в вольтметре: %1").arg(voltErrorCode[voltResponse[1]]), "13");
        return -1;
    }

    return double(result)/1000000;
}

bool directRunner::resetVolt() {
    QByteArray cBA;
    cBA.clear();
    cBA.append(char(0x03)).append(char(0x01)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00)).append(char(0x00));

    voltResponse.clear();



    //voltSocket->write(cBA);
    //voltSocket->flush();
    QTimer timer;
    QEventLoop loop;

    timer.setSingleShot(true);

    QMetaObject::Connection con1 = QObject::connect(this, &directRunner::voltAnswer, [&loop](){
        loop.quit();
    });
    QMetaObject::Connection con2 = QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    voltSocket->write(cBA);
    voltSocket->flush();

    timer.start(600);
    loop.exec();

    timer.stop();
    QObject::disconnect(con1);
    QObject::disconnect(con2);

    if (voltResponse.count() != 8) {
        printInProt("Недопустимое количество байт в ответе от вольтметра", "13");
        return false;
    }

    if (voltResponse[0] != cBA[0]) {
        printInProt("Ответ от вольтметра получен на другую команду", "13");
        return false;
    }

    if (voltResponse[1] != char(0x00)) {
        printInProt(QString("Ошибка выполнения операции в вольтметре: %1").arg(voltErrorCode[voltResponse[1]]), "13");
        return false;
    }
    return true;
}

void directRunner::voltRR() {
    voltResponse.clear();
    QByteArray answer = voltSocket->readAll();
    voltResponse = answer;

    printInProt(QString("%1 NETCL: получено %2 байтов").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(answer.length()), "13", textStyle(), true);
    printInProt(QString("\t\t\tПолучено сообщ. volt"), "23", textStyle(), true);
    QStringList byteList;

    int count = 0;
    for (unsigned char byte: answer){
        QString curByte;
        if (count == 0) curByte.append("\t\t\t");
        count += 1;
        curByte.append(QString("0x") + QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
        if (count >= 16){
            curByte += "\n";
            count = 0;
        }
        byteList << curByte;
    }
    byteList.first().prepend(" ");
    if (byteList.last().at(byteList.last().length()-1) == '\n') byteList.last().chop(1);
    printInProt(byteList.join(" "), "23", textStyle(), true);

    emit voltAnswer();
}
