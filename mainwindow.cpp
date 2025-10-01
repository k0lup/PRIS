#include "mainwindow.h"
#include "widgetinfo.h"
#include "commandline.h"
#include <QProcess>
#include <QtSql>
#include <QtConcurrent>
#include <QThread>
#include "dialogwgt.h"
#include "protmanager.h"
#include "textsearcher.h"
#include "constvalues.h"
#include "helpbrowser.h"
#include <QHelpEngine>
#include <QHelpContentWidget>
#include <QHelpIndexWidget>

#include <QLocalSocket>


QSqlDatabase MainWindow::rrParDB = QSqlDatabase();
QSqlDatabase MainWindow::appcpParDB = QSqlDatabase();
QString MainWindow::curCatalog = "";
QString MainWindow::numProduct = "";
QString MainWindow::cfgFilePath = "";
QString MainWindow::onFilePath = "";

QMap<QString, QString> MainWindow::paramOnValues = QMap<QString, QString>();
QMap<QString, QString> MainWindow::paramValues = QMap<QString, QString>();

QMap<QString, contactAppcp> appcpParam = QMap<QString, contactAppcp>();

const static QString TIME_CONTROL_MEMORY = "TimerControlAppcp284v2";
const static QString LOCAL_SERVER_TIME_CONTROL = "TimeControlAppcp284Server";

