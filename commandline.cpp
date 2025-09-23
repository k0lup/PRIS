#include "commandline.h"
#include "mainwindow.h"

CommandLine::CommandLine(QWidget *parent) : QWidget(parent)
{
    isHasCancelAction = false;
    indexLastCommand = 0;
    qApp->installEventFilter(this);
    lineEdit = new QLineEdit(this);

    historyBtn = new QToolButton(this);
    historyBtn->setText("H");
    historyBtn->setCursor(Qt::ArrowCursor);
    historyBtn->setStyleSheet("QToolButton {border: none;}");

    QHBoxLayout *hBox = new QHBoxLayout();
    hBox->setContentsMargins(0, 0, 0, 0);
    hBox->addWidget(lineEdit);
    hBox->addWidget(historyBtn);
    hBox->setSpacing(0);
    setLayout(hBox);

    /*menu = new QMenu(this);

    QObject::connect(historyBtn, &QToolButton::clicked, [this](){
        menu->exec(mapToGlobal(QPoint(width() - menu->sizeHint().width(), height())));
    });

    QObject::connect(lineEdit, &QLineEdit::returnPressed, [this](){
        QString command = lineEdit->text();
        if (command.isEmpty()) return;
        histrory.prepend(command);

        menu->clear();
        for (const auto& comStr : this->histrory){
            QAction *act = menu->addAction(comStr);
            QObject::connect(act, &QAction::triggered, [this, act](){
                this->lineEdit->setText(act->text());
            });
        }
        lineEdit->clear();
    });*/

    list = new QListWidget(this);
    list->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    list->hide();
    list->setFixedHeight(20);
    list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    //list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    /*QVBoxLayout *vBox = new QVBoxLayout();
    vBox->addWidget(list);
    vBox->addLayout(hBox);
    vBox->setContentsMargins(0, 0, 0, 0);
    setLayout(vBox);*/
    loadLastCommand();

    QObject::connect(lineEdit, &QLineEdit::textChanged, [this](const QString& text){
        if (text == "СП "){
            QDir dir(MainWindow::getCurCatalog());
            QString newText;
            if (dir.exists()) newText = text + dir.dirName();
            else newText = text;
            QString numProd = MainWindow::getNumProduct();
            if (!numProd.isEmpty()) newText.append("_" + numProd);
            /*lineEdit->*/setText(newText);
        }
        if (text.left(this->currentTextForCancelAction.length()) != currentTextForCancelAction){
            lastTextForCancelAction = currentTextForCancelAction;
            isHasCancelAction = true;
        }
        currentTextForCancelAction = text;
    });

    QObject::connect(lineEdit, &QLineEdit::returnPressed, [this](){
        QString command = this->lineEdit->text();
        lineEdit->clear();
        if (command.isEmpty()) return;

        if (this->histrory.length() < 60){
            QListWidgetItem *item = new QListWidgetItem(command);
            item->setSizeHint(QSize(list->width(), 20));
            //this->list->insertItem(0, item);
            this->list->addItem(item);
            this->histrory.append(command);

            if (list->count() <= 6){
                list->setFixedHeight(22 * list->count());
                updateListPos();
            }
            list->scrollToBottom();
            emit historyUpdated();
        } else{
            QStringList newHistory = this->histrory;
            newHistory.append(command);
            this->updateHistory(newHistory);
        }
        if (histrory.length() > 0) indexLastCommand = histrory.length();
        else indexLastCommand = 0;
        emit commandSet(command);
    });

    QObject::connect(historyBtn, &QToolButton::clicked, [this](){
        if (list->isVisible()){
            //list->setVisible(false);
            list->hide();
        } else /*list->setVisible(true);*/ list->show();
    });

    QObject::connect(list, &QListWidget::itemClicked, [this](QListWidgetItem *item){
       /*lineEdit->*/setText(item->text());
       //list->setVisible(false);
       list->hide();
    });

    updateHistory(this->histrory);
}

void CommandLine::resizeEvent(QResizeEvent *event){
    QWidget::resizeEvent(event);
    updateListPos();
}

void CommandLine::moveEvent(QMoveEvent *event){
    //qDebug() << "move";
    QWidget::moveEvent(event);
    updateListPos();
}

void CommandLine::updateListPos(){
    QPoint point = mapToGlobal(QPoint(0, 0));
    point-=QPoint(0, list->height());
    //point+=QPoint(0, lineEdit->height());
    list->move(point);
    list->setFixedWidth(width());
}

