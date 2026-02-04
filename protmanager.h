#ifndef PROTMANAGER_H
#define PROTMANAGER_H
#include <QtWidgets>

extern QTextCodec *codec;

class ProtManager : public QObject
{
    Q_OBJECT
public:
    static ProtManager& instance(){
        static ProtManager manager;
        return manager;
    }
    bool createProtocol();
    bool writeRecord(QString param, const int atomType, int potok = 0);
    bool writeRecordToNU(QString record);

    bool saveFile(const QString& savePath, const QString& saveNUPath);
    bool isProtActive(){return activeProt;}
    QString getAllFileDate(QString& errorText);
    bool isValidProt(QString& errorText);
    QString getNewFileDate(QString& errorText);
    QString getFilePath();
signals:
    void fileUpdate();
    void fileSaved();
private:
    explicit ProtManager(QObject *parent = nullptr);
    ~ProtManager();
    QFile writeFile;
    QFile readFile;
    QFile nuFile;
    //static QTextCodec *codec;
    bool activeProt;
    int lastPosFileSeek;

    //QMap<QString, QString> curStyleParam;
    QString curHTMLStyleString;

    //QString protInfo;
};

#endif // PROTMANAGER_H
