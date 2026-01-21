#ifndef JSONRECEIVER_H
#define JSONRECEIVER_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

struct portBMessage{
    int type;
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    int ms;
    int code;
    QString text;
};

class JsonReceiver : public QObject
{
    Q_OBJECT
public:
    explicit JsonReceiver(QTcpSocket *socket, QObject *parent = nullptr);
private slots:
    void onReadyRead();
signals:
    void messageGet(portBMessage message);
    void errorGet(QString error);
    void bytesGet(QByteArray bytes);
private:
    void processMessage(const QJsonObject &obj);
    QTcpSocket *m_socket;
    QByteArray m_buffer;
    int m_expectedSize;
};

#endif // JSONRECEIVER_H