bool CommandLine::eventFilter(QObject *obj, QEvent *event){
    if (event->type() == QEvent::MouseButtonPress){
        //qDebug() << "m click";
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        //qDebug() << "histB: " << historyBtn->geometry();
        QRect hBRect = QRect(historyBtn->mapToGlobal(QPoint(0, 0)), historyBtn->size());
        //qDebug() << "hBRECT: " << hBRect;
        //qDebug() << "event: " << mouseEvent->globalPos();
        //qDebug() << "cont: " << historyBtn->geometry().contains(mouseEvent->globalPos());
        if (list->isVisible() && !hBRect.contains(mouseEvent->globalPos()) && !list->geometry().contains(mouseEvent->globalPos())){
            //qDebug() << "m click hide";
            list->hide();
        }
    }
    if (event->type() == QEvent::Move){
        //qDebug() << "move";
        updateListPos();
    }
    if (event->type() == QEvent::KeyPress){
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->modifiers() == Qt::CTRL && keyEvent->key() == Qt::Key_Up){
            QString result = getPrevCommand();
            if (!result.isEmpty()) /*lineEdit->*/setText(result);
            return true;
        } else if (keyEvent->modifiers() == Qt::CTRL && keyEvent->key() == Qt::Key_Down){
            QString result = getNextCommand();
            if (!result.isEmpty()) /*lineEdit->*/setText(result);
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void CommandLine::updateHistory(const QStringList& newHistory){
    this->histrory = newHistory;
    while (this->histrory.length() > 60){
        this->histrory.removeFirst();
    }
    this->list->clear();
    for (const auto& command: histrory){
        QListWidgetItem *item = new QListWidgetItem(command);
        item->setSizeHint(QSize(list->width(), 20));
        //this->list->insertItem(0, item);
        this->list->addItem(item);
        //this->histrory.append(command);

        if (list->count() <= 6){
            list->setFixedHeight(22 * list->count());
            updateListPos();
        }
    }

    list->scrollToBottom();
    if (histrory.length() > 0) indexLastCommand = histrory.length();
    else indexLastCommand = 0;
    emit historyUpdated();
}

void CommandLine::appendText(const QString& text){
    QString fullText = lineEdit->text();
    if (fullText.isEmpty()) fullText.append(text + " ");
    else fullText.append(QString(" ") + text);
    lineEdit->setText(fullText);
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard){
        clipboard->setText(text);
    }
    return;
}

void CommandLine::setText(const QString& text){
    lineEdit->setText(text);
}

QString CommandLine::getPrevCommand(){
    indexLastCommand -= 1;
    if (histrory.isEmpty()) return QString();
    if (indexLastCommand < 0) indexLastCommand = 0;
    if (indexLastCommand > histrory.length() - 1) indexLastCommand = histrory.length() - 1;
    QString result = histrory.at(indexLastCommand);
    //if (indexLastCommand >= 1) indexLastCommand -= 1;

    return result;
}

QString CommandLine::getNextCommand(){
    indexLastCommand += 1;
    if (histrory.isEmpty()) return QString();
    if (indexLastCommand < 0) indexLastCommand = 0;
    if (indexLastCommand > histrory.length() - 1) indexLastCommand = histrory.length() - 1;
    QString result = histrory.at(indexLastCommand);
    //if (indexLastCommand < histrory.length() - 1) indexLastCommand += 1;

    return result;
}

void CommandLine::clearText(){
    setText("");
}

void CommandLine::saveLastCommand(){
    settings.beginGroup("LastCommands");

    QStringList trimmedCommands = histrory.mid(qMax(0, histrory.length() - 60));
    settings.setValue("commands", trimmedCommands);

    settings.endGroup();
}

void CommandLine::loadLastCommand(){
    settings.beginGroup("LastCommands");

    this->histrory = settings.value("commands").toStringList();
    indexLastCommand = histrory.length();

    settings.endGroup();
}

CommandLine::~CommandLine(){
    saveLastCommand();
}

bool CommandLine::hasCancelAction(){
    return isHasCancelAction;
}

bool CommandLine::hasSelectedText(){
    return lineEdit->hasSelectedText();
}

void CommandLine::cancelLastAction(){
    setText(lastTextForCancelAction);
}


void CommandLine::selectedAll(){
    lineEdit->selectAll();
}

QLineEdit* CommandLine::getCommandLineEdit(){
    return this->lineEdit;
}
