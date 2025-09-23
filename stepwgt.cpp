#include "stepwgt.h"

StepWgt::StepWgt(QWidget *parent) : QWidget(parent)
{
    this->setWindowTitle("ПРИС: Окно шагового режима");
    this->setWindowFlags(Qt::Widget | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowStaysOnTopHint /*| Qt::FramelessWindowHint*/);


    QVBoxLayout *vBox = new QVBoxLayout();
    for (int i = 0; i < 5; i ++){
        QLabel *label = new QLabel(this);
        //label->setText(QString("ДИРЕКТИВА %1").arg(QString::number(i + 1)));
        vBox->addWidget(label);
        label->setStyleSheet("color: black; padding: 10px; border: 1px solid black;");
        list.append(label);
    }

    this->setLayout(vBox);
    vBox->setSpacing(0);
    vBox->setMargin(1);
    list[0]->setStyleSheet("background-color: blue; color: white; padding: 10px; border: 1px solid black;");
    this->resize(300, 200);

    startProgLine = 0;
}

void StepWgt::setProgram(const QStringList &programText){
    for (int i = 0; i < 5; i++){
        list[i]->setText("");
    }

   int numLabel = 0;
   int numLine = 0;
   this->startProgLine = 0;
   for (int row = 0; row < programText.count(); ++row){
       if (programText[row].startsWith("П!")) this->startProgLine += 1;
       else break;
   }

   while (numLine < programText.count() && numLabel < 5){
      if (numLine < this->startProgLine){
          numLine += 1;
          continue;
      }
      list[numLabel]->setText(programText[numLine]);
      numLabel += 1;
      numLine += 1;
   }

    this->prText = programText;

    emit this->acceptAction();
}

void StepWgt::setNumLine(const int numLine){
    for (int i = 0; i < 5; i++){
        list[i]->setText("");
    }

    int numLabel = 0;
    int numCurLine = numLine;
    while (numCurLine < this->prText.count() && numLabel < 5){
        if (numCurLine < this->startProgLine){
            numCurLine += 1;
            continue;
        }
        list[numLabel]->setText(this->prText[numCurLine]);
        numLabel += 1;
        numCurLine += 1;
    }

    emit this->acceptAction();
}

void StepWgt::removeProgram(){
    for (int i = 0; i < 5; i++){
        list[i]->setText("");
    }

    this->prText.clear();
    emit this->acceptAction();
}