QString MainWindow::getCurCatalog(){
    return curCatalog;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    //blockDirectRun.store(0);
    QString filePath = QFileDialog::getOpenFileName(this, "Выберите файл конфигурации", "", "*.cfg");
    if (filePath.isEmpty() || QFileInfo(filePath).suffix().toUpper() != "CFG"){
        QString errorMessage(QString("CFG_NOT_OPEN"));
        QMessageBox::critical(nullptr, "Ошибка", errorMessage);
        statusOpenned = false;
        //QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
    }
    if (!readConfigFile(filePath, paramValues)){
        QString errorMessage(QString("CFG_NOT_OPEN"));
        QMessageBox::critical(nullptr, "Ошибка", errorMessage);
        //QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        statusOpenned = false;
        return;
    }
    cfgFilePath = filePath;
    QString fileOnPath = QFileDialog::getOpenFileName(this, "Выберите файл настройки", "", "*.on");
    if (fileOnPath.isEmpty() || QFileInfo(fileOnPath).suffix().toUpper() != "ON"){
        QString errorMessage(QString("ON_NOT_OPEN"));
        QMessageBox::critical(nullptr, "Ошибка", errorMessage);
        statusOpenned = false;
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
    }
    if (!readConfigFile(fileOnPath, paramOnValues)){
        QString errorMessage(QString("ON_NOT_OPEN"));
        QMessageBox::critical(nullptr, "Ошибка", errorMessage);
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        statusOpenned = false;
        return;
    }
    statusOpenned = true;
    onFilePath = fileOnPath;

    if (paramValues.contains("ИМИТ") && paramValues.value("ИМИТ").toUpper() == "ДА"){
        constValues::isImitMode.store(1);
    } else{
        constValues::isImitMode.store(0);
    }

    if (paramValues.contains("ВКЛ_КС") && paramValues.value("ВКЛ_КС").toUpper() == "ДА"){
        constValues::isNeedCheckKS.store(1);
    } else{
        constValues::isNeedCheckKS.store(0);
    }


    if (paramValues.contains("ПОРТ_ПРИЕМ_ПЕРЕДАЧА")){
        bool ok{false};
        QString str = paramValues.value("ПОРТ_ПРИЕМ_ПЕРЕДАЧА");
        int val{0};
        if (str.startsWith("0x", Qt::CaseInsensitive)){
            str = str.mid(2);
            val = str.toInt(&ok, 16);
        } else{
            val = str.toInt(&ok);
        }
        if (!ok){
            QMessageBox::critical(nullptr, "Ошибка", "Не удалось получить номер порта ПРИЕМ_ПЕРЕДАЧА");
        } else{
            qDebug() << "WAR: " << val;
            portAppcpWriteAndRead = val;
        }
    }

    if (paramValues.contains("ПОРТ_ТОЛЬКО_ПРИЕМ")){
        bool ok{false};
        QString str = paramValues.value("ПОРТ_ТОЛЬКО_ПРИЕМ");
        int val{0};
        if (str.startsWith("0x", Qt::CaseInsensitive)){
            str = str.mid(2);
            val = str.toInt(&ok, 16);
        } else{
            val = str.toInt(&ok);
        }
        if (!ok){
            QMessageBox::critical(nullptr, "Ошибка", "Не удалось получить номер порта ТОЛЬКО_ПРИЕМ");
        } else{
            qDebug() << "OR: " << val;
            portAppcpOnlyRead = val;
        }
    }

    this->curCatalog = paramOnValues.value("ПРОГРАММЫ");
    if (curCatalog.isEmpty()) this->curCatalog = QCoreApplication::applicationDirPath();


    //dirRunner = &directRunner::instance();
    dirRunner = new directRunner();

    QObject::connect(dirRunner, &directRunner::showVarDialogWindow, this, &MainWindow::showVariantDialogWindow, Qt::QueuedConnection);
    QObject::connect(this, &MainWindow::variantSelected, dirRunner, &directRunner::setVarinatkVar);

    QObject::connect(dirRunner, &directRunner::v100Selected, this, &MainWindow::set100VStyleForProtocol, Qt::QueuedConnection);
    QObject::connect(dirRunner, &directRunner::v100Canceled, this, &MainWindow::unset100VStyleForProtocol, Qt::QueuedConnection);
    QObject::connect(this, &MainWindow::protocolSetStyleState, dirRunner, &directRunner::styleSet);

    QObject::connect(dirRunner, &directRunner::appendMessageToProtocol, this, &MainWindow::appendMessageToProtocol, Qt::QueuedConnection);
    QObject::connect(this, &MainWindow::protocolMessageSet, dirRunner, &directRunner::messageSet);

    QObject::connect(dirRunner, &directRunner::showDirectWindow, this, &MainWindow::showDirectWindow, Qt::QueuedConnection);
    QObject::connect(this, &MainWindow::directVariantSelected, dirRunner, &directRunner::setDirectVar);

    QObject::connect(dirRunner, &directRunner::addProgramToModel, this, &MainWindow::addProgramToModel, Qt::QueuedConnection);
    QObject::connect(dirRunner, &directRunner::setProgramNumDirInModel, this, &MainWindow::setNumDirInModel, Qt::QueuedConnection);
    QObject::connect(dirRunner, &directRunner::removeProgramInModel, this, &MainWindow::delProgramInModel, Qt::QueuedConnection);
    QObject::connect(this, &MainWindow::programModelActionAccept, dirRunner, &directRunner::programModelActionAccept);

    QObject::connect(dirRunner, &directRunner::setStopState, this, &MainWindow::showInfoStopWindow, Qt::QueuedConnection);
    QObject::connect(dirRunner, &directRunner::unsetStopState, this, &MainWindow::closeInfoStopWindow, Qt::QueuedConnection);

    stepWgt = new StepWgt();
    stepWgt->hide();

    programDirectWindowClose = false;

    manual = new ManualMode();
    QMenu *fileMenu = new QMenu("Файл", this);
    QMenu *correctionMenu = new QMenu("Правка", this);
    QMenu *settingsMenu = new QMenu("Настройка", this);
    QMenu *viewerMenu = new QMenu("Просмотр", this);
    QMenu *traktMenu = new QMenu("Тракты", this);
    QMenu *paramMenu = new QMenu("Параметры", this);
    QMenu *directMenu = new QMenu("Директивы", this);
    QMenu *helpMenu = new QMenu("?", this);
    QAction *catalogSelect = fileMenu->addAction("Каталог");
    QMenu *fileTypeMenu = fileMenu->addMenu("Файлы");
        QAction *dipFileSelect = fileTypeMenu->addAction("*.DIP");
        QAction *fileSelect = fileTypeMenu->addAction("Все");
    QAction *exitAction = fileMenu->addAction("Выход");

    QAction *cancelAction = correctionMenu->addAction("Отмена");
    cancelAction->setShortcut(QKeySequence("ALT+BkSp"));
    cancelAction->setEnabled(false);
    QAction *eraseAction = correctionMenu->addAction("Вырезать");
    eraseAction->setShortcut(QKeySequence("CTRL+X"));
    eraseAction->setEnabled(false);
    QAction *copyAction = correctionMenu->addAction("Копировать");
    copyAction->setShortcut(QKeySequence("CTRL+C"));
    copyAction->setEnabled(false);
    QAction *pasteAction = correctionMenu->addAction("Вставить");
    pasteAction->setShortcut(QKeySequence("CTRL+V"));
    pasteAction->setEnabled(false);
    QAction *clearAction = correctionMenu->addAction("Очистить");
    clearAction->setShortcut(QKeySequence("CTRL+DEL"));
    clearAction->setEnabled(false);
    QAction *selectAllAction = correctionMenu->addAction("Выделить все");
    selectAllAction->setEnabled(false);

    QMenu *modeMenu = settingsMenu->addMenu("Режимы");
        QAction *modeTestAVT = modeMenu->addAction("Режим испытаний АВТ");
        modeTestAVT->setCheckable(true);
        modeTestAVT->setChecked(true);
        QAction *printNorm = modeMenu->addAction("Печать норм");
        printNorm->setCheckable(true);
        printNorm->setChecked(true);
        QAction *setReactNotNormNext = modeMenu->addAction("Реакция на ненорм. СЛЕД");
        setReactNotNormNext->setCheckable(true);
        setReactNotNormNext->setChecked(true);
    QAction *numProduct = settingsMenu->addAction("Номер изделия");

    QAction *curProtocol = viewerMenu->addAction("Текущий протокол");
    QAction *listSaveComand = viewerMenu->addAction("Список сохраненных команд");
    QAction *fileAction = viewerMenu->addAction("Файл");
    QAction *savedProtocol = viewerMenu->addAction("Сохраненный протокол");
    QAction *editorCyclogram = viewerMenu->addAction("Редактор циклограмм");

    QAction *setConnectNU = traktMenu->addAction("Установить связь с НУ");
    QAction *disconnectNU = traktMenu->addAction("Разорвать связь с НУ");
    QAction *manualControl = traktMenu->addAction("Ручное управление");
    QAction *param = traktMenu->addAction("Параметры");
    QAction *track = traktMenu->addAction("Трасса");
    track->setCheckable(true);
    track->setChecked(false);
    dirRunner->trackMode.store(0);
    QAction *sendAnswer = traktMenu->addAction("Послать ответ");
    QAction *printCompCoed = traktMenu->addAction("Распечатать компьютерный коэффициент");

    QAction *paramAPPCP = paramMenu->addAction("Параметры АППЦП");
    QAction *paramRR = paramMenu->addAction("РР параметры");

    QAction *directList = directMenu->addAction("Список директив");

    QAction *referense = helpMenu->addAction("Справка по ПРИС");
    QAction *directReferense = helpMenu->addAction("Справка по директиве");
    directReferense->setEnabled(false);
    QAction *searchSection = helpMenu->addAction("Поиск раздела");

    menuBar()->addMenu(fileMenu);
    menuBar()->addMenu(correctionMenu);
    menuBar()->addMenu(settingsMenu);
    menuBar()->addMenu(viewerMenu);
    menuBar()->addMenu(traktMenu);
    menuBar()->addMenu(paramMenu);
    menuBar()->addMenu(directMenu);
    menuBar()->addSeparator();
    menuBar()->addMenu(helpMenu);

    QToolBar *toolBar = new QToolBar(this);
    QMenu *programActionMenu = new QMenu("Оп. с прог.");
        QAction *callAction = programActionMenu->addAction("Вызвать");
        callAction->setShortcut(QKeySequence("CTRL+1"));
        //QAction *startAction = programActionMenu->addAction("ПУСК");
        //startAction->setShortcut(QKeySequence("CTRL+2"));
        QAction *start = programActionMenu->addAction("ПУСК");
        start->setShortcut(QKeySequence("CTRL+2"));
        QAction *toDirectAction = programActionMenu->addAction("НА");
        toDirectAction->setShortcut(QKeySequence("CTRL+3"));
        QAction *exitProgramAction = programActionMenu->addAction("Выход");
        exitProgramAction->setShortcut(QKeySequence("CTRL+4"));
        QAction *rExitProgramAction = programActionMenu->addAction("РВЫХОД");
        rExitProgramAction->setShortcut(QKeySequence("CTRL+5"));
        QAction *repeatAction = programActionMenu->addAction("Повтор");
        repeatAction->setShortcut(QKeySequence("CTRL+6"));
    QToolButton *programMenuButton = new QToolButton(this);
    programMenuButton->setText("Оп. с прог.");
    programMenuButton->setMenu(programActionMenu);
    programMenuButton->setPopupMode(QToolButton::InstantPopup);

    toolBar->addWidget(programMenuButton);
    toolBar->addSeparator();
    QAction *SrStop = toolBar->addAction("СрОст");
    //QAction *start = toolBar->addAction("Пуск");
    toolBar->addAction(start);
    QAction *PFKS = toolBar->addAction("ПФКС");
    QAction *srostM = toolBar->addAction("Срост_М");
    QAction *setManualMode = toolBar->addAction("Режим ручного управления");
    QAction *msWordOpen = toolBar->addAction("MS WORD");
    msWordOpen->setEnabled(false);

    this->addToolBar(toolBar);

    QWidget *centralWgt = new QWidget(this);
    this->setCentralWidget(centralWgt);

    protocolWgt = new QWidget(centralWgt);
    QLabel *titleProtocolWgt = new QLabel("Протокол испытаний", protocolWgt);
    protocolText = new QTextEdit(protocolWgt);
    protocolText->setReadOnly(true);
    protocolText->setWordWrapMode(QTextOption::NoWrap);
    QVBoxLayout *vBoxProtocolWgt = new QVBoxLayout(protocolWgt);
    vBoxProtocolWgt->addWidget(titleProtocolWgt);
    vBoxProtocolWgt->addWidget(protocolText);
    protocolWgt->setLayout(vBoxProtocolWgt);

    QWidget *commandLineWgt = new QWidget(centralWgt);
    QLabel *titleCommandLineWgt = new QLabel("Рзд", commandLineWgt);
    //QLineEdit *commandLine = new QLineEdit(commandLineWgt);
    CommandLine *commandLine = new CommandLine(commandLineWgt);
    QPushButton *showWindowCommandLineWgt = new QPushButton("...", commandLineWgt);

    QHBoxLayout *hBoxCommandLineWgt = new QHBoxLayout(commandLineWgt);
    hBoxCommandLineWgt->addWidget(titleCommandLineWgt);
    hBoxCommandLineWgt->addWidget(commandLine);
    hBoxCommandLineWgt->addWidget(showWindowCommandLineWgt);

    commandLineWgt->setLayout(hBoxCommandLineWgt);

    QWidget *infoWgt = new QWidget(centralWgt);
    infoWgt->setMaximumWidth(250);
    WidgetInfo *timerInfoWgt = new WidgetInfo(infoWgt);
    timerInfoWgt->setTitle("Таймеры");

    QLabel *curDateLabel = new QLabel(timerInfoWgt);
    QString str = QDateTime::currentDateTime().toString("ddd, dd MM yyyy");
    curDateLabel->setText("Дата: " + str);
    QLabel *curTimeLabel = new QLabel(timerInfoWgt);
    str = QDateTime::currentDateTime().toString("HH:mm:ss");
    curTimeLabel->setText("Время: " + str);
    QLabel *rSekLabel = new QLabel(timerInfoWgt);
    rSekLabel->setText("Сек Р: 00:00:00");
    QVBoxLayout *vBoxTimerInfoWgt = new QVBoxLayout();
    vBoxTimerInfoWgt->addWidget(curDateLabel);
    vBoxTimerInfoWgt->addWidget(curTimeLabel);
    vBoxTimerInfoWgt->addWidget(rSekLabel);
    timerInfoWgt->setLayout(vBoxTimerInfoWgt);
    QTimer *timer = new QTimer(infoWgt);
    QElapsedTimer *elapsedTimer = new QElapsedTimer();
    elapsedTimer->start();
    QObject::connect(timer, &QTimer::timeout, [curDateLabel, curTimeLabel, rSekLabel, elapsedTimer](){
        qint64 elMS = elapsedTimer->elapsed();
        qint64 elHours = elMS / (1000 * 60 * 60);
        qint64 elMin = (elMS / (1000 * 60)) % 60;
        qint64 elSec = (elMS / 1000) % 60;

        curDateLabel->setText("Дата: " + QDateTime::currentDateTime().toString("ddd, dd MM yyyy"));
        curTimeLabel->setText("Время: " + QDateTime::currentDateTime().toString("HH:mm:ss"));
        rSekLabel->setText(QString("Сек Р: %1:%2:%3").arg(elHours, 2, 10, QChar('0')).arg(elMin, 2, 10, QChar('0')).arg(elSec, 2, 10, QChar('0')));
    });
    timer->start(1000);


    WidgetInfo *programInfo = new WidgetInfo(infoWgt);
    programInfo->setTitle("Программы");
    QTableView *tableView = new QTableView(programInfo);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    programInfomodel = new QStandardItemModel(centralWgt);
    programInfomodel->setHorizontalHeaderLabels({"№", "Имя программы", "№ директивы"});
    tableView->setModel(programInfomodel);
    tableView->resizeColumnsToContents();
    QHBoxLayout *programInfoHBox = new QHBoxLayout();
    programInfoHBox->addWidget(tableView);
    programInfo->setLayout(programInfoHBox);


    WidgetInfo *modeInfo = new WidgetInfo(infoWgt);
    modeInfo->setTitle("Режимы");
    QLabel *KA = new QLabel("КА: ", modeInfo);
    QLabel *nameKA = new QLabel(modeInfo);
    QLabel *modeTest = new QLabel("Режим испытаний", modeInfo);
    QLabel *react = new QLabel("Реак. на ненорм.", modeInfo);
    QPushButton *modeTestType = new QPushButton("АВТ", modeInfo);
    modeTestType->setStyleSheet("color: green;");
    QObject::connect(modeTestAVT, &QAction::triggered, modeTestType, &QPushButton::clicked);
    QObject::connect(modeTestType, &QPushButton::clicked, [this, modeTestType, modeTestAVT](){
        if (modeTestType->text() == "АВТ"){
            modeTestAVT->setChecked(false);
            modeTestAVT->setText("Режим испытаний ШАГ");
            modeTestType->setText("ШАГ");
            modeTestType->setStyleSheet("color: orange;");
            this->dirRunner->stepMode.store(1);
            this->stepWgt->show();
            emit this->printMessageToProtocol("\t\tИСП ШАГ", "0");
        } else{
            modeTestAVT->setChecked(true);
            modeTestAVT->setText("Режим испытаний АВТ");
            modeTestType->setText("АВТ");
            modeTestType->setStyleSheet("color: green;");
            this->dirRunner->stepMode.store(0);
            this->stepWgt->hide();
            emit this->printMessageToProtocol("\t\tИСП АВТ", "0");
        }
    });

    QPushButton *reactType = new QPushButton("СЛЕД", modeInfo);
    reactType->setStyleSheet("color: green");
    QObject::connect(setReactNotNormNext, &QAction::triggered, reactType, &QPushButton::clicked);
    QObject::connect(reactType, &QPushButton::clicked, [reactType, setReactNotNormNext, this](){
       if (reactType->text() == "СЛЕД"){
           setReactNotNormNext->setChecked(false);
           setReactNotNormNext->setText("Реакция на ненорм. СТОП");
           reactType->setText("СТОП");
           reactType->setStyleSheet("color: red;");
           this->dirRunner->reactStopMode.store(1);
           emit this->printMessageToProtocol("\t\tРЕАК СТОП", "0");

       } else{
           setReactNotNormNext->setChecked(true);
           setReactNotNormNext->setText("Реакция на ненорм. СЛЕД");
           reactType->setText("СЛЕД");
           reactType->setStyleSheet("color: green;");
           this->dirRunner->reactStopMode.store(0);
           emit this->printMessageToProtocol("\t\tРЕАК СЛЕД", "0");
       }
    });
    QGridLayout *modeInfoGridLayout = new QGridLayout();
    modeInfoGridLayout->addWidget(KA, 1, 1);
    modeInfoGridLayout->addWidget(nameKA, 1, 2);
    modeInfoGridLayout->addWidget(modeTest, 2, 1);
    modeInfoGridLayout->addWidget(modeTestType, 2, 2);
    modeInfoGridLayout->addWidget(react, 3, 1);
    modeInfoGridLayout->addWidget(reactType, 3, 2);

    modeInfo->setLayout(modeInfoGridLayout);
    modeInfo->setObjectName("modeInfo");
    modeInfo->setStyleSheet("#modeInfo {"
                            "border: 2px solid black;"
                            "}");

    QVBoxLayout *vBoxInfoWgt = new QVBoxLayout(infoWgt);
    vBoxInfoWgt->addWidget(timerInfoWgt, Qt::AlignTop);
    vBoxInfoWgt->addWidget(programInfo, Qt::AlignTop);
    vBoxInfoWgt->addWidget(modeInfo, Qt::AlignTop);
    vBoxInfoWgt->addStretch();

    infoWgt->setLayout(vBoxInfoWgt);

    timerInfoWgt->setMaximumHeight(150);
    programInfo->setMaximumHeight(250);
    modeInfo->setMaximumHeight(150);

    QVBoxLayout *vBoxProtocol = new QVBoxLayout();
    vBoxProtocol->addWidget(protocolWgt);
    vBoxProtocol->addWidget(commandLineWgt);

    QHBoxLayout *hBox = new QHBoxLayout(centralWgt);
    hBox->addLayout(vBoxProtocol);
    hBox->addWidget(infoWgt, 0, Qt::AlignRight);

    centralWgt->setLayout(hBox);

    QObject::connect(manualControl, &QAction::triggered, [this](){
        if (!manual){
            manual = new ManualMode();
        }
        if (dirRunner->hasRunDirective.load() == 1){
            //dirRunner->printInProt("Открытие окна ручного режима невозможно при наличии выполняемой директивы", "13", directRunner::textStyle());
            emit this->printMessageToProtocol("Открытие окна ручного режима невозможно при наличии выполняемой директивы", "13");
            return;
        } else if (dirRunner->hasConnectNU.load() == 0){
            emit this->printMessageToProtocol("Открытие окна ручного режима невозможно при отсутствии подключения к НУ", "13");
            return;
        }
        else{
            manual->show();
        }
    });

    QObject::connect(setManualMode, &QAction::triggered, [this](){
        if (!manual){
            manual = new ManualMode();
        }
        if (dirRunner->hasRunDirective.load() == 1){
            //dirRunner->printInProt("Открытие окна ручного режима невозможно при наличии выполняемой директивы", "13", directRunner::textStyle());
            emit this->printMessageToProtocol("Открытие окна ручного режима невозможно при наличии выполняемой директивы", "13");
            return;
        } else if (dirRunner->hasConnectNU.load() == 0){
            emit this->printMessageToProtocol("Открытие окна ручного режима невозможно при отсутствии подключения к НУ", "13");
            return;
        }
        else{
            manual->show();
        }
    });
    QObject::connect(track, &QAction::triggered, [this, track](){
        //qDebug() << track->isChecked();
        //track->setChecked(false);
        if (track->isChecked()){
            dirRunner->trackMode.store(1);
            //track->setChecked(false);
        } else{
            dirRunner->trackMode.store(0);
            //track->setChecked(true);
        }
        /*if (track->text() == "Трасса ВКЛ"){
       //if (track->isChecked()){
           dirRunner->trackMode.store(0);
           track->setChecked(false);
           track->setText("Трасса ВЫКЛ");
       } else{
           dirRunner->trackMode.store(1);
           track->setChecked(true);
           track->setText("Трасса ВКЛ");
       }*/
    });
    QObject::connect(showWindowCommandLineWgt, &QPushButton::clicked, [commandLine, this](){
        static QWidget *commandWidget;
        if (!commandWidget){
            commandWidget = new QWidget();
            QTextEdit *txtEdit = new QTextEdit(commandWidget);
            QPushButton *okBtn = new QPushButton("Ок" ,commandWidget);
            QPushButton *cancelBtn = new QPushButton("Отмена", commandWidget);

            QHBoxLayout *hBox = new QHBoxLayout();
            hBox->addStretch();
            hBox->addWidget(okBtn, Qt::AlignRight);
            hBox->addWidget(cancelBtn, Qt::AlignRight);

            QVBoxLayout *vBox = new QVBoxLayout();
            vBox->addWidget(txtEdit);
            vBox->addLayout(hBox);
            commandWidget->setLayout(vBox);

            QObject::connect(okBtn, &QPushButton::clicked, [txtEdit, commandLine](){
                commandLine->setText(txtEdit->toPlainText());
                commandWidget->close();
            });
            QObject::connect(cancelBtn, &QPushButton::clicked, commandWidget, &QWidget::close);
        }
        commandWidget->raise();
        commandWidget->activateWindow();
        commandWidget->show();
    });

    QObject::connect(numProduct, &QAction::triggered, [this](){
       static QWidget *numWgt;
       if (!numWgt){
           numWgt = new QWidget();

           QLineEdit *lineEdit = new QLineEdit(numWgt);
           lineEdit->setText(this->numProduct);
           QPushButton *okBtn = new QPushButton("Сохранить", numWgt);

           QVBoxLayout *vBox = new QVBoxLayout();
           vBox->addWidget(lineEdit);
           vBox->addWidget(okBtn);

           numWgt->setLayout(vBox);

           QObject::connect(okBtn, &QPushButton::clicked, [this, lineEdit](){
               this->numProduct = lineEdit->text();
               numWgt->close();

           });
       }
       numWgt->raise();
       numWgt->activateWindow();
       numWgt->show();
    });



    this->rrParDB = QSqlDatabase::addDatabase("QODBC", "RR PAR");

    QString dbFilePath = paramOnValues.value("РР_ПАРАМЕТРЫ");
    if (dbFilePath.isEmpty() || (QFileInfo(dbFilePath).suffix().toUpper() != "MDB" && QFileInfo(dbFilePath).suffix().toUpper() != "ACCDB")){
        QString errorMessage(QString("RR_DB_NOT_OPEN"));
        QMessageBox::critical(nullptr, "Ошибка", errorMessage);
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    }

    if (!QFile::exists(dbFilePath)){
        if (!QFile::copy(":/DB/EmptyBD.mdb", dbFilePath)){
            QString errorMessage(QString("RR_DB_NOT_OPEN"));
            QMessageBox::critical(nullptr, "Ошибка", errorMessage);
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }
    }


    QFile(dbFilePath).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    //rrParDB.setDatabaseName(QString("DRIVER={Microsoft Access Driver (*.mdb, *.accdb)};DBQ=%1;CharSet=Windows-1251").arg(dbFilePath));
    rrParDB.setDatabaseName(QString("DRIVER={Driver do Microsoft Access (*.mdb)};DBQ=%1;CharSet=Windows-1251").arg(dbFilePath));

    if (!rrParDB.open()){
        QString errorMessage(QString("RR_DB_NOT_OPEN"));
        QMessageBox::critical(nullptr, "Ошибка", errorMessage);
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        qDebug() << "DB_NOT_OPEN";
    }

    appcpParDB = QSqlDatabase::addDatabase("QODBC", "APPCP PAR");
    dbFilePath = paramValues.value("БАЗА_ДАННЫХ");
    ipAppcpServ = paramValues.value("СЕРВЕР_АППЦП");

    if (dbFilePath.isEmpty() || (QFileInfo(dbFilePath).suffix().toUpper() != "MDB" && QFileInfo(dbFilePath).suffix().toUpper() != "ACCDB")){
        QString errorMessage(QString("APPCP_DB_NOT_OPEN"));
        QMessageBox::critical(nullptr, "Ошибка", errorMessage);
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    }

    if (!QFile::exists(dbFilePath)){
        if (!QFile::copy(":/DB/EmptyBD.mdb", dbFilePath)){
            QString errorMessage(QString("APPCP_DB_NOT_OPEN"));
            QMessageBox::critical(nullptr, "Ошибка", errorMessage);
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }
    }

    //appcpParDB.setDatabaseName(QString("DRIVER={Microsoft Access Driver (*.mdb, *.accdb)};DBQ=%1;CharSet=Windows-1251").arg(dbFilePath));
    QFile(dbFilePath).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    appcpParDB.setDatabaseName(QString("DRIVER={Driver do Microsoft Access (*.mdb)};DBQ=%1;CharSet=Windows-1251").arg(dbFilePath));
    if (!appcpParDB.open()){
        QString errorMessage(QString("APPCP_DB_NOT_OPEN"));
        QMessageBox::critical(nullptr, "Ошибка", errorMessage);
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        qDebug() << "DB_NOT_OPEN";
    }



    {
        QSqlQuery query(appcpParDB);
        if (!query.exec("SELECT ID, Nom, Kont FROM APPCP_ZO")){
            QString errorMessage(QString("Ошибка получения параметров АППЦП из БД\n") + query.lastError().text());
            QMessageBox::critical(nullptr, "Ошибка", errorMessage);
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }
        while (query.next()){
            QString value = query.value(0).toString();
            bool ok{false};
            int raz = query.value(1).toInt(&ok);
            int cont = query.value(2).toInt(&ok);
            if (!ok){
                QString errorMessage(QString("Ошибка получения параметров АППЦП из БД\n") + query.lastError().text());
                QMessageBox::critical(nullptr, "Ошибка", errorMessage);
                QTimer::singleShot(0, qApp, &QCoreApplication::quit);
                return;
            }
            appcpParam.insert(value, contactAppcp(raz, cont));
        }
        query.clear();
    }

    //qDebug() << appcpParam;
    //qDebug() << appcpParam.length();
    //qDebug() << appcpParam.last();

    nameKA->setText(paramValues.value("КОСМИЧЕСКИЙ_АППАРАТ"));


    this->raise();
    this->activateWindow();
    this->show();
    QObject::connect(correctionMenu, &QMenu::aboutToShow, [this, commandLine, cancelAction, copyAction, eraseAction, selectAllAction, pasteAction, clearAction](){
        if (commandLine->hasCancelAction()) cancelAction->setEnabled(true);
        else cancelAction->setEnabled(false);

        if (commandLine->hasSelectedText()){
            copyAction->setEnabled(true);
            eraseAction->setEnabled(true);
        } else{
            copyAction->setEnabled(false);
            eraseAction->setEnabled(false);
        }

        if (!commandLine->getText().isEmpty()){
            selectAllAction->setEnabled(true);
            clearAction->setEnabled(true);
        } else{
            selectAllAction->setEnabled(false);
            clearAction->setEnabled(false);
        }

        pasteAction->setEnabled(false);
        QClipboard *clipboard = QApplication::clipboard();
        if (clipboard != nullptr && clipboard->mimeData() != nullptr && clipboard->mimeData()->hasText()){
            pasteAction->setEnabled(true);
        }
        //проверить буффер обмена на наличие данных
    });

    QObject::connect(cancelAction, &QAction::triggered, [commandLine](){
        commandLine->cancelLastAction();
    });

    QObject::connect(copyAction, &QAction::triggered, [commandLine](){
        /*QClipboard *clipboard = QApplication::clipboard();
        if (clipboard){
            QString text = commandLine->getSelectedText();
            clipboard->setText(text);
        }*/
        commandLine->getCommandLineEdit()->copy();
    });

    QObject::connect(eraseAction, &QAction::triggered, [commandLine](){
        /*QClipboard *clipboard = QApplication::clipboard();
        if (clipboard){
            QString text = commandLine->getSelectedText();
            clipboard->setText(text);
            commandLine->removeSelectedText();
        }*/
        commandLine->getCommandLineEdit()->cut();
    });

    QObject::connect(selectAllAction, &QAction::triggered, [commandLine](){
        commandLine->selectedAll();
    });

    QObject::connect(pasteAction, &QAction::triggered, [commandLine](){
        /*QString textForPaste = "";
        QClipboard *clipboard = QApplication::clipboard();
        if (clipboard && clipboard->mimeData() && clipboard->mimeData()->hasText())
            textForPaste = clipboard->mimeData()->text();*/
        commandLine->getCommandLineEdit()->paste();

    });
    QObject::connect(clearAction, &QAction::triggered, [commandLine](){
        commandLine->clearText();
    });
    QObject::connect(directList, &QAction::triggered, [this, commandLine](){
        static QWidget *directInfoWgt;
        if (!directInfoWgt){
            directInfoWgt = new QWidget();

            QSqlQueryModel *model = new QSqlQueryModel(directInfoWgt);
            model->setQuery("SELECT NumDir AS N, NameDir AS директива, FullNameDir AS назначение_директивы, Potok AS поток, KO as КО, TF AS ТФ, KF AS КФ FROM Dirs", this->appcpParDB);
            QTableView *tableView = new QTableView(directInfoWgt);
            tableView->setModel(model);
            tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
            tableView->setSelectionMode(QAbstractItemView::SingleSelection);
            tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
            tableView->setStyleSheet(R"(
                QTableView::item:selected {
                    background-color: #0078D7;
                    color: white;
                }
                QTableView::item:selected:!active {
                    background-color: #0078D7;
                    color: white;
                }
            )");
            tableView->resizeColumnsToContents();

            QPushButton *toTop = new QPushButton("T", directInfoWgt);
            QPushButton *toUp = new QPushButton("U", directInfoWgt);
            QPushButton *toDown = new QPushButton("D", directInfoWgt);
            QPushButton *toBottom = new QPushButton("B", directInfoWgt);
            QLineEdit *findLineEdit = new QLineEdit(directInfoWgt);
            QPushButton *copy = new QPushButton("копировать", directInfoWgt);

            QHBoxLayout *hBox = new QHBoxLayout();
            hBox->addWidget(toTop, Qt::AlignLeft);
            hBox->addWidget(toUp, Qt::AlignLeft);
            hBox->addWidget(toDown, Qt::AlignLeft);
            hBox->addWidget(toBottom, Qt::AlignLeft);
            hBox->addWidget(findLineEdit, Qt::AlignLeft);
            hBox->addWidget(copy, Qt::AlignLeft);

            QVBoxLayout *vBox = new QVBoxLayout();
            vBox->addLayout(hBox);
            vBox->addWidget(tableView);
            directInfoWgt->setLayout(vBox);

            QObject::connect(toTop, &QPushButton::clicked, [tableView, model](){
               QItemSelectionModel *selectModel = tableView->selectionModel();
               QModelIndex index = model->index(0, 0);
               selectModel->clearSelection();
               selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
               tableView->setCurrentIndex(index);
               tableView->scrollTo(index);
            });

            QObject::connect(toDown, &QPushButton::clicked, [tableView, model](){
               QItemSelectionModel *selectModel = tableView->selectionModel();
               QModelIndex index = tableView->currentIndex();
               if (index.row() < 0 || index.row() >= model->rowCount()) index = model->index(0, 0);
               int rowIndex = (index.row() + 1 >= model->rowCount()) ? index.row() : index.row() + 1;
               index = model->index(rowIndex, 0);
               selectModel->clearSelection();
               selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
               tableView->setCurrentIndex(index);
               tableView->scrollTo(index);
            });

            QObject::connect(toUp, &QPushButton::clicked, [tableView, model](){
               QItemSelectionModel *selectModel = tableView->selectionModel();
               QModelIndex index = tableView->currentIndex();
               if (index.row() < 0 || index.row() >= model->rowCount()) index = model->index(0, 0);
               int rowIndex = (index.row() - 1 < 0) ? index.row() : index.row() - 1;
               index = model->index(rowIndex, 0);
               selectModel->clearSelection();
               selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
               tableView->setCurrentIndex(index);
               tableView->scrollTo(index);
            });

            QObject::connect(toBottom, &QPushButton::clicked, [tableView, model](){
               QItemSelectionModel *selectModel = tableView->selectionModel();
               QModelIndex index = model->index(model->rowCount() - 1, 0);
               selectModel->clearSelection();
               selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
               tableView->setCurrentIndex(index);
               tableView->scrollTo(index);
            });

            QObject::connect(copy, &QPushButton::clicked, [commandLine, tableView, model](){
               int row = tableView->currentIndex().row();
               if (row < 0 || row >= model->rowCount()) return;
               QString direct = model->data(model->index(row, 1)).toString();

               commandLine->appendText(direct);
               directInfoWgt->close();
            });

            QObject::connect(findLineEdit, &QLineEdit::textChanged, [tableView, model, findLineEdit](){
                QString findStr = findLineEdit->text();
                if (findStr.isEmpty()) return;

                int findRowIndex = -1;
                for (int row = 0; row < model->rowCount(); ++row){
                    QModelIndex col1 = model->index(row, 1);
                    QModelIndex col2 = model->index(row, 2);

                    if (model->data(col1).toString().contains(findStr, Qt::CaseInsensitive) || model->data(col2).toString().contains(findStr, Qt::CaseInsensitive)){
                        findRowIndex = row;
                        break;
                    }
                }
                if (findRowIndex == -1) return;

                QItemSelectionModel *selectModel = tableView->selectionModel();
                selectModel->clearSelection();
                QModelIndex index = model->index(findRowIndex, 0);
                selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                tableView->setCurrentIndex(index);
                tableView->scrollTo(index);
            });
            //db.close();
        }

        directInfoWgt->raise();
        directInfoWgt->activateWindow();
        directInfoWgt->show();
    });
    QObject::connect(&ProtManager::instance(), &ProtManager::fileSaved, this, [this](){
          this->protocolText->setText("");
        //this->protocolText->append("ЗАКРЫТИЕ");
        //QMetaObject::invokeMethod(protocolText, "setText", Qt::BlockingQueuedConnection, Q_ARG(QString, ""));
    }, Qt::QueuedConnection);
    QObject::connect(paramAPPCP, &QAction::triggered, [this, commandLine](){
       static QWidget *paramAPPCPInfo;
       if (!paramAPPCPInfo){
           paramAPPCPInfo = new QWidget();
           //QSqlDatabase db;
           //QString dbFilePath = this->paramValues.value("БАЗА_ДАННЫХ");
           //if (dbFilePath.isEmpty() || (QFileInfo(dbFilePath).suffix().toUpper() != "MDB" && QFileInfo(dbFilePath).suffix().toUpper() != "ACCDB")) return;

           //db = QSqlDatabase::addDatabase("QODBC", "MS ACCESS CONNECTION 1");
           //QString connectionString = QString("DRIVER={Microsoft Access Driver (*.mdb, *.accdb)};DBQ=%1;CharSet=Windows-1251").arg(dbFilePath);
           //db.setDatabaseName(connectionString);

           /*if (!db.open()){
               qDebug() << "dbNotOpen";
               return;
           }*/

           QSqlQueryModel *model = new QSqlQueryModel(paramAPPCPInfo);
           model->setQuery(QString("SELECT Nom AS разъем, ID AS Идентификатор, N AS №, Kont AS контакт FROM APPCP_ZO"), this->appcpParDB);

           QTableView *tableView = new QTableView(paramAPPCPInfo);
           tableView->setModel(model);
           tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
           tableView->setSelectionMode(QAbstractItemView::SingleSelection);
           tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
           tableView->setStyleSheet(R"(
                                    QTableView::item::selected {
                                        background-color: #0078D7;
                                        color: white;
                                    }
                                    QTableView::item::selected:!active {
                                        background-color: #0078D7;
                                        color: white;
                                    }
                                    )");
           tableView->resizeColumnsToContents();

           QPushButton *topBtn = new QPushButton("t", paramAPPCPInfo);
           QPushButton *upBtn = new QPushButton("u", paramAPPCPInfo);
           QPushButton *downBtn = new QPushButton("d", paramAPPCPInfo);
           QPushButton *bottomBtn = new QPushButton("b", paramAPPCPInfo);

           QPushButton *findBtn = new QPushButton("найти", paramAPPCPInfo);
           QPushButton *copyBtn = new QPushButton("Копировать", paramAPPCPInfo);

           QHBoxLayout *hBox = new QHBoxLayout();
           hBox->addWidget(topBtn);
           hBox->addWidget(upBtn);
           hBox->addWidget(downBtn);
           hBox->addWidget(bottomBtn);

           hBox->addWidget(findBtn);

           hBox->addWidget(copyBtn);

           QVBoxLayout *vBox = new QVBoxLayout();
           vBox->addLayout(hBox);
           vBox->addWidget(tableView);

           paramAPPCPInfo->setLayout(vBox);

           QObject::connect(topBtn, &QPushButton::clicked, [tableView, model](){
              QItemSelectionModel *selectModel = tableView->selectionModel();
              selectModel->clearSelection();

              QModelIndex index = model->index(0, 0);
              selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
              tableView->setCurrentIndex(index);
              tableView->scrollTo(index);
           });

           QObject::connect(upBtn, &QPushButton::clicked, [tableView, model](){
              QModelIndex index = tableView->currentIndex();
              if (index.row() < 0 || index.row() >= model->rowCount()){
                  index = model->index(0, 0);
              }
              int rowIndex = (index.row() - 1 >= 0) ? index.row() - 1 : index.row();
              index = model->index(rowIndex, 0);

              QItemSelectionModel *selectModel = tableView->selectionModel();
              selectModel->clearSelection();
              selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
              tableView->setCurrentIndex(index);
              tableView->scrollTo(index);
           });

           QObject::connect(downBtn, &QPushButton::clicked, [tableView, model](){
              QModelIndex index = tableView->currentIndex();
              if (index.row() < 0 || index.row() >= model->rowCount()){
                  index = model->index(0, 0);
              }
              int rowIndex = (index.row() + 1 < model->rowCount()) ? index.row() + 1 : index.row();
              index = model->index(rowIndex, 0);

              QItemSelectionModel *selectModel = tableView->selectionModel();
              selectModel->clearSelection();
              selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
              tableView->setCurrentIndex(index);
              tableView->scrollTo(index);
           });

           QObject::connect(bottomBtn, &QPushButton::clicked, [tableView, model](){
               QModelIndex index = model->index(model->rowCount() - 1, 0);

               QItemSelectionModel *selectModel = tableView->selectionModel();
               selectModel->clearSelection();
               selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
               tableView->setCurrentIndex(index);
               tableView->scrollTo(index);
           });

           QObject::connect(findBtn, &QPushButton::clicked, [tableView, model](){
               QDialog *findDialog = new QDialog();
               findDialog->setModal(true);

               QLabel *titleColFind = new QLabel("Поле", findDialog);
               QLabel *titleValFind = new QLabel("Значение", findDialog);

               QComboBox *colsFind = new QComboBox(findDialog);
               colsFind->addItem("Идентификатор");

               QLineEdit *valFind = new QLineEdit(findDialog);

               QCheckBox *enCodeValCheckBox = new QCheckBox(findDialog);
               QLabel *enCodeValLabel = new QLabel("Перекодировка значения", findDialog);

               QPushButton *findVal = new QPushButton("Найти", findDialog);

               QGridLayout *gridLayout = new QGridLayout();
               gridLayout->addWidget(titleColFind, 0, 0);
               gridLayout->addWidget(titleValFind, 0, 1);
               gridLayout->addWidget(colsFind, 1, 0);
               gridLayout->addWidget(valFind, 1, 1);

               QHBoxLayout *hBox = new QHBoxLayout();
               hBox->addWidget(enCodeValCheckBox, 0, Qt::AlignLeft);
               hBox->addWidget(enCodeValLabel, 0, Qt::AlignLeft);

               gridLayout->addLayout(hBox, 2, 0, Qt::AlignLeft);
               gridLayout->addWidget(findVal, 2, 1, Qt::AlignRight);

               findDialog->setLayout(gridLayout);

               QObject::connect(findVal, &QPushButton::clicked, [tableView, model, enCodeValCheckBox, valFind, findDialog](){
                  QString valString = valFind->text().trimmed();
                  if (enCodeValCheckBox->isChecked()){
                      auto recode = [&](const QString &text) -> QString{
                          static const QMap<QChar, QChar> rusToEng={
                              {u'А', u'A'}, {u'В', u'B'}, {u'Е', u'E'}, {u'К', u'K'},
                              {u'М', u'M'}, {u'Н', u'H'}, {u'О', u'O'}, {u'Р', u'P'},
                              {u'С', u'C'}, {u'Т', u'T'}, {u'Х', u'X'},
                              {u'а', u'a'}, {u'е', u'e'}, {u'к', u'k'}, {u'о', u'o'},
                              {u'р', u'p'}, {u'с', u'c'}, {u'х', u'x'}, {u'у', u'y'},
                              {u'–', u'-'}
                          };
                          QString normalizied;
                          for (const QChar& ch : text)
                              if (rusToEng.contains(ch)) normalizied.append(rusToEng[ch]);
                              else normalizied.append(ch);
                          return normalizied;
                       };

                      valString = recode(QString(valString));
                  }
                  int colIndex = 1;
                  int findRowIndex = -1;
                  for (int row = 0; row < model->rowCount(); ++row){
                      if (model->data(model->index(row, colIndex)).toString().contains(valString, Qt::CaseInsensitive)){
                          findRowIndex = row;
                          break;
                      }
                  }
                  if (findRowIndex == -1) findRowIndex = 0;
                  QModelIndex index = model->index(findRowIndex, 0);
                  QItemSelectionModel *selectModel = tableView->selectionModel();
                  selectModel->clearSelection();
                  selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                  tableView->setCurrentIndex(index);
                  tableView->scrollTo(index);
                  findDialog->accept();
               });

               findDialog->exec();
           });

           QObject::connect(copyBtn, &QPushButton::clicked, [tableView, model, commandLine](){
               QModelIndex index = tableView->currentIndex();
               if (index.row() < 0 || index.row() >= model->rowCount()) return;

               QString paramString = model->data(model->index(index.row(), 1)).toString();

               commandLine->appendText(paramString);
               paramAPPCPInfo->close();
           });
       }
       paramAPPCPInfo->raise();
       paramAPPCPInfo->activateWindow();
       paramAPPCPInfo->show();
    });
    QObject::connect(setConnectNU, &QAction::triggered, dirRunner, &directRunner::connectNU);
    QObject::connect(disconnectNU, &QAction::triggered, dirRunner, &directRunner::disconnectNU);
    QObject::connect(paramRR, &QAction::triggered, [this, commandLine](){

        static QWidget *rrParWgt;
        if (rrParWgt){
            delete rrParWgt;
            rrParWgt = nullptr;
        }

        if (!rrParWgt){
            rrParWgt = new QWidget();
            QPushButton *topBtn = new QPushButton("t", rrParWgt);
            QPushButton *upBtn = new QPushButton("u", rrParWgt);
            QPushButton *downBtn = new QPushButton("d", rrParWgt);
            QPushButton *bottomBtn = new QPushButton("b", rrParWgt);

            QPushButton *findBtn = new QPushButton("найти", rrParWgt);

            QPushButton *copyBtn = new QPushButton("Копировать", rrParWgt);

            QSqlQueryModel *rrParModel = new QSqlQueryModel(rrParWgt);
            /*SELECT Bl_Name, Par_Name, Index FROM RR_PAR*/
            rrParModel->setQuery("SELECT 'FL.' & Bl_Name & '_' & Par_Name AS Идентификатор, COUNT(Index) AS Длина_массива FROM RR_PAR GROUP BY BL_Name, Par_Name", rrParDB);
            QTableView *rrParView = new QTableView(rrParWgt);
            rrParView->setModel(rrParModel);
            rrParView->setSelectionBehavior(QAbstractItemView::SelectRows);
            rrParView->setSelectionMode(QAbstractItemView::SingleSelection);
            rrParView->setEditTriggers(QAbstractItemView::NoEditTriggers);
            rrParView->setStyleSheet(R"(
                                     QTableView::item::selected{
                                         background-color: #0078D7;
                                         color: white;
                                     }
                                     QTableView::item::selected:!active{
                                         background-color: #0078D7;
                                         color: white;
                                     }
                                     )");
            QSqlQueryModel *rrMasParModel = new QSqlQueryModel(rrParWgt);
            QTableView *rrMasParView = new QTableView(rrParWgt);
            rrMasParView->setModel(rrMasParModel);
            rrMasParView->setSelectionBehavior(QAbstractItemView::SelectRows);
            rrMasParView->setSelectionMode(QAbstractItemView::SingleSelection);
            rrMasParView->setEditTriggers(QAbstractItemView::NoEditTriggers);
            rrMasParView->setStyleSheet(R"(
                                        QTableView::item::selected{
                                            background-color: #0078D7;
                                            color: white;
                                        }
                                        QTableView::item::selected:!active{
                                            background-color: #0078D7;
                                            color: white;
                                        }
                                        )");

            QLabel *titleFilterLable = new QLabel("Фильтр", rrParWgt);
            QComboBox *filterComboBox = new QComboBox(rrParWgt);
            filterComboBox->addItem("Все блоки");

            QSqlQuery query(rrParDB);
            if (!query.exec(QString("SELECT DISTINCT Bl_Name FROM RR_PAR"))){
                qDebug() << "query error";
                return;
            }

            while (query.next()){
                QString blName = query.value(0).toString();
                if (!blName.isEmpty()) filterComboBox->addItem(blName);
            }

            QHBoxLayout *hBox = new QHBoxLayout();
            hBox->addWidget(topBtn);
            hBox->addWidget(upBtn);
            hBox->addWidget(downBtn);
            hBox->addWidget(bottomBtn);

            hBox->addWidget(findBtn);
            hBox->addWidget(copyBtn);

            QVBoxLayout *filterVBox = new QVBoxLayout();
            filterVBox->addWidget(titleFilterLable);
            filterVBox->addWidget(filterComboBox);

            QVBoxLayout *rightVBox = new QVBoxLayout();
            rightVBox->addWidget(rrMasParView);
            rightVBox->addLayout(filterVBox);

            QHBoxLayout *viewHBox = new QHBoxLayout();
            viewHBox->addWidget(rrParView);
            viewHBox->addLayout(rightVBox);

            QVBoxLayout *vBox = new QVBoxLayout();
            vBox->addLayout(hBox);
            vBox->addLayout(viewHBox);

            rrParWgt->setLayout(vBox);

            QObject::connect(topBtn, &QPushButton::clicked, [rrParView, rrParModel](){
               if (rrParModel->rowCount() == 0) return;
               QModelIndex index = rrParModel->index(0, 0);

               QItemSelectionModel *selectModel = rrParView->selectionModel();
               selectModel->clearSelection();
               selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
               rrParView->setCurrentIndex(index);
               rrParView->scrollTo(index);
            });

            QObject::connect(upBtn, &QPushButton::clicked, [rrParView, rrParModel](){
               if (rrParModel->rowCount() == 0) return;
               QModelIndex index = rrParView->currentIndex();
               if (index.row() < 0 || index.row() >= rrParModel->rowCount()) index = rrParModel->index(0, 0);
               int rowIndex = (index.row() - 1 >= 0) ? index.row() - 1 : index.row();
               index = rrParModel->index(rowIndex, 0);

               QItemSelectionModel *selectModel = rrParView->selectionModel();
               selectModel->clearSelection();
               selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
               rrParView->setCurrentIndex(index);
               rrParView->scrollTo(index);
            });

            QObject::connect(downBtn, &QPushButton::clicked, [rrParView, rrParModel](){
                if (rrParModel->rowCount() == 0) return;
                QModelIndex index = rrParView->currentIndex();

                if (index.row() < 0 || index.row() >= rrParModel->rowCount()) index = rrParModel->index(0, 0);
                int rowIndex = (index.row() + 1 < rrParModel->rowCount()) ? index.row() + 1 : index.row();
                index = rrParModel->index(rowIndex, 0);

                QItemSelectionModel *selectModel = rrParView->selectionModel();
                selectModel->clearSelection();
                selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                rrParView->setCurrentIndex(index);
                rrParView->scrollTo(index);
            });

            QObject::connect(bottomBtn, &QPushButton::clicked, [rrParView, rrParModel](){
                if (rrParModel->rowCount() == 0) return;
               int rowIndex = (rrParModel->rowCount() - 1 >= 0) ? rrParModel->rowCount() - 1 : 0;
               QModelIndex index = rrParModel->index(rowIndex, 0);

               QItemSelectionModel *selectModel = rrParView->selectionModel();
               selectModel->clearSelection();
               selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
               rrParView->setCurrentIndex(index);
               rrParView->scrollTo(index);
            });

            QObject::connect(findBtn, &QPushButton::clicked, [rrParView, rrParModel](){
                QDialog *findDialog = new QDialog();
                findDialog->setModal(true);

                QLabel *titleColFind = new QLabel("Поле", findDialog);
                QLabel *titleValFind = new QLabel("Значение", findDialog);

                QComboBox *colsFind = new QComboBox(findDialog);
                colsFind->addItem("Идентификатор");

                QLineEdit *valFind = new QLineEdit(findDialog);

                QCheckBox *enCodeValCheckBox = new QCheckBox(findDialog);
                QLabel *enCodeValLabel = new QLabel("Перекодировка значения", findDialog);

                QPushButton *findVal = new QPushButton("Найти", findDialog);

                QGridLayout *gridLayout = new QGridLayout();
                gridLayout->addWidget(titleColFind, 0, 0);
                gridLayout->addWidget(titleValFind, 0, 1);
                gridLayout->addWidget(colsFind, 1, 0);
                gridLayout->addWidget(valFind, 1, 1);

                QHBoxLayout *hBox = new QHBoxLayout();
                hBox->addWidget(enCodeValCheckBox, 0, Qt::AlignLeft);
                hBox->addWidget(enCodeValLabel, 0, Qt::AlignLeft);

                gridLayout->addLayout(hBox, 2, 0, Qt::AlignLeft);
                gridLayout->addWidget(findVal, 2, 1, Qt::AlignRight);

                findDialog->setLayout(gridLayout);

                QObject::connect(findVal, &QPushButton::clicked, [rrParView, rrParModel, enCodeValCheckBox, valFind, findDialog](){
                   QString valString = valFind->text().trimmed();
                   if (enCodeValCheckBox->isChecked()){
                       auto recode = [&](const QString &text) -> QString{
                           static const QMap<QChar, QChar> rusToEng={
                               {u'А', u'A'}, {u'В', u'B'}, {u'Е', u'E'}, {u'К', u'K'},
                               {u'М', u'M'}, {u'Н', u'H'}, {u'О', u'O'}, {u'Р', u'P'},
                               {u'С', u'C'}, {u'Т', u'T'}, {u'Х', u'X'},
                               {u'а', u'a'}, {u'е', u'e'}, {u'к', u'k'}, {u'о', u'o'},
                               {u'р', u'p'}, {u'с', u'c'}, {u'х', u'x'}, {u'у', u'y'},
                               {u'–', u'-'}
                           };
                           QString normalizied;
                           for (const QChar& ch : text)
                               if (rusToEng.contains(ch)) normalizied.append(rusToEng[ch]);
                               else normalizied.append(ch);
                           return normalizied;
                        };

                       valString = recode(QString(valString));
                   }
                   int colIndex = 0;
                   int findRowIndex = -1;
                   for (int row = 0; row < rrParModel->rowCount(); ++row){
                       if (rrParModel->data(rrParModel->index(row, colIndex)).toString().contains(valString, Qt::CaseInsensitive)){
                           findRowIndex = row;
                           break;
                       }
                   }
                   if (findRowIndex == -1) findRowIndex = 0;
                   QModelIndex index = rrParModel->index(findRowIndex, 0);
                   QItemSelectionModel *selectModel = rrParView->selectionModel();
                   selectModel->clearSelection();
                   selectModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                   rrParView->setCurrentIndex(index);
                   rrParView->scrollTo(index);
                   findDialog->accept();
                });

                findDialog->exec();
            });

            QObject::connect(copyBtn, &QPushButton::clicked, [rrParModel, rrParView, commandLine](){
                QModelIndex index = rrParView->currentIndex();
                if (index.row() < 0 || index.row() >= rrParModel->rowCount()) return;
                index = rrParModel->index(index.row(), 0);

                QString text = rrParModel->data(index).toString();
                commandLine->appendText(text);

                rrParWgt->close();
            });

            QObject::connect(rrParView->selectionModel(), &QItemSelectionModel::currentRowChanged, [this, rrParModel, rrMasParModel](const QModelIndex &curIndex){
                QModelIndex index = rrParModel->index(curIndex.row(), 0);
                if (index.row() < 0 || index.row() >= rrParModel->rowCount()) index = rrParModel->index(0, 0);

                QString param = rrParModel->data(index).toString();
                if (param.isEmpty()) return;

                QStringList paramList = param.split('_');
                paramList[0] = paramList[0].mid(3);
                qDebug() << paramList[0];
                QString queryString = QString("SELECT IIF(ISNULL(Index), 0, '[' & Index & ']') AS Индекс, Val AS Значение FROM RR_PAR WHERE Bl_Name = '%1' AND Par_Name = '%2'").arg(paramList[0]).arg(paramList[1]);
                qDebug() << paramList;
                rrMasParModel->setQuery(queryString, this->rrParDB);
                qDebug() << rrMasParModel->rowCount();
            });

            QObject::connect(filterComboBox, &QComboBox::currentTextChanged, [rrParModel](const QString& text){
                QString queryString;
                if (text == "Все блоки" || text.isEmpty()){
                    queryString = "SELECT 'FL.' & Bl_Name & '_' & Par_Name AS Идентификатор, COUNT(Index) AS Длина_массива FROM RR_PAR GROUP BY BL_Name, Par_Name";
                } else{
                    queryString = QString("SELECT 'FL.' & Bl_Name & '_' & Par_Name AS Идентификатор, COUNT(Index) AS Длина_массива FROM RR_PAR GROUP BY BL_Name, Par_Name HAVING Bl_Name = '%1'").arg(text);
                }
                rrParModel->setQuery(queryString, rrParDB);
            });
        }

        rrParWgt->raise();
        rrParWgt->activateWindow();
        rrParWgt->show();

    });
    QObject::connect(curProtocol, &QAction::triggered, [this](){
        static bool init{false};
        static QWidget *curFullProtWgt = new QWidget(this);
        static QTextEdit *curFullProtText = new QTextEdit(curFullProtWgt);
        static QLineEdit *findLineEdit = new QLineEdit(curFullProtWgt);
        static QPushButton *up = new QPushButton("up", curFullProtWgt);
        static QPushButton *down = new QPushButton("down", curFullProtWgt);
        static QCheckBox *needUpdate = new QCheckBox("Обновлять протокол", curFullProtWgt);
        static QVBoxLayout *vBox = new QVBoxLayout();
        static QHBoxLayout *hBox = new QHBoxLayout();
        static TextSearcher *txtSearch = new TextSearcher(curFullProtText, curFullProtWgt);
        static bool hasNotVisibleUpdate{false};
        static bool hasNotVisbleSavedFile{false};
        if (!init){
            hBox->addWidget(findLineEdit);
            hBox->addWidget(up);
            hBox->addWidget(down);
            hBox->addWidget(needUpdate);
            vBox->addLayout(hBox);
            vBox->addWidget(curFullProtText);
            curFullProtWgt->setLayout(vBox);
            curFullProtWgt->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
            curFullProtWgt->setWindowTitle("Текущий протокол");
            curFullProtText->setReadOnly(true);
            needUpdate->setChecked(true);
            curFullProtText->setText(ProtManager::instance().getAllFileDate());
            QTimer::singleShot(50, curFullProtWgt, [](){
               curFullProtText->verticalScrollBar()->setValue(curFullProtText->verticalScrollBar()->maximum());
            });
            QObject::connect(findLineEdit, &QLineEdit::textChanged, [](const QString& t){
                txtSearch->setSearchTerm(t);
            });
            QObject::connect(down, &QPushButton::clicked, txtSearch, &TextSearcher::findNext);
            QObject::connect(up, &QPushButton::clicked, txtSearch, &TextSearcher::findPrevious);

            QObject::connect(&ProtManager::instance(), &ProtManager::fileUpdate, curFullProtWgt, [](){
                    if (needUpdate->isChecked()){
                        QString newProtInfo = ProtManager::instance().getNewFileDate();
                        if (!newProtInfo.isEmpty()){
                            qDebug() << curFullProtText->verticalScrollBar()->value();
                            qDebug() << curFullProtText->verticalScrollBar()->maximum();
                            bool needScroll = (curFullProtText->verticalScrollBar()->value() == curFullProtText->verticalScrollBar()->maximum());
                            qDebug() << "needScroll: " << needScroll;
                            curFullProtText->append(newProtInfo);
                            if (needScroll){
                                QTimer::singleShot(50, curFullProtWgt, [](){
                                    curFullProtText->verticalScrollBar()->setValue(curFullProtText->verticalScrollBar()->maximum());
                                });
                            }
                        }
                    } else hasNotVisibleUpdate = true;
            }, Qt::QueuedConnection);
            QObject::connect(&ProtManager::instance(), &ProtManager::fileSaved, curFullProtWgt, [](){
                if (needUpdate->isChecked()){
                    QTimer::singleShot(500, curFullProtWgt, [](){
                       curFullProtText->setText(ProtManager::instance().getAllFileDate());;
                    });
                    //QMetaObject::invokeMethod(curFullProtText, "setText", Qt::BlockingQueuedConnection, Q_ARG(QString, ""));
                } else{
                    hasNotVisbleSavedFile = true;
                    hasNotVisibleUpdate = false;
                }
            }, Qt::QueuedConnection);
            QObject::connect(needUpdate, &QCheckBox::toggled, [](bool isChecked){
               if (isChecked && hasNotVisbleSavedFile){
                   //QMetaObject::invokeMethod(curFullProtText, "setText", Qt::BlockingQueuedConnection, Q_ARG(QString, ProtManager::instance().getAllFileDate()));
                   curFullProtText->setText(ProtManager::instance().getAllFileDate());
                   hasNotVisbleSavedFile = false;
                   hasNotVisibleUpdate = false;
               }
               if (isChecked && hasNotVisibleUpdate){
                   QString newProtInfo = ProtManager::instance().getNewFileDate();
                   if (!newProtInfo.isEmpty()) curFullProtText->append(newProtInfo);
                   hasNotVisibleUpdate = false;
               }
            });
            init = true;
        }
        findLineEdit->setText("");
        curFullProtWgt->show();
    });
    QObject::connect(listSaveComand, &QAction::triggered, [commandLine](){
        static QWidget *listSaveCommandWgt;
        static QStringList deleteCommand;

        if (listSaveCommandWgt){
            delete listSaveCommandWgt;
            listSaveCommandWgt = nullptr;
            deleteCommand.clear();
        }
        if (!listSaveCommandWgt){
            listSaveCommandWgt = new QWidget();
            QListWidget *listWgt = new QListWidget(listSaveCommandWgt);

            for (const QString& command : commandLine->getHistory()){
                QListWidgetItem *item = new QListWidgetItem(command);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Unchecked);
                listWgt->insertItem(0, item);
            }

            QObject::connect(commandLine, &CommandLine::historyUpdated, [listWgt, commandLine](){
                listWgt->clear();
                for (const QString& command : commandLine->getHistory()){
                    QListWidgetItem *item = new QListWidgetItem(command);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    item->setCheckState(Qt::Unchecked);
                    listWgt->insertItem(0, item);
                }
            });

            QPushButton *selectAllBtn = new QPushButton("Пометить все", listSaveCommandWgt);
            QPushButton *selectBtn = new QPushButton("Пометить все с 31", listSaveCommandWgt);
            QPushButton *unSelectAllBtn = new QPushButton("Отменить все", listSaveCommandWgt);
            QPushButton *deleteBtn = new QPushButton("Удалить помеч.", listSaveCommandWgt);

            QPushButton *okBtn = new QPushButton("Ок", listSaveCommandWgt);
            QPushButton *cancelBtn = new QPushButton("Отмена", listSaveCommandWgt);

            QVBoxLayout *vBoxBtn = new QVBoxLayout();
            vBoxBtn->addWidget(selectAllBtn);
            vBoxBtn->addWidget(selectBtn);
            vBoxBtn->addWidget(unSelectAllBtn);
            vBoxBtn->addStretch();
            vBoxBtn->addWidget(deleteBtn);

            QHBoxLayout *hBoxTop = new QHBoxLayout();
            hBoxTop->addWidget(listWgt);
            hBoxTop->addLayout(vBoxBtn);

            QHBoxLayout *hBoxBtn = new QHBoxLayout();
            hBoxBtn->addStretch();
            hBoxBtn->addWidget(okBtn);
            hBoxBtn->addWidget(cancelBtn);

            QVBoxLayout *vBox = new QVBoxLayout();
            vBox->addLayout(hBoxTop);
            vBox->addLayout(hBoxBtn);

            listSaveCommandWgt->setLayout(vBox);

            QObject::connect(selectAllBtn, &QPushButton::clicked, [listWgt](){
               for (int row = 0; row < listWgt->count(); ++row){
                   listWgt->item(row)->setCheckState(Qt::Checked);
               }
            });

            QObject::connect(selectBtn, &QPushButton::clicked, [listWgt](){
               for (int row = 31; row < listWgt->count(); ++row){
                   listWgt->item(row)->setCheckState(Qt::Checked);
               }
            });

            QObject::connect(unSelectAllBtn, &QPushButton::clicked, [listWgt](){
               for (int row = 0; row < listWgt->count(); ++row){
                   listWgt->item(row)->setCheckState(Qt::Unchecked);
               }
            });

            QObject::connect(deleteBtn, &QPushButton::clicked, [listWgt](){
               for (int row = 0; row < listWgt->count(); ++row){
                   if (listWgt->item(row)->checkState() == Qt::Checked){
                       delete listWgt->item(row);
                       row -= 1;
                   }
               }
            });

            QObject::connect(cancelBtn, &QPushButton::clicked, [](){
               listSaveCommandWgt->close();
            });

            QObject::connect(okBtn, &QPushButton::clicked, [listWgt, commandLine](){
                if (commandLine->getHistory().count() != listWgt->count() || true){
                    QStringList newHistory;
                    for (int row = 0; row < listWgt->count(); ++row){
                        newHistory.append(listWgt->item(row)->text());
                    }
                    commandLine->updateHistory(newHistory);
                }

                listSaveCommandWgt->close();
            });
        }

        listSaveCommandWgt->raise();
        listSaveCommandWgt->activateWindow();
        listSaveCommandWgt->show();
    });

    QObject::connect(catalogSelect, &QAction::triggered, [this](){
       QString catalog = QFileDialog::getExistingDirectory(this, "Выберите каталог", QDir::homePath(), QFileDialog::ShowDirsOnly);
       if (!catalog.isEmpty() && QDir(catalog).exists()){
           this->curCatalog = catalog;
           emit this->printMessageToProtocol(QString("Каталог: %1").arg(catalog), "0");
       }
    });

    /*QObject::connect(dipFileSelect, &QAction::triggered, [protocolText, protocolWgt, programInfomodel](){
       QString filePath = QFileDialog::getOpenFileName(nullptr, "Выберите циклограмму", "", "*.dip");
       DirectParser dirParser;
       directRunner dirRunner = directRunner::instance();
       QList<DirectParser::Direct*> directives = dirParser.parseFile(filePath);
       QMap<QString, int> metki;
       for (int numDirect = 0; numDirect < directives.count(); ++numDirect){
           if (directives[numDirect]->metka.isEmpty()) continue;
           metki.insert(directives[numDirect]->metka, numDirect);
       }*/
       /*for (const DirectParser::Direct *directive : directives){
           dirRunner.runDirect(*directive, protocolText, protocolWgt);
       }*/
       /*for (int numDirecct = 0; numDirecct < directives.count(); ++numDirecct){
           dirRunner.runDirect(*directives[numDirecct], protocolText, protocolWgt, programInfomodel);
           if (!dirRunner.getMetka().isEmpty()){
               if (!metki.contains(dirRunner.getMetka())){
                   qDebug() << "ERROR METKA NOT FOUND";
                   return;
               }
               numDirecct = metki.value(dirRunner.getMetka()) - 1;
               dirRunner.resetMetka();
           }
       }
    });*/
    //directRunner dirRunner;
    QObject::connect(commandLine, &CommandLine::commandSet, this, [this/*, &dirRunner*/](const QString& command){
       //directRunner dirRunner = directRunner::instance();
       DirectParser dirPareser;
       QList<DirectParser::Direct*> directives = dirPareser.parseKO(command);
       if (directives.isEmpty()) return;
       //auto future = QtConcurrent::run(this->dirRunner, &directRunner::runDirect, *directives.at(0)/*, protocolText, protocolWgt,programInfomodel*/);
       emit this->runDirectives(*directives.at(0));


       //future.waitForFinished();
       //dirRunner.runDirect(*directives.at(0), protocolText, protocolWgt, programInfomodel);
    }, Qt::QueuedConnection);

    QObject::connect(exitAction, &QAction::triggered, [this](){close();});

    QObject::connect(SrStop, &QAction::triggered, this, [this](){
        emit this->printMessageToProtocol("ЗАПРОШЕН ОСТАНОВ ПО СРОСТ", "23");
        this->dirRunner->ost_flag.store(1);
    }, Qt::QueuedConnection);
    QObject::connect(srostM, &QAction::triggered, this, [this](){
        emit this->printMessageToProtocol("ЗАПРОШЕН ОСТАНОВ ПО СРОСТ_М", "23");
        this->dirRunner->m_ost_flag.store(1);
    }, Qt::QueuedConnection);
    QObject::connect(start, &QAction::triggered, this, [this]{
        DirectParser dirParser;
        QList<DirectParser::Direct*> directives = dirParser.parseKO("ПУСК");
        //auto future = QtConcurrent::run(this->dirRunner, &directRunner::runDirect, *directives.at(0));
        emit this->runDirectives(*directives.at(0));
    }, Qt::QueuedConnection);
    QObject::connect(this, &MainWindow::runDirectives, dirRunner, &directRunner::runDirect);
    dirRunnerThread = new QThread;
    dirRunner->moveToThread(dirRunnerThread);
    QObject::connect(dirRunnerThread, &QThread::started, dirRunner, &directRunner::startWork);
    QObject::connect(dirRunnerThread, &QThread::finished, dirRunner, &QObject::deleteLater);
    dirRunnerThread->start();

    QObject::connect(manual, &ManualMode::podkl, dirRunner, &directRunner::runCommandNU);
    QObject::connect(dirRunner, &directRunner::manualCommandComplete, manual, &ManualMode::commandComplete);
    QObject::connect(dirRunner, &directRunner::printMessageToManualWindow, manual, &ManualMode::printMessage);
    QObject::connect(dirRunner, &directRunner::sendResulToManualWindow, manual, &ManualMode::setResult);

    QObject::connect(manual, &ManualMode::widgetClosed, dirRunner, &directRunner::unSetManualMode);
    QObject::connect(manual, &ManualMode::widgetShown, dirRunner, &directRunner::setManualMode);

    QObject::connect(this, &MainWindow::printMessageToProtocol, dirRunner, &directRunner::printInProt);


    QObject::connect(callAction, &QAction::triggered, [commandLine](){
       commandLine->setText("ВЫЗВАТЬ ");
    });

    QObject::connect(toDirectAction, &QAction::triggered, [commandLine](){
       commandLine->setText("НА ");
    });

    QObject::connect(exitProgramAction, &QAction::triggered, [commandLine](){
        commandLine->setText("ВЫХОД ");
    });

    QObject::connect(rExitProgramAction, &QAction::triggered, [commandLine](){
        commandLine->setText("РВЫХОД ");
    });

    QObject::connect(repeatAction, &QAction::triggered, [commandLine](){
        commandLine->setText("ПОВТОР ");
    });

    QObject::connect(fileAction, &QAction::triggered, [this](){
        if (paramValues.contains("ПП")){
            QString pfPath = paramValues.value("ПП");

            QString fileName = QFileDialog::getOpenFileName(this, "Выберите файл", "", "Файлы циклограмм (*.dip);;Файлы настройки (*.set);;Файлы структуры (*.dii)");

            QProcess *pfProcess = new QProcess(this);
            pfProcess->start(QFileInfo(pfPath).filePath(), {fileName});
        } else{
            QMessageBox::critical(nullptr, "Ошибка!", "Путь к PF не определен в файле конфигурации");
        }
    });

    QObject::connect(savedProtocol, &QAction::triggered, [this](){
        if (paramValues.contains("ППИ")){
            QString protViewPath = paramValues.value("ППИ");

            QString fileName = QFileDialog::getOpenFileName(this, "Выберите файл протокола", "", "*.pcp");

            QProcess *protViewProcess = new QProcess(this);
            protViewProcess->start(QFileInfo(protViewPath).filePath(), {fileName});
        } else{
            QMessageBox::critical(nullptr, "Ошибка!", "Путь к ProtView не определен в файле конфигурации");
        }
    });

    QObject::connect(dipFileSelect, &QAction::triggered, [this, commandLine](){
        QString filePath = QFileDialog::getOpenFileName(this, "Выберите файл циклограммы", MainWindow::getCurCatalog(), "*.dip");
        if (filePath.isEmpty() || QFileInfo(filePath).suffix().toUpper() != "DIP"){
            return;
        } else{
            commandLine->appendText(QFileInfo(filePath).fileName());
        }
    });

    QObject::connect(fileSelect, &QAction::triggered, [this, commandLine](){
        QString filePath = QFileDialog::getOpenFileName(this, "Выберите файл циклограммы", MainWindow::getCurCatalog());
        if (filePath.isEmpty()){
            return;
        } else{
            commandLine->appendText(QFileInfo(filePath).fileName());
        }
    });

    QObject::connect(this, &MainWindow::sendTimeWorkAppcp, dirRunner, &directRunner::setTimeWorkAppcp);

    QObject::connect(dirRunner, &directRunner::requestTimeWorkAppcp, this, [this](){
        QStringList timeWork = getTimeWorkAppcp();
        emit sendTimeWorkAppcp(timeWork);
    }, Qt::QueuedConnection);

    if (!paramValues.contains("СПРАВКА") || QFileInfo(paramValues.value("СПРАВКА")).suffix().toUpper() != "QHC" || !QFile(paramValues.value("СПРАВКА")).exists()){
        QMessageBox::critical(nullptr, "Ошибка!", "Не удалось загрузить файл справки!\nСправка не будет доступна в приложении");
    } else{
        qDebug() << paramValues.value("СПРАВКА");
        QHelpEngine *helpEngine = new QHelpEngine(paramValues.value("СПРАВКА"), this);
        if (!helpEngine->setupData()) {
            QMessageBox::critical(nullptr, "Ошибка!", "Не удалось загрузить файл справки!\nСправка не будет доступна в приложении");
        } else{
            QObject::connect(referense, &QAction::triggered, this, [this, helpEngine](){
                static QSplitter *splitter;
                static bool init{false};
                if (!init){
                    QHelpContentWidget *contentWidget = helpEngine->contentWidget();

                    // Виджет индекса (поиск по ключевым словам)
                    QHelpIndexWidget *indexWidget = helpEngine->indexWidget();

                    // Браузер для отображения страниц справки
                    // Браузер для отображения страниц справки
                    HelpBrowser *textBrowser = new HelpBrowser(helpEngine);

                    // Используем лямбду, чтобы сигналы и слоты совпадали
                    QObject::connect(contentWidget, &QHelpContentWidget::linkActivated,
                                     [=](const QUrl &url){
                                         QByteArray html = helpEngine->fileData(url);
                                         textBrowser->setSource(url);
                                     });

                    QObject::connect(indexWidget, &QHelpIndexWidget::linkActivated,
                                     [=](const QUrl &url){
                                         QByteArray html = helpEngine->fileData(url);
                                         textBrowser->setSource(url);
                                     });

                    QObject::connect(textBrowser, &QTextBrowser::anchorClicked, [=](const QUrl& url){
                        QByteArray html = helpEngine->fileData(url);
                        textBrowser->setHtml(QString::fromUtf8(html));
                    });

                    // Разделитель для панели содержания и текста
                    splitter = new QSplitter();
                    splitter->addWidget(contentWidget);
                    splitter->addWidget(textBrowser);
                    splitter->setStretchFactor(1, 1);
                    init = true;
                }

                splitter->resize(800, 600);
                splitter->setWindowTitle("APPCP HELP");
                splitter->show();
            });
        }
    }

    QString TimeControlPath;
    if (paramValues.contains("КОНТР_ВРЕМЕНИ")){
        TimeControlPath = paramValues.value("КОНТР_ВРЕМЕНИ");

        if (!QSharedMemory(TIME_CONTROL_MEMORY).attach()){
            QProcess *timeControlProcess = new QProcess(this);
            qDebug() << "START";
            timeControlProcess->start(QFileInfo(TimeControlPath).filePath());
        }
        timeControlSocket = new QLocalSocket(this);
        QTimer::singleShot(5000, [this](){
            if (!QSharedMemory(TIME_CONTROL_MEMORY).attach()){
                emit printMessageToProtocol("НЕ УДАЛОСЬ ЗАПУСТИТЬ СЕРВЕР УЧЕТА ВРЕМЕНИ АППЦП. УЧЕТ ВРЕМЕНИ РАБОТЫ НЕДОПУСТЕН!", "13");
                return;
            }
            //timeControlSocket->disconnectFromServer();
            timeControlSocket->connectToServer(LOCAL_SERVER_TIME_CONTROL);
            QTimer::singleShot(5000, [this](){
               if (timeControlSocket->state() != QLocalSocket::ConnectedState){
                   emit printMessageToProtocol("НЕ УДАЛОСЬ ПОДКЛЮЧИТЬСЯ К СЕРВЕРУ УЧЕТА ВРЕМЕНИ АППЦП. УЧЕТ ВРЕМЕНИ РАБОТЫ НЕДОПУСТЕН!", "13");
                   return;
               }
               emit printMessageToProtocol("УЧЕТ ВРЕМЕНИ РАБОТЫ АППЦП АКТИВЕН!", "23");
            });
        });
    } else{
        emit printMessageToProtocol("НЕТ ПУТИ К СЕРВЕРУ УЧЕТА ВРЕМЕНИ АППЦП (.exe файл). УЧЕТ ВРЕМЕНИ РАБОТЫ НЕДОСТУПЕН!", "13");
    }
}

