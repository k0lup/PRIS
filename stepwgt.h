#ifndef STEPWGT_H
#define STEPWGT_H
#include <QtWidgets>
#include "directrunner.h"

class StepWgt : public QWidget
{
    Q_OBJECT
public:
    StepWgt(QWidget *parent = nullptr);

public slots:
    void setProgram(const QStringList &programText);

    void setNumLine(const int numLine);

    void removeProgram();
signals:
    void acceptAction();
private:
    QList<QLabel*> list;
    QStringList prText;
    int startProgLine;
};

#endif // STEPWGT_H
