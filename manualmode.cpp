#include "manualmode.h"
#include "constvalues.h"

const QString defaultBtnStyle{QString(R"(
                                      QPushButton {
                                          background-color: lightgray;
                                          border: 2 px solid gray;
                                      }
                                      QPushButton::checked {
                                          background-color: green;
                                          color: white;
                                      }
                                      )")};
const QString yellowBtnStyle{QString(R"(
                                     QPushButton {
                                         background-color: yellow;
                                         border: 2 px solid gray;
                                     }
                                     )")};

ManualMode::ManualMode(QWidget *parent) : QWidget(parent)
{
    this->setWindowTitle("Ручное управление");
    this->contStatus.fill(ContStatus::DISCONNECTED, 101);
    textEdit = new QTextEdit(this);
    textEdit->setMaximumHeight(this->height() * 0.1);
    textEdit->setMaximumWidth(this->width() * 0.4);
    textEdit->setReadOnly(true);
    textEdit->setText("");

    resultLabel = new QLabel("Измерения", this);
    QFont font = resultLabel->font();
    font.setPointSize(20);
    resultLabel->setFont(font);
    QHBoxLayout *hBox1 = new QHBoxLayout();
    hBox1->addWidget(textEdit);
    hBox1->addWidget(resultLabel);

    r50 = new QPushButton("R 50 Ом", this);
    r50->setMinimumHeight(r50->height());
    r1 = new QPushButton("R 1 Мом", this);
    r1->setMinimumHeight(r1->height());
    r5 = new QPushButton("R 5 Мом", this);
    r5->setMinimumHeight(r5->height());
    r20 = new QPushButton("R 20 Мом", this);
    r20->setMinimumHeight(r20->height());
    u = new QPushButton("НАПР", this);
    u->setMinimumHeight(u->height());
    reset = new QPushButton("СБРОС", this);
    reset->setMinimumHeight(reset->height());
    v100 = new QPushButton("100 В", this);
    v100->setMinimumHeight(v100->height());
    v100->setCheckable(true);
    v100->setStyleSheet(defaultBtnStyle);
    conn = new QPushButton("ПОДК_1М", this);
    conn->setMinimumHeight(conn->height());
    conn->setCheckable(true);
    conn->setStyleSheet(defaultBtnStyle);

    QObject::connect(conn, &QPushButton::clicked, [this](){
        bool res{false};
       if (conn->isChecked()){
           res = sendCommand(static_cast<char>(NUCommand::PODKL_1M), 1);
       } else{
           res = sendCommand(static_cast<char>(NUCommand::PODKL_1M), 0);
       }
       if (!res){
           conn->setChecked(!conn->isChecked());
           return;
       }
    });

    QObject::connect(r50, &QPushButton::clicked, [this](){
       this->rControl(0);
    });
    QObject::connect(r1, &QPushButton::clicked, [this](){
        this->rControl(1);
    });
    QObject::connect(r5, &QPushButton::clicked, [this](){
        this->rControl(2);
    });
    QObject::connect(r20, &QPushButton::clicked, [this](){
        this->rControl(3);
    });
    QObject::connect(u, &QPushButton::clicked, this, &ManualMode::nControl);

    QHBoxLayout *hBox2 = new QHBoxLayout();
    hBox2->addWidget(r50);
    hBox2->addWidget(r1);
    hBox2->addWidget(r5);
    hBox2->addWidget(r20);
    hBox2->addWidget(u);
    hBox2->addWidget(reset);
    hBox2->addWidget(v100);
    hBox2->addWidget(conn);

    QWidget *minConns = new QWidget(this);
    minX1btnVector.reserve(50);

        QWidget *minX1 = new QWidget(minConns);
        QLabel *minx1Lable = new QLabel("X1");
        QGridLayout *boxBtnMinX1 = new QGridLayout();
        for (int col = 0; col < 5; col++){
            //QVBoxLayout *vBoxBtn = new QVBoxLayout();
            for (int row = 0; row < 10; row++){
                QString text = QString::number(10 * col + row + 1);
                QPushButton *btn = new QPushButton(text, minConns);
                QRect border = btn->fontMetrics().boundingRect(QString::number(99));
                btn->setMaximumSize(border.width() * 3, border.height()*3);
                btn->setMinimumSize(border.width() * 3, border.height()*3);
                boxBtnMinX1->addWidget(btn, row, col);
                minX1btnVector.insert(10 * col + row, btn);
                btn->setCheckable(true);
                //btn->setStyleSheet("background-color: lightgray; color: black;");
                btn->setStyleSheet(defaultBtnStyle);
                QObject::connect(btn, &QPushButton::clicked, [btn, this](){
                    if (blockCommand.load() == 0){
                        if (!contactSelected(raz::X1, polus::MINUS, btn->text().toInt() - 1)) btn->setChecked(!btn->isChecked());
                    }
                    else btn->setChecked(!btn->isChecked());
                });
            }
            //hBoxBtnMinX1->addLayout(vBoxBtn);
        }
        QVBoxLayout *vBoxMinX1 = new QVBoxLayout();
        vBoxMinX1->addWidget(minx1Lable);
        vBoxMinX1->addLayout(boxBtnMinX1);
        minX1->setLayout(vBoxMinX1);

        minX2btnVector.reserve(50);
        QWidget *minX2 = new QWidget(minConns);
        QLabel *minx2Lable = new QLabel("X2");
        QGridLayout *boxBtnMinX2 = new QGridLayout();
        for (int col = 0; col < 5; col++){
            //QVBoxLayout *vBoxBtn = new QVBoxLayout();
            for (int row = 0; row < 10; row++){
                QPushButton *btn = new QPushButton(QString::number(10 * col + row + 1), minConns);
                QRect border = btn->fontMetrics().boundingRect(QString::number(99));
                btn->setMaximumSize(border.width() * 3, border.height()*3);
                btn->setMinimumSize(border.width() * 3, border.height()*3);
                boxBtnMinX2->addWidget(btn, row, col);
                minX2btnVector.insert(10 * col + row, btn);
                btn->setCheckable(true);
                //btn->setStyleSheet("background-color: lightgray; color: black;");
                btn->setStyleSheet(defaultBtnStyle);
                QObject::connect(btn, &QPushButton::clicked, [btn, this](){
                    if (blockCommand.load() == 0){
                        if (!contactSelected(raz::X2, polus::MINUS, btn->text().toInt() - 1)) btn->setChecked(!btn->isChecked());
                    }
                    else btn->setChecked(!btn->isChecked());
                });
            }
            //hBoxBtnMinX2->addLayout(vBoxBtn);
        }
        QVBoxLayout *vBoxMinX2 = new QVBoxLayout();
        vBoxMinX2->addWidget(minx2Lable);
        vBoxMinX2->addLayout(boxBtnMinX2);
        minX2->setLayout(vBoxMinX2);

        QHBoxLayout *hBoxMinConns = new QHBoxLayout();
        hBoxMinConns->addWidget(minX1);
        hBoxMinConns->addWidget(minX2);

        QLabel *minTitle = new QLabel("МИНУС АЦП", minConns);

        QVBoxLayout *vBoxMinConns = new QVBoxLayout();
        vBoxMinConns->addLayout(hBoxMinConns);
        vBoxMinConns->addWidget(minTitle);

        minConns->setLayout(vBoxMinConns);

        QWidget *plConns = new QWidget(this);

            plX1btnVector.reserve(50);
            QWidget *plX1 = new QWidget(plConns);
            QLabel *plx1Lable = new QLabel("X1");
            QGridLayout *boxBtnPlX1 = new QGridLayout();
            for (int col = 0; col < 5; col++){
                //QVBoxLayout *vBoxBtn = new QVBoxLayout();
                for (int row = 0; row < 10; row++){
                    QPushButton *btn = new QPushButton(QString::number(10 * col + row + 1), plConns);
                    QRect border = btn->fontMetrics().boundingRect(QString::number(99));
                    btn->setMaximumSize(border.width() * 3, border.height()*3);
                    btn->setMinimumSize(border.width() * 3, border.height()*3);
                    //vBoxBtn->addWidget(btn);
                    boxBtnPlX1->addWidget(btn, row, col);
                    plX1btnVector.insert(10 * col + row, btn);
                    btn->setCheckable(true);
                    //btn->setStyleSheet("background-color: lightgray; color: black;");
                    btn->setStyleSheet(defaultBtnStyle);
                    QObject::connect(btn, &QPushButton::clicked, [btn, this](){
                        if (blockCommand.load() == 0){
                            if (!contactSelected(raz::X1, polus::PLUS, btn->text().toInt() - 1)) btn->setChecked(!btn->isChecked());
                        }
                        else btn->setChecked(!btn->isChecked());
                    });
                }
                //hBoxBtnPlX1->addLayout(vBoxBtn);
            }
            QVBoxLayout *vBoxPlX1 = new QVBoxLayout();
            vBoxPlX1->addWidget(plx1Lable);
            vBoxPlX1->addLayout(boxBtnPlX1);
            plX1->setLayout(vBoxPlX1);

            plX2btnVector.reserve(50);
            QWidget *plX2 = new QWidget(plConns);
            QLabel *plx2Lable = new QLabel("X2");
            QGridLayout *boxBtnPlX2 = new QGridLayout();
            for (int col = 0; col < 5; col++){
                //QVBoxLayout *vBoxBtn = new QVBoxLayout();
                for (int row = 0; row < 10; row++){
                    QPushButton *btn = new QPushButton(QString::number(10 * col + row + 1), plConns);
                    QRect border = btn->fontMetrics().boundingRect(QString::number(99));
                    btn->setMaximumSize(border.width() * 3, border.height()*3);
                    btn->setMinimumSize(border.width() * 3, border.height()*3);
                    boxBtnPlX2->addWidget(btn, row, col);
                    plX2btnVector.insert(10 *col + row, btn);
                    btn->setCheckable(true);
                    //btn->setStyleSheet("background-color: lightgray; color: black;");
                    btn->setStyleSheet(defaultBtnStyle);
                    QObject::connect(btn, &QPushButton::clicked, [btn, this](){
                        if (blockCommand.load() == 0){
                            if (!contactSelected(raz::X2, polus::PLUS, btn->text().toInt() - 1)) btn->setChecked(!btn->isChecked());
                        }
                        else btn->setChecked(!btn->isChecked());
                    });
                }
                //hBoxBtnPlX2->addLayout(vBoxBtn);
            }
            QVBoxLayout *vBoxPlX2 = new QVBoxLayout();
            vBoxPlX2->addWidget(plx2Lable);
            vBoxPlX2->addLayout(boxBtnPlX2);
            plX2->setLayout(vBoxPlX2);

            QHBoxLayout *hBoxplConns = new QHBoxLayout();
            hBoxplConns->addWidget(plX1);
            hBoxplConns->addWidget(plX2);

            QLabel *plTitle = new QLabel("ПЛЮС АЦП", plConns);

            QVBoxLayout *vBoxplConns = new QVBoxLayout();
            vBoxplConns->addLayout(hBoxplConns);
            vBoxplConns->addWidget(plTitle);

            plConns->setLayout(vBoxplConns);

    minGr = new QPushButton("З", this);
    minGr->setIcon(QIcon(":/img/img/ground.png"));
    minGr->setCheckable(true);
    minGr->setStyleSheet(defaultBtnStyle);
    plGr = new QPushButton("З", this);
    plGr->setIcon(QIcon(":/img/img/ground.png"));
    plGr->setCheckable(true);
    plGr->setStyleSheet(defaultBtnStyle);
    QRect border = minGr->fontMetrics().boundingRect(minGr->text());
    int len = (border.width() > border.height()) ? border.width() : border.height();
    minGr->setMinimumSize(len * 3, len * 3);
    minGr->setMaximumSize(len * 3, len * 3);
    boxBtnMinX2->addWidget(minGr, 0, 5);
    boxBtnPlX2->addWidget(plGr, 0, 5);
    plGr->setMaximumSize(len * 3, len * 3);
    plGr->setMinimumSize(len * 3, len * 3);

    QHBoxLayout *hBox3 = new QHBoxLayout();
    hBox3->addWidget(minConns);
    //hBox3->addWidget(minGr, Qt::AlignTop);
    hBox3->addWidget(plConns);
    //hBox3->addWidget(plGr, Qt::AlignTop);

    QVBoxLayout *vBox = new QVBoxLayout();
    vBox->addLayout(hBox1);
    vBox->addLayout(hBox2);
    vBox->addLayout(hBox3);
    this->setLayout(vBox);


    QObject::connect(reset, &QPushButton::clicked, this, &ManualMode::resetConnections);
    QObject::connect(minGr, &QPushButton::clicked, [this](){
        if (minGr->isChecked()){
            if (contStatus[100] == ContStatus::CONNECTED_PLUS){
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), 100, false)){
                    minGr->setChecked(!minGr->isChecked());
                    return;
                }
                plGr->setChecked(false);
                contStatus[100] = ContStatus::DISCONNECTED;
            }
            /*for (auto &btn : minX1btnVector){
                btn->setChecked(false);
            }
            for (auto &btn : minX2btnVector){
                btn->setChecked(false);
            }*/
            for (int num = 0; num < minX1btnVector.length(); ++num){
                if (contStatus[num] == ContStatus::CONNECTED_MINUS){
                    if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), num, false)){
                        minGr->setChecked(!minGr->isChecked());
                        return;
                    }
                    minX1btnVector[num]->setChecked(false);
                    contStatus[num] = ContStatus::DISCONNECTED;
                }
                if (contStatus[50 + num] == ContStatus::CONNECTED_MINUS){
                    if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), num + 50, false)){
                        minGr->setChecked(!minGr->isChecked());
                        return;
                    }
                    minX2btnVector[num]->setChecked(false);
                    contStatus[num + 50] = ContStatus::DISCONNECTED;
                }
            }
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), 100, true)){
                minGr->setChecked(!minGr->isChecked());
                return;
            }
            contStatus[100] = ContStatus::CONNECTED_MINUS;
       } else{
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), 100, false)){
                minGr->setChecked(!minGr->isChecked());
                return;
            }
            contStatus[100] = ContStatus::DISCONNECTED;
        }
    });

    QObject::connect(plGr, &QPushButton::clicked, [this](){
        if (plGr->isChecked()){
            if (contStatus[100] == ContStatus::CONNECTED_MINUS){
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), 100, false)){
                    plGr->setChecked(!plGr->isChecked());
                    return;
                }
                minGr->setChecked(false);
                contStatus[100] = ContStatus::DISCONNECTED;
            }
            for (auto &btn : plX1btnVector){
                if (btn->isChecked()){
                    int numBtn = btn->text().toInt() - 1;
                    if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), numBtn, false)){
                        plGr->setChecked(!plGr->isChecked());
                        return;
                    }
                    contStatus[numBtn] = ContStatus::DISCONNECTED;
                    btn->setChecked(false);
                    minX1btnVector[numBtn]->setStyleSheet(defaultBtnStyle);
                }
            }
            for (auto &btn : plX2btnVector){
                if (btn->isChecked()){
                    int numBtn = btn->text().toInt() - 1;
                    if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), numBtn + 50, false)){
                        plGr->setChecked(!plGr->isChecked());
                        return;
                    }
                    contStatus[numBtn + 50] = ContStatus::DISCONNECTED;
                    btn->setChecked(false);
                    minX2btnVector[numBtn]->setStyleSheet(defaultBtnStyle);
                }
            }
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), 100, true)){
                plGr->setChecked(!plGr->isChecked());
                return;
            }
            contStatus[100] = ContStatus::CONNECTED_PLUS;
        } else{
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), 100, false)) {
                plGr->setChecked(!plGr->isChecked());
                return;
            }
            contStatus[100] = ContStatus::DISCONNECTED;
        }
    });

    QObject::connect(v100, &QPushButton::clicked, [this](){
       if (v100->isChecked()){
           r50->setEnabled(false);
       } else r50->setEnabled(true);
    });
    QObject::connect(conn, &QPushButton::clicked, [this](){
       if (conn->isChecked()) r50->setEnabled(false);
       else r50->setEnabled(true);
    });
    minGr->setText("");
    plGr->setText("");
}

