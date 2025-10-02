#include "protmanager.h"
#include <cstring>
#include "mainwindow.h"
#include "directrunner.h"
#include "constvalues.h"
const int protV = 1;
const QByteArray beginStr = QByteArray().append(char(0xFF)).append(char(0xFF)).append(char(0xFF)).append(char(0xFF)).append(char(0x00)).append(char(0x00)).append(0x20).append(0x20);
QTextCodec *codec = QTextCodec::codecForName("Windows-1251");

ProtManager::ProtManager(QObject *parent) : QObject(parent), activeProt(false), lastPosFileSeek(0)
{

}

bool ProtManager::createProtocol(){
    qDebug() << "createProtocol";
    QString tempDir = /*QStandardPaths::writableLocation(QStandardPaths::TempLocation);*/ MainWindow::getOnParam("ПРОТОКОЛ");
    QString filePath = QDir(tempDir).filePath("prot.PCP");
    writeFile.setFileName(filePath);
    readFile.setFileName(filePath);
    QString tempDirNU = QDir(MainWindow::getOnParam("ПРОТОКОЛ")).filePath("НУ");
    if (!QDir(tempDirNU).exists()){
        bool status = QDir().mkpath(tempDirNU);
        if (!status){
            QMessageBox::critical(nullptr, "Ошибка!", "Не удалось создать файл протокола НУ");
            return false;
        }
    }
    tempDirNU = QDir(tempDirNU).filePath("prot.txt");
    nuFile.setFileName(tempDirNU);
    if (!nuFile.exists()){
        if (!nuFile.open(QIODevice::WriteOnly)){
            QMessageBox::critical(nullptr, "Ошибка!", "Не удалось открыть файл протокола НУ!\n" + nuFile.errorString());
            return false;
        }
    } else if (!nuFile.isOpen()){
        if (!nuFile.open(QIODevice::Append)){
            QMessageBox::critical(nullptr, "Ошибка", "Не удалось открыть файл протокола НУ\n" + nuFile.errorString());
            return false;
        }
    }
    if (!writeFile.exists()){
        QString tempDir = /*QStandardPaths::writableLocation(QStandardPaths::TempLocation);*/ MainWindow::getOnParam("ПРОТОКОЛ");
        if (tempDir.isEmpty()){
            QMessageBox::warning(nullptr, "Ошибка!", "не удалось записать файл протокола по адресу в файле .on");
            tempDir = QCoreApplication::applicationDirPath().append("/TempFiles");
        }
        //tempDir.append("/PRIS");
        QDir().mkpath(tempDir);
        QString filePath = QDir(tempDir).filePath("prot.PCP");

        writeFile.setFileName(filePath);
        readFile.setFileName(filePath);
        writeFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        readFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        if (!writeFile.open(QIODevice::WriteOnly | QIODevice::Truncate) || !readFile.open(QIODevice::ReadOnly)){
            QMessageBox::critical(nullptr, "Ошибка!", "Не удалось открыть файл протокола\n" + writeFile.errorString() + "\n" + readFile.errorString());
            return false;
        }

        if (!codec){
            QMessageBox::critical(nullptr, "Ошибка!", "Не удалось подключить перекодировщик (cp1251 to UTF8)");
            return false;
        }

        QString param(QString("Протокол ПРИС. Версия %1. Создан %2 в %3.").arg(2).arg(QDateTime::currentDateTime().toString("dd.MM.yyyy")).arg(QDateTime::currentDateTime().toString("HH:mm:ss")));

        int len = param.length() + 2;
        QByteArray masForWrite;
        masForWrite.append(codec->fromUnicode("WinPrisPRT"));
        masForWrite.append(QByteArray(4, char(0)));
        masForWrite.append(static_cast<char>(len & 0xFF));
        masForWrite.append(static_cast<char>((len >> 8) & 0xFF));
        masForWrite.append(static_cast<char>(protV & 0xFF));
        masForWrite.append(static_cast<char>((protV >> 8) & 0xFF));
        masForWrite.append(codec->fromUnicode(param));
        writeFile.write(masForWrite);
        writeFile.flush();
        emit fileUpdate();

        writeRecord(QString("Раздел открыт %1 в %2\r\n").arg(QDateTime::currentDateTime().toString("dd.MM.yyyy")).arg(QDateTime::currentDateTime().toString("HH:mm:ss")), -1);
    } else{
        if (!writeFile.isOpen()){
            writeFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            readFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            if (!writeFile.open(QIODevice::Append) || !readFile.open(QIODevice::ReadOnly)){
                QMessageBox::critical(nullptr, "Ошибка!", "Не удалось открыть файл протокола");
                return false;
            }
        }
    }
    activeProt = true;
    return true;
}