bool MainWindow::readConfigFile(const QString& filePath, QMap<QString, QString>& paramMap){
    QFile configFile(filePath);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)){
        return false;
    }

    QTextStream in(&configFile);

    while (!in.atEnd()){
        QString line = in.readLine();

        if (!line.contains(QChar('='))) continue;

        QStringList param = line.split("=");
        if (paramMap.contains(param[0])){
            return false;
        }
        if (param[1].contains("//")){
            param[1] = param[1].split("//")[0].trimmed();
        }
        paramMap.insert(param[0], param[1]);
    }
    return true;
}

MainWindow::~MainWindow()
{
    if (stepWgt != nullptr && this->statusOpenned) delete this->stepWgt;
    //if (dirRunner) delete dirRunner;
    dirRunnerThread->quit();
}

QSqlQuery MainWindow::getQueryRRDB(const QString &queryString){
    //QSqlQuery *query = new QSqlQuery(rrParDB);
    QSqlQuery query(rrParDB);
    query.exec(queryString);
    return query;
}

void MainWindow::showVariantDialogWindow(const QString& text, const QStringList& variable){
    //QDialog *varTK = new QDialog();
    //varTK->setModal(true);
    //varTK->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    //this->programDirectWindowClose = false;
    //QWidget *varTK = new QWidget();
    DialogWgt *varTK = new DialogWgt(true);
    varTK->setWindowFlags(Qt::Widget | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);
    varTK->setWindowTitle("Директива ВАРИАНТК");

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

    varTK->setLayout(vBox);

    textEdit->setText(text);

    for (const auto& var : variable){
        listWgt->addItem(var);
    }

    listWgt->selectionModel()->clearSelection();
    listWgt->setCurrentRow(-1);

    QObject::connect(acceptStop, &QPushButton::clicked, [this, varTK, listWgt](){
        if (listWgt->currentItem() == nullptr || listWgt->currentRow() == -1) return;
        QString GL_VAR = listWgt->currentItem()->text();
        //varTK->accept();
        this->programDirectWindowClose = true;
        varTK->setProgramCloseFlag(true);
        varTK->close();

        //delete varTK;

        emit this->variantSelected(GL_VAR, true);
    });

    QObject::connect(accept, &QPushButton::clicked, [this, varTK, listWgt](){
        if (listWgt->currentItem() == nullptr || listWgt->currentRow() == -1) return;
        QString GL_VAR = listWgt->currentItem()->text();
        //varTK->accept();
        varTK->setProgramCloseFlag(true);
        varTK->close();

        delete varTK;

        emit this->variantSelected(GL_VAR, false);
    });


    QObject::connect(varTK, &DialogWgt::userCloseWindow, [this, varTK](){
       QString GL_VAR = "";
       //varTK->setProgramCloseFlag(true);
       //varTK->close();

       emit this->variantSelected(GL_VAR, false);
    });

    //varTK->exec();
    varTK->setMinimumSize(400, 400);
    varTK->show();

}

