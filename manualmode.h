#ifndef MANUALMODE_H
#define MANUALMODE_H
#include <QtWidgets>


class ManualMode : public QWidget
{
    Q_OBJECT
public:
    ManualMode(QWidget *parent = nullptr);

    enum class ContStatus{
        CONNECTED_PLUS,
        CONNECTED_MINUS,
        DISCONNECTED,
        ERR_STATUS
    };

    ContStatus getContStatus(int numCont);
signals:
    void podkl(const unsigned char command, int contact, bool setConnect);
    void commandResultReady();

    void widgetShown();
    void widgetClosed();
private:
    enum class raz{
      X1,
      X2
    };
    enum class polus{
        PLUS,
        MINUS
    };

    QTextEdit *textEdit;

    QVector<QPushButton*> plX1btnVector;
    QVector<QPushButton*> plX2btnVector;
    QVector<QPushButton*> minX1btnVector;
    QVector<QPushButton*> minX2btnVector;

    QVector<ContStatus> contStatus;

    QAtomicInt commandStatus;
    QAtomicInt blockCommand;

    QPushButton *minGr;
    QPushButton *plGr;
    QPushButton *r50;
    QPushButton *r1;
    QPushButton *r5;
    QPushButton *r20;
    QPushButton *u;
    QPushButton *v100;
    QPushButton *conn;
    QPushButton *reset;

    QLabel *resultLabel;

    bool sendCommand(const unsigned char command, int contact = -1, bool setConnect = false);

    bool checkMinContConnected();
    bool checkPlusContConnected();

public slots:
    //void commandComplete(bool result, const QString& message);
    void commandComplete(bool result);
    void printMessage(const QString& message, const QString& color = "blue");
    void setResult(const float result);
private slots:
    bool contactSelected(raz x, polus p, int num);
    void resetConnections();
    void rControl(int diap = 0);
    void nControl();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
};

#endif // MANUALMODE_H