bool ManualMode::contactSelected(raz x, polus p, int num){
    if (blockCommand.load() == 1){
        qDebug() << "BLOCKED";
        return false;
    } else qDebug() << "NOT BLOCKED";
    qDebug() << "RUNNED";
    if (num < 0 || num >= 50) return false;
    if (x == raz::X1 && p == polus::MINUS){
        if (!minX1btnVector[num]->isChecked()){
            //minX1btnVector[num]->setChecked(false);
            //emit podkl(static_cast<char>(NUCommand::PODKL_M), num, false);
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), num, false)) return false;
            minX1btnVector[num]->setStyleSheet(defaultBtnStyle);
            contStatus[num] = ContStatus::DISCONNECTED;
        }
        else{
            //minX1btnVector[num]->setChecked(true);
            if (contStatus[100] == ContStatus::CONNECTED_MINUS){
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), 100, false)) return false;
                minGr->setChecked(false);
                contStatus[100] = ContStatus::DISCONNECTED;
            }
            if (plX1btnVector[num]->isChecked() == true || contStatus[num] == ContStatus::CONNECTED_PLUS){
                //emit podkl(static_cast<char>(NUCommand::PODKL_P), num, false);
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), num, false)) return false;
                plX1btnVector[num]->setChecked(false);
                plX1btnVector[num]->setStyleSheet(defaultBtnStyle);
                contStatus[num] = ContStatus::DISCONNECTED;
            }
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), num, true)) return false;
            minX1btnVector[num]->setStyleSheet(defaultBtnStyle);
            contStatus[num] = ContStatus::CONNECTED_MINUS;
            //emit podkl(static_cast<char>(NUCommand::PODKL_M), num, true);
        }
    } else if (x == raz::X2 && p == polus::MINUS){
        if (!minX2btnVector[num]->isChecked()){
            //minX2btnVector[num]->setChecked(false);
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), num + 50, false)) return false;
            minX2btnVector[num]->setStyleSheet(defaultBtnStyle);
            contStatus[num + 50] = ContStatus::DISCONNECTED;
            //emit podkl(static_cast<char>(NUCommand::PODKL_M), num + 50, false);
        }
        else{
            //minX2btnVector[num]->setChecked(true);
            if (contStatus[100] == ContStatus::CONNECTED_MINUS){
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), 100, false)) return false;
                minGr->setChecked(false);
                contStatus[100] = ContStatus::DISCONNECTED;
            }
            if (plX2btnVector[num]->isChecked() == true || contStatus[num + 50] == ContStatus::CONNECTED_PLUS){
                //emit podkl(static_cast<char>(NUCommand::PODKL_P), num + 50, false);
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), num + 50, false)) return false;
                plX2btnVector[num]->setChecked(false);
                plX2btnVector[num]->setStyleSheet(defaultBtnStyle);
                contStatus[num + 50] = ContStatus::DISCONNECTED;
            }
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), num + 50, true)) return false;
            minX2btnVector[num]->setStyleSheet(defaultBtnStyle);
            contStatus[num + 50] = ContStatus::CONNECTED_MINUS;
            //emit podkl(static_cast<char>(NUCommand::PODKL_M), num + 50, true);
        }
    } else if (x == raz::X1 && p == polus::PLUS){
        if (!plX1btnVector[num]->isChecked()){
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), num, false)) return false;
            plX1btnVector[num]->setStyleSheet(defaultBtnStyle);
            minX1btnVector[num]->setStyleSheet(defaultBtnStyle);
            contStatus[num] = ContStatus::DISCONNECTED;
            {
                /*blockCommand.store(1);
                commandStatus.store(0);
                QEventLoop loop;
                QTimer timer;
                timer.setInterval(5000);
                QObject::connect(this, &ManualMode::commandResultReady, &loop, &QEventLoop::quit);
                QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
                emit podkl(static_cast<char>(NUCommand::PODKL_P), num, false);
                timer.start();
                loop.exec();
                blockCommand.store(0);*/
            }
        } else {
            if (contStatus[100] == ContStatus::CONNECTED_PLUS){
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), 100, false)) return false;
                plGr->setChecked(false);
                contStatus[100] = ContStatus::DISCONNECTED;
            }
            for (auto &btn : plX1btnVector){
                if (!btn->isChecked() || btn->text().toInt() - 1 == num) continue;
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), btn->text().toInt() - 1, false)) return false;
                btn->setChecked(false);
                contStatus[btn->text().toInt() - 1] = ContStatus::DISCONNECTED;
                {
                    /*blockCommand.store(1);
                    commandStatus.store(0);
                    QEventLoop loop;
                    QTimer timer;
                    timer.setInterval(5000);
                    QObject::connect(this, &ManualMode::commandResultReady, &loop, &QEventLoop::quit);
                    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
                    emit this->podkl(static_cast<char>(NUCommand::PODKL_P), btn->text().toInt() - 1, false);
                    timer.start();
                    loop.exec();
                    blockCommand.store(0);*/
                }
                //qDebug() << "DISCONNECT " << btn->text().toInt() - 1;
                btn->setStyleSheet(defaultBtnStyle);
                int btnNum = btn->text().toInt() - 1;
                minX1btnVector[btnNum]->setStyleSheet(defaultBtnStyle);
            }
            for (auto &btn : plX2btnVector){
                if (!btn->isChecked() || btn->text().toInt() - 1 == num) continue;
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), btn->text().toInt() - 1 + 50, false)) return false;
                contStatus[btn->text().toInt() - 1 + 50] = ContStatus::DISCONNECTED;
                //emit podkl(static_cast<char>(NUCommand::PODKL_P), btn->text().toInt() - 1 + 50, false);
                btn->setChecked(false);
                btn->setStyleSheet(defaultBtnStyle);
                int btnNum = btn->text().toInt() - 1;
                minX2btnVector[btnNum]->setStyleSheet(defaultBtnStyle);
            }
            //plX1btnVector[num]->setChecked(true);
            if (minX1btnVector[num]->isChecked() == true || contStatus[num] == ContStatus::CONNECTED_MINUS){
                {
                    /*blockCommand.store(1);
                    commandStatus.store(0);
                    QEventLoop loop;
                    QTimer timer;
                    timer.setInterval(5000);
                    QObject::connect(this, &ManualMode::commandResultReady, &loop, &QEventLoop::quit);
                    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
                    emit podkl(static_cast<char>(NUCommand::PODKL_M), num, false);
                    timer.start();
                    loop.exec();
                    blockCommand.store(0);*/
                    if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), num, false)) return false;
                    minX1btnVector[num]->setChecked(false);
                    contStatus[num] = ContStatus::DISCONNECTED;
                }
            }
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), num, true)) return false;
            minX1btnVector[num]->setStyleSheet(yellowBtnStyle);
            plX1btnVector[num]->setStyleSheet(defaultBtnStyle);
            contStatus[num] = ContStatus::CONNECTED_PLUS;
            //emit podkl(static_cast<char>(NUCommand::PODKL_P), num, true);
            {
                /*blockCommand.store(1);
                commandStatus.store(0);
                QEventLoop loop;
                QTimer timer;
                timer.setInterval(5000);
                QObject::connect(this, &ManualMode::commandResultReady, &loop, &QEventLoop::quit);
                QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
                emit podkl(static_cast<char>(NUCommand::PODKL_P), num, true);
                timer.start();
                loop.exec();
                qDebug() << "CONNECT " << num;
                blockCommand.store(0);*/
            }
        }
    } else if (x == raz::X2 && p == polus::PLUS){
        if (!plX2btnVector[num]->isChecked()){
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), num + 50, false)) return false;
            plX2btnVector[num]->setStyleSheet(defaultBtnStyle);
            minX2btnVector[num]->setStyleSheet(defaultBtnStyle);
            contStatus[num + 50] = ContStatus::DISCONNECTED;
            //emit podkl(static_cast<char>(NUCommand::PODKL_P), num + 50, false);
        } else{
            if (contStatus[100] == ContStatus::CONNECTED_PLUS){
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), 100, false)) return false;
                plGr->setChecked(false);
                contStatus[100] = ContStatus::DISCONNECTED;
            }
            for (auto &btn : plX1btnVector){
                if (!btn->isChecked() || btn->text().toInt() - 1 == num) continue;
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), btn->text().toInt() - 1, false)) return false;
                btn->setChecked(false);
                contStatus[btn->text().toInt() - 1] = ContStatus::DISCONNECTED;
                {
                    /*blockCommand.store(1);
                    commandStatus.store(0);
                    QEventLoop loop;
                    QTimer timer;
                    timer.setInterval(5000);
                    QObject::connect(this, &ManualMode::commandResultReady, &loop, &QEventLoop::quit);
                    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
                    emit this->podkl(static_cast<char>(NUCommand::PODKL_P), btn->text().toInt() - 1, false);
                    timer.start();
                    loop.exec();
                    blockCommand.store(0);*/
                }
                //qDebug() << "DISCONNECT " << btn->text().toInt() - 1;
                btn->setStyleSheet(defaultBtnStyle);
                int btnNum = btn->text().toInt() - 1;
                minX1btnVector[btnNum]->setStyleSheet(defaultBtnStyle);
            }
            for (auto &btn : plX2btnVector){
                if (!btn->isChecked() || btn->text().toInt() - 1 == num) continue;
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), btn->text().toInt() - 1 + 50, false)) return false;
                contStatus[btn->text().toInt() - 1 + 50] = ContStatus::DISCONNECTED;
                //emit podkl(static_cast<char>(NUCommand::PODKL_P), btn->text().toInt() - 1 + 50, false);
                btn->setChecked(false);
                btn->setStyleSheet(defaultBtnStyle);
                int btnNum = btn->text().toInt() - 1;
                minX2btnVector[btnNum]->setStyleSheet(defaultBtnStyle);
            }
            if (minX2btnVector[num]->isChecked() == true || contStatus[num + 50] == ContStatus::CONNECTED_MINUS){
                //emit podkl(static_cast<char>(NUCommand::PODKL_M), num + 50, false);
                if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), num + 50, false)) return false;
                minX2btnVector[num]->setChecked(false);
                contStatus[num + 50] = ContStatus::DISCONNECTED;
            }
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), num + 50, true)) return false;
            minX2btnVector[num]->setStyleSheet(yellowBtnStyle);
            plX2btnVector[num]->setStyleSheet(defaultBtnStyle);
            contStatus[num + 50] = ContStatus::CONNECTED_PLUS;
            //emit podkl(static_cast<char>(NUCommand::PODKL_P), num + 50, true);
        }
    }
    return true;
}

