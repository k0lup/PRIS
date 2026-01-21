#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets>
#include "manualmode.h"
#include "directparser.h"
#include "directrunner.h"
#include <QtSql>
#include <QLocalSocket>
#include <QThread>
#include "stepwgt.h"

struct contactAppcp{
    int raz;
    int kont;
    contactAppcp(int raz, int kont) : raz(raz), kont(kont){}
    contactAppcp() : raz(-1), kont(-1){}
};

extern QMap<QString, contactAppcp> appcpParam;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    bool statusOpenned;

    static QSqlQuery getQueryRRDB(const QString& queryString);
    static QString getCurCatalog();

    static QString getNumProduct(){return numProduct;}

    static QString getOnParam(const QString& paramName) {
        if (paramOnValues.contains(paramName)) return paramOnValues.value(paramName);
        else return QString();
    }

    static QString getCfgParam(const QString& paramName){
        if (paramValues.contains(paramName)) return paramValues.value(paramName);
        else return QString();
    }

    static QString getCfgFilePath(){ return cfgFilePath; }
    static QString getOnFilePath(){ return onFilePath; }
public slots:
    void showVariantDialogWindow(const QString& text, const QStringList& variants);

    void showDirectWindow(const QString& textDirect);

    void appendMessageToProtocol(const QString& message);
    void set100VStyleForProtocol();
    void unset100VStyleForProtocol();

    void addProgramToModel(const QString& programName, const QStringList& prText);
    void setNumDirInModel(const int numDir, const int numLine);
    void delProgramInModel();

    void showInfoStopWindow(const QString& infoMessage);
signals:
    void variantSelected(const QString& text, bool stopProg);

    void directVariantSelected(const directRunner::DIRECT_VARIABLE dirVar);

    void protocolSetStyleState();
    void protocolMessageSet();

    void programModelActionAccept();

    void closeInfoStopWindow();

    void runDirectives(const DirectParser::Direct &direct);

    void printMessageToProtocol(const QString& message, const QString& style, directRunner::textStyle tStyle = directRunner::textStyle(), bool nuMessage = false, bool onlyNUFile = true);

    void sendTimeWorkAppcp(const QStringList& timeWorkAppcp);
    void sendExitEventToNU();
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    ManualMode *manual;
    //QWidget *commandWidget;
    static QString numProduct;

    bool readConfigFile(const QString& filePath, QMap<QString, QString>& paramMap);
    static QMap<QString, QString> paramValues;
    static QMap<QString, QString> paramOnValues;
    static QSqlDatabase rrParDB;
    static QSqlDatabase appcpParDB;
    static QString curCatalog;

    static QString cfgFilePath;
    static QString onFilePath;
    //QString getConfigParam(const QString& paramName);

    directRunner *dirRunner;

    QTextEdit *protocolText;
    QWidget *protocolWgt;

    QStandardItemModel *programInfomodel;
    StepWgt *stepWgt;

    bool programDirectWindowClose;
    QThread *dirRunnerThread;
};
#endif // MAINWINDOW_H