void MainWindow::appendMessageToProtocol(const QString& message){
    protocolText->append(message);

    //emit this->protChanged();
    emit this->protocolMessageSet();
}

void MainWindow::set100VStyleForProtocol(){
    protocolText->setStyleSheet("background-color: lightblue;");
    this->protocolWgt->setObjectName("protocolWGT");
    this->protocolWgt->setStyleSheet("#protocolWGT {border: 2px solid red;}");

    emit this->protocolSetStyleState();
}

void MainWindow::unset100VStyleForProtocol(){
    this->protocolText->setStyleSheet("");
    this->protocolWgt->setStyleSheet("");

    emit this->protocolSetStyleState();
}

void MainWindow::showDirectWindow(const QString& textDirect){
    directRunner::DIRECT_VARIABLE dirVar = directRunner::DIRECT_VARIABLE::EMTY_VAR;

    //QDialog *directDiaog = new QDialog();
    //directDiaog->setModal(true);
    //directDiaog->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    //QWidget *directDiaog = new QWidget();
    DialogWgt *directDiaog = new DialogWgt();
    directDiaog->setWindowFlags(Qt::Widget | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowStaysOnTopHint);
    directDiaog->setWindowTitle("Директива оператору");
    QTextEdit *textEdit = new QTextEdit(directDiaog);
    textEdit->setReadOnly(true);
    textEdit->setText(textDirect);

    QRadioButton *okAndGo = new QRadioButton("ШТАТ (с продолжением)", directDiaog);
    okAndGo->setChecked(true);
    QRadioButton *nOkAndGo = new QRadioButton("НЕШТАТ (с продолжением)", directDiaog);
    QRadioButton *okAdnStop = new QRadioButton("ШТАТ (с остановом)", directDiaog);
    QRadioButton *nOkANdStop = new QRadioButton("НЕШТАТ (с остановом)", directDiaog);

    QButtonGroup *rButtonGroup = new QButtonGroup(directDiaog);
    rButtonGroup->addButton(okAndGo);
    rButtonGroup->addButton(nOkAndGo);
    rButtonGroup->addButton(nOkANdStop);
    rButtonGroup->addButton(okAdnStop);

    QPushButton *priemBtn = new QPushButton("Прием директивы");
    QPushButton *sendBtn = new QPushButton("Исполнение директивы");
    sendBtn->setEnabled(false);

    QVBoxLayout *rVBoxGO = new QVBoxLayout();
    rVBoxGO->addWidget(okAndGo);
    rVBoxGO->addWidget(nOkAndGo);

    QVBoxLayout *rVBoxStop = new QVBoxLayout();
    rVBoxStop->addWidget(okAdnStop);
    rVBoxStop->addWidget(nOkANdStop);

    QHBoxLayout *rHBoxLayout = new QHBoxLayout();
    rHBoxLayout->addLayout(rVBoxGO);
    rHBoxLayout->addLayout(rVBoxStop);

    QHBoxLayout *bHBox = new QHBoxLayout();
    bHBox->addWidget(priemBtn);
    bHBox->addWidget(sendBtn);

    QVBoxLayout *vBox = new QVBoxLayout();
    vBox->addWidget(textEdit);
    vBox->addLayout(rHBoxLayout);
    vBox->addLayout(bHBox);

    directDiaog->setLayout(vBox);

    QObject::connect(priemBtn, &QPushButton::clicked, [sendBtn, priemBtn, this](){
        QString pMsg = ("\t\t" + QDateTime::currentDateTime().toString("HH:mm:ss.zzz") + "\t" + "ОПЕРАТОР ПОДТВЕРДИЛ ДИРЕКТИВУ");
        //pMsg.prepend("<span style='color: blue; white-space: pre;'>");
        //pMsg.append("</span>");
        //this->protocolText->append(pMsg);
        priemBtn->setEnabled(false);
        sendBtn->setEnabled(true);
        //this->dirRunner->printInProt(pMsg, "18", directRunner::textStyle());
        emit this->printMessageToProtocol(pMsg, "18");
    });

    QObject::connect(sendBtn, &QPushButton::clicked, [directDiaog, okAndGo, nOkAndGo, okAdnStop, nOkANdStop, this, &dirVar](){
        QString pMsg = ("\t\t" + QDateTime::currentDateTime().toString("HH:mm:ss.zzz") + "\t");
        if (okAndGo->isChecked()){
            pMsg.append("\tОТВЕТ ОПЕРАТОРА - ШТАТНО");
            dirVar = directRunner::DIRECT_VARIABLE::OK_GO;
            //this->GL_NORM_STATUS = true;
            //this->GL_MODE_NEXT = true;
        } else if (nOkANdStop->isChecked()){
            pMsg.append("\tОТВЕТ ОПЕРАТОРА - НЕШТАТНО С ОСТАНОВОМ");
            //pMsg.prepend("<span style='color: red; white-space: pre;'>");
            //pMsg.append("</span>");
            dirVar = directRunner::DIRECT_VARIABLE::NOT_OK_STOP;
            //this->GL_NORM_STATUS = false;
            //this->GL_MODE_NEXT = false;
        } else if (nOkAndGo->isChecked()){
            pMsg.append("\tОТВЕТ ОПЕРАТОРА - НЕШТАТНО");
            //pMsg.prepend("<span style='color: red; white-space: pre;'>");
            //pMsg.append("</span>");
            dirVar = directRunner::DIRECT_VARIABLE::NOT_OK_GO;
            //this->GL_NORM_STATUS = false;
            //this->GL_MODE_NEXT = true;
        } else if (okAdnStop->isChecked()){
            pMsg.append("\tОТВЕТ ОПЕРАТОРА - ШТАТНО С ОСТАНОВОМ");
            dirVar = directRunner::DIRECT_VARIABLE::OK_STOP;
            //this->GL_NORM_STATUS = true;
            //this->GL_MODE_NEXT = false;
        } else{
            pMsg.append("\tОШИБКА ПОЛУЧЕНИЯ ОТВЕТА ОПЕРАТОРА");
            //pMsg.prepend("<span style='color: red; white-space: pre;'>");
            //pMsg.append("</span>");
            dirVar = directRunner::DIRECT_VARIABLE::EMTY_VAR;
            return;
        }
        //this->protocolText->append(pMsg);
        directDiaog->setProgramCloseFlag(true);
        directDiaog->close();
        if (dirVar == directRunner::DIRECT_VARIABLE::OK_GO || dirVar == directRunner::DIRECT_VARIABLE::OK_STOP){
            //this->dirRunner->printInProt(pMsg, "0", directRunner::textStyle());
            emit this->printMessageToProtocol(pMsg, "0");
        } else{
            //this->dirRunner->printInProt(pMsg, "13", directRunner::textStyle());
            emit this->printMessageToProtocol(pMsg, "13");
        }
        emit this->directVariantSelected(dirVar);
        //delete directDiaog;
    });

    //directDiaog->setMinimumSize(400, 600);
    directDiaog->show();
}