bool ProtManager::writeRecord(QString param, const int atomType, int potok){
    if (!writeFile.isOpen()){
        return false;
    }
    if (atomType == -1){
        //param.append("\r\n");
    }
    else if (atomType == -2){
        param.prepend(char(potok)).prepend(char(0));
    }
    else if (atomType == 3){

    }
    else{
        return false;
    }
    QByteArray masForWrite;
    int len;
    if (atomType == 3) len = param.length() + beginStr.length();
    else len = param.length();
    masForWrite.append(static_cast<char>(atomType & 0xFF));
    masForWrite.append(static_cast<char>(len & 0xFF));
    masForWrite.append(static_cast<char>((len >> 8) & 0xFF));
    if (atomType == 3){
        QByteArray writeStr = beginStr;
        writeStr[4] = char(potok);
        masForWrite.append(writeStr);
    }
    masForWrite.append(codec->fromUnicode(param));
    writeFile.write(masForWrite);
    writeFile.flush();
    emit fileUpdate();

    return true;
}

bool ProtManager::saveFile(const QString& savePath, const QString& saveNUPath){
    if (!writeFile.isOpen() || !nuFile.isOpen()){
        return false;
    }
    writeFile.flush();
    writeFile.close();
    nuFile.flush();
    nuFile.close();
    if (readFile.isOpen()) readFile.close();
    this->lastPosFileSeek = 0;
    bool res = writeFile.rename(savePath);
    res &= nuFile.rename(saveNUPath);
    if (!res) return false;
    writeFile.setFileName("");
    readFile.setFileName("");
    nuFile.setFileName("");
    res &= createProtocol();
    emit fileSaved();
    return res;
}
static const int size_prewiev_title_prot = 10 + 4;
QString ProtManager::getAllFileDate(){
    if (!this->readFile.isOpen()){
        return QString();
    }

    QByteArray readMas;
    qint32 curSeek = 0;
    readFile.seek(curSeek);
    QString protInfo;
    while (!readFile.atEnd()){
        readMas.clear();
        readMas.reserve(1);
        //readFile.read(readMas.data(), 1);
        readMas = readFile.read(1);
        //qDebug() << readMas[0];
        if (readMas[0] == char('W')){
            if (readFile.pos() > 1){
                qDebug() << "Неверное распложение заголовка протокола";
                return QString();
            }
            readMas.clear();
            readMas.reserve(2);
            curSeek += size_prewiev_title_prot;
            readFile.seek(curSeek);
            //readFile.read(readMas.data(), 2);
            readMas = readFile.read(2);
            bool ok{false};
            /*int len = readMas.toInt(&ok);
            if (!ok){
                qDebug() << "Error convert len to int";
                return QString();
            }*/
            qint16 len{0};
            std::memcpy(&len, readMas.constData(), sizeof(len));
            curSeek += 2 + len;
            //readFile.seek(curSeek);
            readFile.seek(0);
            readMas = readFile.read(curSeek);
        } else if (readMas[0] == char(0xFF)){
            readMas.clear();
            readMas.reserve(2);
            //readFile.read(readMas.data(), 2);
            readMas = readFile.read(2);
            bool ok{false};
            /*int len = readMas.toInt(&ok);
            if (!ok){
                qDebug() << "Error convert len to int";
                return QString();
            }*/
            qint16 len{0};
            std::memcpy(&len, readMas.constData(), sizeof(len));
            curSeek += 3 + len;
            readFile.seek(curSeek);
        } else if (readMas[0] == char(0xFE)){
            readMas.clear();
            readMas.reserve(2);
            //readFile.read(readMas.data(), 2);
            readMas = readFile.read(2);
            bool ok{false};
            /*int len = readMas.toInt(&ok);
            if (!ok){
                qDebug() << "Error convert len to int";
                return QString();
            }*/
            qint16 len{0};
            std::memcpy(&len, readMas.constData(), sizeof(len));
            readMas.clear();
            readMas.reserve(len);
            //readFile.read(readMas.data(), len);
            readMas = readFile.read(len);

            /*int numPotok = readMas.mid(0, 2).toInt(&ok);
            if (!ok){
                qDebug() << "Erroro convert numPotok to int";
                return QString();
            }*/
            qint16 numPotok{0};
            std::memcpy(&numPotok, readMas.constData(), sizeof(numPotok));
            readMas = readMas.mid(2);
            QString styleString = codec->toUnicode(readMas);
            //qDebug() << styleString;
            /*
             * textForProt = QString("Поток=%1\r\n"
                          "ИмяПотока=Style%1\r\n"
                          "Название=Style%1\r\n"
                          "Color=%2\r\n"
                          "BackColor=%3\r\n"
                          "FontName=%4\r\n"
                          "Size=%5\r\n"
                          "Charset=%6\r\n"
                          "Bold=%7\r\n"
                          "Italic=%8\r\n"
                          "Underline=%9\r\n"
                          "Strikeout=%10\r\n").arg(styleTest.potok).arg(styleTest.color).arg(styleTest.backColor).arg(styleTest.fontName).arg(styleTest.fontSize).arg(styleTest.charSet).arg(styleTest.bold).arg(styleTest.italic).arg(styleTest.underLine).arg(styleTest.strikeOut);
                          */
           styleString.replace("\r", "");
           QStringList styleList = styleString.split("\n");
           QMap<QString, QString> styleParams;
           for (QString styleRow : styleList){
               QStringList pars = styleRow.split("=");
               if (pars.length() < 2) continue;
               styleParams.insert(pars[0], pars[1]);
           }
           //qDebug() << styleParams;
           QString textColor;
           QString backgroundColor;
           if (!constValues::colorTranslate.contains(styleParams.contains("Color") ? styleParams.value("Color") : QString()) ||
                   !constValues::colorTranslate.contains(styleParams.contains("BackColor") ? styleParams.value("BackColor") : QString())){
               textColor = "black";
               backgroundColor = "transparent";
           } else{
               textColor = constValues::colorTranslate.value(styleParams.value("Color"));
               backgroundColor = constValues::colorTranslate.value(styleParams.value("BackColor"));
           }
           QString textDecoration;
           /*if ((!styleParams.contains("Underline") || styleParams.value("Underline").isEmpty())
                   && (!styleParams.contains("Strikeout") || styleParams.value("Strikeout").isEmpty())) textDecoration = "none";
           else if ((styleParams.contains("Underline") && styleParams.value("Underline") == "1") &&
                    (styleParams.contains("Strikeout") || styleParams.value("Strikeout") == "1")) textDecoration = "underline line-through";
           else if ((styleParams.contains("Underline") && styleParams.value("Underline") == "1")) textDecoration = "underline";
           else textDecoration = "line-through";*/

           if ((styleParams.contains("Underline") && styleParams.value("Underline") == "1") &&
                               (styleParams.contains("Strikeout") || styleParams.value("Strikeout") == "1")) textDecoration = "underline line-through";
           else if ((styleParams.contains("Underline") && styleParams.value("Underline") == "1")) textDecoration = "underline";
           else if ((styleParams.contains("Strikeout")) && styleParams.value("Strikeout") == "1") textDecoration = "line-through";
           else textDecoration = "none";


           int fontSize {10};
           bool bold {false};
           bool italic {false};
           ok = false;
           if (styleParams.contains("Size")){
               fontSize = styleParams.value("Size").toInt(&ok);
           }
           if (!ok || fontSize <= 0 || !styleParams.contains("Size")){
               fontSize = 10;
           }
           if (styleParams.contains("Bold")){
               bold = styleParams.value("Bold").toInt(&ok);
           }
           if (!ok || !styleParams.contains("Bold")){
               bold = false;
           }
           if (styleParams.contains("Italic")){
               italic = styleParams.value("Italic").toInt(&ok);
           }
           if (!ok || !styleParams.contains("Italic")){
               italic = false;
           }
           QString fontName;
           if (styleParams.contains("FontName") && !styleParams.value("FontName").isEmpty()){
               fontName = styleParams.value("FontName");
           } else{
               fontName = "Courier New";
           }

           curHTMLStyleString = QString(R"(<span style="white-space: pre; color: %1; background-color: %2; font-family: '%3'; font-size: %4px; font-weight: %5; font-style: %6; text-decoration: %7;">)").arg(textColor).arg(backgroundColor).arg(fontName)
                       .arg(fontSize * 1.5).arg(bold ? "bold" : "normal").arg(italic ? "italic" : "normal").arg(textDecoration);
        } else if (readMas[0] == char(0x03)){
            readMas.clear();
            readMas = readFile.read(2);
            qint16 len{0};
            std::memcpy(&len, readMas.constData(), sizeof(len));
            readMas.clear();
            readMas = readFile.read(len);
            readMas = readMas.mid(4);
            //qDebug() << int(readMas[0]);
            readMas = readMas.mid(4);
            QString message = codec->toUnicode(readMas);
            //qDebug() << message;

            message.prepend(curHTMLStyleString);
            message.append("\n</span>");

            protInfo.append(message);
        } else{
            qDebug() << "ERROR";
            return QString();
        }

        //curSeek = readFile.pos();
    }

   /* QWidget *curProtWgt = new QWidget();
    QTextEdit *curProtTextEdit = new QTextEdit(curProtWgt);
    curProtTextEdit->setText(protInfo);
    QHBoxLayout *hBox = new QHBoxLayout(curProtWgt);
    hBox->addWidget(curProtTextEdit);
    curProtWgt->setLayout(hBox);
    curProtWgt->show();*/

    lastPosFileSeek = readFile.pos();
    if (protInfo.length() > 8 && protInfo.right(8) == "\n</span>"){
        protInfo.chop(8);
        protInfo.append("</span>");
    }

    return protInfo;
}