void ManualMode::commandComplete(bool result/*, const QString& message*/){
    //QString text = "<span stylecolor = %1>" + message + "</span>";
    /*QString text = QString(R"(<span style="white-space: pre; color: %1;">)") + message + "</span>";
    text.replace("\t", "");
    text.replace("РУ:", "");
    if (result){
        text = text.arg("blue");
        textEdit->append(text);
        commandStatus.store(1);
    }
    else{
        text = text.arg("red");
        textEdit->append(text);
        commandStatus.store(0);
    }*/
    if (result) commandStatus.store(1);
    else commandStatus.store(0);

    emit commandResultReady();
}

void ManualMode::printMessage(const QString& message, const QString& color){
    QString text = "<span style='white-space: pre; color: %1;'>" + message + "</span>";
    text = text.arg(color);
    text.replace("\t", "");
    text.replace("РУ:", "");
    textEdit->append(text);
}

bool ManualMode::sendCommand(const unsigned char command, int contact, bool setConnect){
    blockCommand.store(1);
    commandStatus.store(0);
    QEventLoop loop;
    QTimer timer;
    timer.setInterval(5000);
    QObject::connect(this, &ManualMode::commandResultReady, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    emit podkl(command, contact, setConnect);
    timer.start();
    loop.exec();
    blockCommand.store(0);
    return commandStatus.load();
}

bool ManualMode::checkMinContConnected(){
    for (const auto &contact : contStatus){
        if (contact == ContStatus::CONNECTED_MINUS) return true;
    }
    return false;
}

bool ManualMode::checkPlusContConnected(){
    for (const auto &contact : contStatus){
        if (contact == ContStatus::CONNECTED_PLUS) return true;
    }
    return false;
}

void ManualMode::resetConnections(){
    bool res = sendCommand(static_cast<char>(NUCommand::SBR_PODKL));
    if (!res) {
        resultLabel->setText("НЕ УДАЛОСЬ ВЫПОЛНИТЬ СБР_ПОДКЛ. ВСЕ ДЕЙСТВИЯ, КРОМЕ СБР_ПОДКЛ ЗАПРЕЩЕНЫ!");
        for (auto &btn: minX1btnVector){
            btn->setEnabled(false);
        }
        for (auto &btn : minX2btnVector){
            btn->setEnabled(false);
        }
        for (auto &btn : plX1btnVector){
            btn->setEnabled(false);
        }
        for (auto &btn : plX2btnVector){
            btn->setEnabled(false);
        }
        minGr->setEnabled(false);
        plGr->setEnabled(false);
        r50->setEnabled(false);
        r1->setEnabled(false);
        r5->setEnabled(false);
        r20->setEnabled(false);
        u->setEnabled(false);
        v100->setEnabled(false);
        conn->setEnabled(false);;
        return;
    } else {
        for (auto &btn: minX1btnVector){
            btn->setEnabled(true);
        }
        for (auto &btn : minX2btnVector){
            btn->setEnabled(true);
        }
        for (auto &btn : plX1btnVector){
            btn->setEnabled(true);
        }
        for (auto &btn : plX2btnVector){
            btn->setEnabled(true);
        }
        minGr->setEnabled(true);
        plGr->setEnabled(true);
        r50->setEnabled(true);
        r1->setEnabled(true);
        r5->setEnabled(true);
        r20->setEnabled(true);
        u->setEnabled(true);
        v100->setEnabled(true);
        conn->setEnabled(true);;
    }
    for (auto &btn: minX1btnVector){
        btn->setChecked(false);
        btn->setStyleSheet(defaultBtnStyle);
    }
    for (auto &btn : minX2btnVector){
        btn->setChecked(false);
        btn->setStyleSheet(defaultBtnStyle);
    }
    for (auto &btn : plX1btnVector){
        btn->setChecked(false);
    }
    for (auto &btn : plX2btnVector){
        btn->setChecked(false);
    }
    for (auto &cont : contStatus){
        cont = ContStatus::DISCONNECTED;
    }
    minGr->setChecked(false);
    plGr->setChecked(false);
    resultLabel->setText("Измерения");
    /*for (int num = 0; num < minX1btnVector.length(); ++num){
        if (contStatus[num] == ContStatus::CONNECTED_MINUS){
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), num, false)) return;
            minX1btnVector[num]->setChecked(false);
            contStatus[num] = ContStatus::DISCONNECTED;
        } else if (contStatus[num] == ContStatus::CONNECTED_PLUS){
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), num, false)) return;
            plX1btnVector[num]->setChecked(false);
            minX1btnVector[num]->setStyleSheet(defaultBtnStyle);
            contStatus[num] = ContStatus::DISCONNECTED;
        } else if (contStatus[num + 50] == ContStatus::CONNECTED_MINUS){
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), num + 50, false)) return;
            minX2btnVector[num]->setChecked(false);
            contStatus[num + 50] = ContStatus::DISCONNECTED;
        } else if (contStatus[num + 50] == ContStatus::CONNECTED_PLUS){
            if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), num + 50, false)) return;
            plX2btnVector[num]->setChecked(false);
            minX2btnVector[num]->setStyleSheet(defaultBtnStyle);
            contStatus[num + 50] = ContStatus::DISCONNECTED;
        }
    }
    if (contStatus[100] == ContStatus::CONNECTED_PLUS){
        if (!sendCommand(static_cast<char>(NUCommand::PODKL_P), 100, false)) return;
        plGr->setChecked(false);
        contStatus[100] = ContStatus::DISCONNECTED;
    }
    if (contStatus[100] == ContStatus::CONNECTED_MINUS){
        if (!sendCommand(static_cast<char>(NUCommand::PODKL_M), 100, false)) return;
        minGr->setChecked(false);
        contStatus[100] = ContStatus::DISCONNECTED;
    }*/
    v100->setChecked(false);
    conn->setChecked(false);
    r50->setEnabled(true);
}

void ManualMode::rControl(int diap){
    if (!checkMinContConnected() || !checkPlusContConnected()){
        textEdit->append(QString(R"(<span style="white-space: pre; color: red;">Нет подключенных контактов ПЛЮС и МИНУС</span>)"));
        return;
    }
    if (diap == 0){
        if (!sendCommand(static_cast<char>(NUCommand::ISM_SN))) return;
    } else{
        if (!sendCommand(static_cast<char>(NUCommand::ISM_SV), diap, v100->isChecked())) return;
    }
}

void ManualMode::setResult(const float result){
    resultLabel->setText(QString::number(result, 'f', 4));
}

void ManualMode::nControl(){
    if (!checkMinContConnected() || !checkPlusContConnected()){
        textEdit->append(QString(R"(<span style="white-space: pre; color: red;">Нет подключенных контактов ПЛЮС и МИНУС</span>)"));
        return;
    }
    if (!sendCommand(static_cast<char>(NUCommand::ISM_NAPR))) return;
}

void ManualMode::showEvent(QShowEvent *event){
    emit widgetShown();
    QWidget::showEvent(event);
    reset->clicked(true);
}

void ManualMode::closeEvent(QCloseEvent *event){
    reset->clicked(true);
    emit widgetClosed();
    QWidget::closeEvent(event);
}
