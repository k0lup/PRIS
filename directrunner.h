#ifndef DIRECTRUNNER_H
#define DIRECTRUNNER_H
#include "directparser.h"
#include <QTextEdit>
#include <QStack>
#include <QQueue>
#include <QStandardItemModel>
#include <QtWidgets>
#include "protmanager.h"
#include "jsonreceiver.h"
#include <QTcpSocket>

class directRunner : public QObject
{
    Q_OBJECT
public:
    /*static directRunner& instance(){
        static directRunner directrunner;
        return directrunner;
    }*/
    QAtomicInt hasConnectNU;
    QAtomicInt hasRunDirective;
    QAtomicInt hasRunManualMode;
    QAtomicInt stepMode;
    QAtomicInt reactStopMode;
    QAtomicInt ost_flag;
    QAtomicInt m_ost_flag;
    QAtomicInt trackMode;
    directRunner(QObject *parent = nullptr);
    ~directRunner();
    //void addCommand(const QString& operatorCommand);
    void startWork();
    //void resetMetka(){metka = "";}

    static QMap<QString, QString> getStyleInfo(const QString& styleName) {if (styles.contains(styleName)) return styles.value(styleName); else return QMap<QString, QString>();}

    struct textStyle{
        int potok;
        QString color;
        QString backColor;
        QString fontName;
        int fontSize;
        int charSet;
        bool bold;
        bool italic;
        bool underLine;
        bool strikeOut;

        textStyle() : potok(-1) {}
        textStyle(int potok, QString color = "clWindowText", QString backColor = "clWindow", QString fontName = "Courier New", int fontSize = 10, int charSet = 1, bool bold = false, bool italic = false, bool underLine = false, bool strikeOut = false) :
            potok(potok), color(color), backColor(backColor), fontName(fontName), fontSize(fontSize), charSet(charSet), bold(bold), italic(italic), underLine(underLine), strikeOut(strikeOut) {}
    };

    struct programStruct{
        QString programName;
        QList<DirectParser::Direct*> directList;
        int numDir;
        QMap<QString, int> metkaAddr;
        bool autoRun;
        bool blockRun;
        QString infoStopMsg;
        QStringList programText;

        programStruct() : numDir(-1) {}
    };

    enum class DIRECT_VARIABLE{
        OK_GO,
        OK_STOP,
        NOT_OK_GO,
        NOT_OK_STOP,
        EMTY_VAR
    };

signals:
    void showVarDialogWindow(const QString& text, const QStringList& var);
    void varinantkSelectedVar();

    void appendMessageToProtocol(const QString& message);
    void messageSet();

    void v100Selected();
    void v100Canceled();
    void styleSet();

    void showDirectWindow(const QString& text);
    void directSelectedVar();

    void addProgramToModel(const QString& prName, const QStringList& prText);
    void setProgramNumDirInModel(const int numDir, const int numLine);
    void removeProgramInModel();

    void programModelActionAccept();

    void setStopState(const QString& infoMessage);
    void unsetStopState();

    void socketRRMes();

    //void manualCommandComplete(bool result, const QString& message);
    void manualCommandComplete(bool result);
    void printMessageToManualWindow(const QString& message, const QString& color);
    void sendResulToManualWindow(const float result);

    void requestTimeWorkAppcp();
    void haveTimeWorkAppcp();

    void voltAnswer();

    void errorProtValid(const QString errorProt);
    void stStopRequested();
public slots:
    void setVarinatkVar(const QString& var, bool stopProg);

    bool runDirect(const DirectParser::Direct &direct);
    //bool runCommandNU(const unsigned char command, int contact = -1, bool setConnect = true);
    bool runCommandNU(const unsigned char command, int contact = -1, bool setConnect = true);

    void setManualMode();
    void unSetManualMode();

    void setDirectVar(DIRECT_VARIABLE dirVar);

    void printInProt(const QString &text, const QString &styleName, const textStyle& style = textStyle(), bool nuMessage = false, bool onlyNUFile = true);

    void connectNU();
    void disconnectNU();

    void setTimeWorkAppcp(const QStringList& timeWorkAppcp);

    void exitEvent();

    void voltRR();
private:
    QAtomicInt waitNUMessage;
    QString GL_VAR;
    static QString metka;
    bool GL_NORM_STATUS;
    bool GL_MODE_NEXT;


    QStringList m_timeWorkAppcp;
    bool v100Mode;

    //ProtManager protWirter;

    static bool hasRunProg;
    static bool stopProg;

    static DIRECT_VARIABLE dirVar;

    static QStack<directRunner::programStruct> programs;
    static QQueue<DirectParser::Direct> command;

    static QList<int> directNumPotok;

    static QMap<QString, QMap<QString, QString>> styles;

    void sendMessageToNU(const char *data, int len, bool *status);
    QString& getMetka(){return metka;}
    void printStartMessage();

    QByteArray respondNU;

    void runProgram(/*QTextEdit *protocol, QWidget *protocolWgt, QStandardItemModel *programInfomodel*/);
    void addProgram(const QString& fileName);

    bool runDirectFunc(const DirectParser::Direct &direct);



    QTcpSocket *socketCanal1;
    QTcpSocket *socketCanal2;
    bool can1RR;
    bool can2RR;

    QString stopMessageStr;

    JsonReceiver *jsonReceiver;


    QTcpSocket *voltSocket;

    bool hasVoltMode;
    bool voltReady;

    bool haveVolt();
    double getRWithVolt(int diap);
    bool resetVolt();

    bool blockAllIsm;

    QByteArray voltResponse;

};

#endif // DIRECTRUNNER_H
