#include "jsonreceiver.h"
#include <QDateTime>

JsonReceiver::JsonReceiver(QTcpSocket *socket, QObject *parent)
    : QObject(parent), m_socket(socket), m_expectedSize(0)
{
    connect(m_socket, &QTcpSocket::readyRead, this, &JsonReceiver::onReadyRead);
}

void JsonReceiver::onReadyRead(){
    m_buffer.append(m_socket->readAll());
    emit bytesGet(m_buffer);

    while (true) {
        if (m_expectedSize == 0) {
            int newLineIndex = m_buffer.indexOf('\n');
            if (newLineIndex == -1){
                if (m_buffer.size() > 5){
                    QString errorString("В первых 5 байтах не был обнаружен разделитель длины и JSON блока");
                    qWarning() << errorString;
                    emit errorGet(errorString);
                    m_buffer.clear();
                    m_expectedSize = 0;
                    return;
                }
                return;
            }
            QByteArray sizePart = m_buffer.left(newLineIndex);
            m_buffer.remove(0, newLineIndex + 1);

            bool ok = false;
            m_expectedSize = sizePart.toInt(&ok);
            if (!ok || m_expectedSize <= 0){
                QString errorString("Ошибка разбора длины JSON блока");
                qWarning() << errorString;
                emit errorGet(errorString);
                m_buffer.clear();
                m_expectedSize = 0;
                return;
            }
        }

        if (m_buffer.size() < m_expectedSize)
            return;

        QByteArray jsonByte = m_buffer.left(m_expectedSize);
        m_buffer.remove(0, m_expectedSize);
        m_expectedSize = 0;

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonByte, &parseError);
        if (parseError.error != QJsonParseError::NoError){
            QString errorString("Ошибка JSON: " + parseError.errorString());
            qWarning() << errorString;
            emit errorGet(errorString);
            continue;
        }

        if (!doc.isObject()){
            QString errorString("Ожидался объект JSON");
            qWarning() << errorString;
            emit errorGet(errorString);
            continue;
        }

        this->processMessage(doc.object());
    }
}

void JsonReceiver::processMessage(const QJsonObject &obj) {
    portBMessage structMessage;
    structMessage.type = obj.value("type").toInt();
    QString tsString = obj.value("timestamp").toString();
    QDateTime timestamp = QDateTime::fromString(tsString, Qt::ISODateWithMs);
    structMessage.year = timestamp.date().year();
    structMessage.month = timestamp.date().month();
    structMessage.day = timestamp.date().day();
    structMessage.hour = timestamp.time().hour();
    structMessage.min = timestamp.time().minute();
    structMessage.sec = timestamp.time().second();
    structMessage.ms = timestamp.time().msec();

    structMessage.code = obj.value("text").toInt();
    structMessage.text = obj.value("text").toString();

    emit messageGet(structMessage);
}