void MainWindow::addProgramToModel(const QString& prName, const QStringList& prText){
    QList<QStandardItem*> row;
    row << new QStandardItem(QString::number(programInfomodel->rowCount() + 1));
    row << new QStandardItem(QString(prName));
    programInfomodel->appendRow(row);
    this->stepWgt->setProgram(prText);

    emit this->programModelActionAccept();
}

void MainWindow::setNumDirInModel(const int numDir, const int numLine){
    if (programInfomodel->rowCount() >= 1) programInfomodel->setData(programInfomodel->index(programInfomodel->rowCount() - 1, 2), QString::number(numDir));
    this->stepWgt->setNumLine(numLine);

    emit this->programModelActionAccept();
}

void MainWindow::delProgramInModel(){
    if (programInfomodel->rowCount() >= 1) programInfomodel->removeRow(programInfomodel->rowCount() - 1);
    this->stepWgt->removeProgram();

    emit this->programModelActionAccept();
}

void MainWindow::showInfoStopWindow(const QString& infoMessage){
    QWidget *infoStopWindow = new QWidget(this);
    infoStopWindow->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint);

    infoStopWindow->setWindowTitle("Останов");
    QLabel *label = new QLabel(infoStopWindow);
    label->setText(infoMessage);

    QHBoxLayout *hBox = new QHBoxLayout();
    hBox->addWidget(label);
    infoStopWindow->setLayout(hBox);

    QObject::connect(this, &MainWindow::closeInfoStopWindow, infoStopWindow, &QWidget::close);

    infoStopWindow->show();
}