QString ProtManager::getNewFileDate(){
    if (!this->readFile.isOpen()){
        return QString();
    }

    QByteArray readMas;
    int curSeek = lastPosFileSeek;
    QString protInfo;
    readFile.seek(curSeek);
    while (!readFile.atEnd()){
        readMas.clear();
        readMas.reserve(1);
        //readFile.read(readMas.data(), 1);
        readMas = readFile.read(1);
        //qDebug() << readMas[0];
        if (readMas[0] == char('W')){
            if (readFile.pos() > 1){
                qDebug() << "Неверное распложение заголовка протокола";
                return QString();
            }
            readMas.clear();
            readMas.reserve(2);
            curSeek += size_prewiev_title_prot;
            readFile.seek(curSeek);
            //readFile.read(readMas.data(), 2);
            readMas = readFile.read(2);
            /*bool ok{false};
            int len = readMas.toInt(&ok);
            if (!ok){
                qDebug() << "Error convert len to int";
                return QString();
            }*/
            qint16 len{0};
            std::memcpy(&len, readMas.constData(), sizeof(len));
            curSeek += 2 + len;
            //readFile.seek(curSeek);
            readFile.seek(0);
            readMas = readFile.read(curSeek);
        } else if (readMas[0] == char(0xFF)){
            readMas.clear();
            readMas.reserve(2);
            //readFile.read(readMas.data(), 2);
            readMas = readFile.read(2);
            /*bool ok{false};
            int len = readMas.toInt(&ok);
            if (!ok){
                qDebug() << "Error convert len to int";
                return QString();
            }*/
            qint16 len{0};
            std::memcpy(&len, readMas.constData(), sizeof(len));
            curSeek += 3 + len;
            readFile.seek(curSeek);
        } else if (readMas[0] == char(0xFE)){
            readMas.clear();
            readMas.reserve(2);
            //readFile.read(readMas.data(), 2);
            readMas = readFile.read(2);
            bool ok{false};
            /*int len = readMas.toInt(&ok);
            if (!ok){
                qDebug() << "Error convert len to int";
                return QString();
            }*/
            qint16 len{0};
            std::memcpy(&len, readMas.constData(), sizeof(len));
            readMas.clear();
            readMas.reserve(len);
            //readFile.read(readMas.data(), len);
            readMas = readFile.read(len);

            /*int numPotok = readMas.mid(0, 2).toInt(&ok);
            if (!ok){
                qDebug() << "Erroro convert numPotok to int";
                return QString();
            }*/
            qint16 numPotok{0};
            std::memcpy(&numPotok, readMas.constData(), sizeof(numPotok));
            readMas = readMas.mid(2);
            QString styleString = codec->toUnicode(readMas);
            //qDebug() << styleString;
            /*
             * textForProt = QString("Поток=%1\r\n"
                          "ИмяПотока=Style%1\r\n"
                          "Название=Style%1\r\n"
                          "Color=%2\r\n"
                          "BackColor=%3\r\n"
                          "FontName=%4\r\n"
                          "Size=%5\r\n"
                          "Charset=%6\r\n"
                          "Bold=%7\r\n"
                          "Italic=%8\r\n"
                          "Underline=%9\r\n"
                          "Strikeout=%10\r\n").arg(styleTest.potok).arg(styleTest.color).arg(styleTest.backColor).arg(styleTest.fontName).arg(styleTest.fontSize).arg(styleTest.charSet).arg(styleTest.bold).arg(styleTest.italic).arg(styleTest.underLine).arg(styleTest.strikeOut);
                          */
           styleString.replace("\r", "");
           QStringList styleList = styleString.split("\n");
           QMap<QString, QString> styleParams;
           for (QString styleRow : styleList){
               QStringList pars = styleRow.split("=");
               if (pars.length() < 2) continue;
               styleParams.insert(pars[0], pars[1]);
           }
           //qDebug() << styleParams;
           QString textColor;
           QString backgroundColor;
           if (!constValues::colorTranslate.contains(styleParams.contains("Color") ? styleParams.value("Color") : QString()) ||
                   !constValues::colorTranslate.contains(styleParams.contains("BackColor") ? styleParams.value("BackColor") : QString())){
               textColor = "black";
               backgroundColor = "transparent";
           } else{
               textColor = constValues::colorTranslate.value(styleParams.value("Color"));
               backgroundColor = constValues::colorTranslate.value(styleParams.value("BackColor"));
           }
           QString textDecoration;
           /*if ((!styleParams.contains("Underline") || styleParams.value("Underline").isEmpty())
                   && (!styleParams.contains("Strikeout") || styleParams.value("Strikeout").isEmpty())) textDecoration = "none";
           else if ((styleParams.contains("Underline") && styleParams.value("Underline") == "1") &&
                    (styleParams.contains("Strikeout") || styleParams.value("Strikeout") == "1")) textDecoration = "underline line-through";
           else if ((styleParams.contains("Underline") && styleParams.value("Underline") == "1")) textDecoration = "underline";
           else textDecoration = "line-through";*/

           if ((styleParams.contains("Underline") && styleParams.value("Underline") == "1") &&
                               (styleParams.contains("Strikeout") || styleParams.value("Strikeout") == "1")) textDecoration = "underline line-through";
           else if ((styleParams.contains("Underline") && styleParams.value("Underline") == "1")) textDecoration = "underline";
           else if ((styleParams.contains("Strikeout")) && styleParams.value("Strikeout") == "1") textDecoration = "line-through";
           else textDecoration = "none";


           int fontSize {10};
           bool bold {false};
           bool italic {false};
           ok = false;
           if (styleParams.contains("Size")){
               fontSize = styleParams.value("Size").toInt(&ok);
           }
           if (!ok || fontSize <= 0 || !styleParams.contains("Size")){
               fontSize = 10;
           }
           if (styleParams.contains("Bold")){
               bold = styleParams.value("Bold").toInt(&ok);
           }
           if (!ok || !styleParams.contains("Bold")){
               bold = false;
           }
           if (styleParams.contains("Italic")){
               italic = styleParams.value("Italic").toInt(&ok);
           }
           if (!ok || !styleParams.contains("Italic")){
               italic = false;
           }
           QString fontName;
           if (styleParams.contains("FontName") && !styleParams.value("FontName").isEmpty()){
               fontName = styleParams.value("FontName");
           } else{
               fontName = "Courier New";
           }

           curHTMLStyleString = QString(R"(<span style="white-space: pre; color: %1; background-color: %2; font-family: '%3'; font-size: %4px; font-weight: %5; font-style: %6; text-decoration: %7;">)").arg(textColor).arg(backgroundColor).arg(fontName)
                       .arg(fontSize * 1.5).arg(bold ? "bold" : "normal").arg(italic ? "italic" : "normal").arg(textDecoration);
        } else if (readMas[0] == char(0x03)){
            readMas.clear();
            readMas = readFile.read(2);
            qint16 len{0};
            std::memcpy(&len, readMas.constData(), sizeof(len));
            readMas.clear();
            readMas = readFile.read(len);
            readMas = readMas.mid(4);
            //qDebug() << int(readMas[0]);
            readMas = readMas.mid(4);
            QString message = codec->toUnicode(readMas);
            //qDebug() << message;
            message.prepend(curHTMLStyleString);
            message.append("\n</span>");
            protInfo.append(message);
        } else{
            qDebug() << "ERROR";
            return QString();
        }

        //curSeek = readFile.pos();
    }

   /* QWidget *curProtWgt = new QWidget();
    QTextEdit *curProtTextEdit = new QTextEdit(curProtWgt);
    curProtTextEdit->setText(protInfo);
    QHBoxLayout *hBox = new QHBoxLayout(curProtWgt);
    hBox->addWidget(curProtTextEdit);
    curProtWgt->setLayout(hBox);
    curProtWgt->show();*/

    lastPosFileSeek = readFile.pos();

    if (protInfo.length() > 8 && protInfo.right(8) == "\n</span>"){
        protInfo.chop(8);
        protInfo.append("</span>");
    }

    return protInfo;
}

QString ProtManager::getFilePath(){
    return readFile.fileName();
}

bool ProtManager::writeRecordToNU(QString record){
    if (!nuFile.isOpen()){
        return false;
    }
    if (record.length() > 0 && record.right(1) != "\n") record.append("\n");
    nuFile.write(QByteArray(record.toLocal8Bit()));
    nuFile.flush();
    return true;
}

ProtManager::~ProtManager(){
    if (writeFile.isOpen()) writeFile.close();
    if (readFile.isOpen()) readFile.close();
}