void MainWindow::closeEvent(QCloseEvent *event){
    if (programInfomodel->rowCount() >= 1){
        QMessageBox::critical(nullptr, "ПРИС", "НЕЛЬЗЯ ЗАКОНЧИТЬ СЕАНС ПОКА ЕСТЬ НЕЗАВЕРШЕННЫЕ ЦГ");
        event->ignore();
    } else{
        QMessageBox msgBox;
        msgBox.setWindowTitle("АППЦП");
        msgBox.setText("ЗАВЕРШИТЬ СЕАНС?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setButtonText(QMessageBox::Yes, "Да");
        msgBox.setButtonText(QMessageBox::No, "Нет");
        msgBox.setDefaultButton(QMessageBox::No);

        int result = msgBox.exec();
        if (result == QMessageBox::No){
            event->ignore();
        } else{
            dirRunner->printInProt(QString("\t\t%1\tКонец сенаса").arg(QDateTime::currentDateTime().toString("HH:mm:ss")), "0");
            QStringList timeWork = getTimeWorkAppcp();
            if (timeWork.isEmpty()){
                dirRunner->printInProt(QString("\t\t\tНе удалось получить время работы АППЦП"), "13", directRunner::textStyle());
            } else{
                dirRunner->printInProt(QString("\t\t\tВремя работы АППЦП:\n"
                                               "\t\t\tЗа текущий день: %1\n"
                                               "\t\t\tЗа текущий месяц: %2\n").arg(timeWork[0]).arg(timeWork[1]), "0", directRunner::textStyle());
            }
            QApplication::closeAllWindows();
            event->accept();
        }
    }
}
