#include "directparser.h"
#include <QFile>
#include <QtWidgets>

DirectParser::DirectParser()
{

}

const QList<DirectParser::Direct*>& DirectParser::parseFile(const QString& fileName){
    QFile file(fileName);
    if (!file.exists()){
        qDebug() << "File Not Exists";
        return *(new QList<DirectParser::Direct*>());
    }

    /*if (QFileInfo(fileName).suffix().toUpper() != "DIP"){
        qDebug() << "File Suffix Not DIP";
        return *(new QList<DirectParser::Direct*>());
    }*/

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        qDebug() << "File Not Open";
        return *(new QList<DirectParser::Direct*>());
    }

    QTextStream in(&file);
    //int numDir = 0;
    QString text = "";
    while (!in.atEnd()){
        text.append(in.readLine() + "\n");
    }

    this->parseString(text);

    /*if (!parse(in)){
        qDebug() << "ERROR PARSE DIRECT";
        return*(new QList<DirectParser::Direct*>());
    }*/



    file.close();
    /*for (int i = 0; i < directives.count(); ++i){
        qDebug() << "_________________________________________________________________________";
        qDebug() << "Direct Num: " << i;
        QTextCodec *codec = QTextCodec::codecForName("Windows-1251");
        qDebug() << "METKA: " << codec->toUnicode(directives[i]->metka.toLocal8Bit());
        qDebug() << "DIRECTIVE: " << codec->toUnicode(directives[i]->directive.toLocal8Bit());
        qDebug() << "DIR PARAM: " << directives[i]->paramDirect;
        qDebug() << "_________________________________________________________________________";
    }*/
    return directives;
}

const QList<DirectParser::Direct*>& DirectParser::parseString(const QString& strForParse){
    QString str = strForParse;
    QTextStream in(&str);

    if (!parse(in)){
        qDebug() << "ERROR PARSE DIRECT";
        return *(new QList<DirectParser::Direct*>());
    }

    return directives;
}

const QList<DirectParser::Direct*>& DirectParser::parseKO(const QString& KOStr){
    QString str = KOStr;
    str.prepend("О!");
    str.append("!");
    QTextStream in(&str);

    if (!parse(in)){
        qDebug() << "ERROR PARSE DIRECT";
        return *(new QList<DirectParser::Direct*>());
    }

    if (directives.count() == 1){
        directives[0]->numDirect = -1;
        directives[0]->numLine = -1;
    }

    return directives;
}

DirectParser::TypeDirect DirectParser::getDir(const QString& direct){
    DirectParser::TypeDirect tDir;
    if      (direct == "*")         tDir = TypeDirect::COMMENT;
    else if (direct == "А_КОНТР")   tDir = TypeDirect::A_KONTR;
    else if (direct == "ВАРИАНТК")  tDir = TypeDirect::VARIANTK;
    else if (direct == "ВЫБОР")     tDir = TypeDirect::VYBOR;
    else if (direct == "ВЫБОР100")  tDir = TypeDirect::VYBOR_100;
    else if (direct == "ВЫЗВАТЬ")   tDir = TypeDirect::VYSVAT;
    else if (direct == "ВЫХОД")     tDir = TypeDirect::VYHOD;
    else if (direct == "ВЫЧИСЛ")    tDir = TypeDirect::VYCHISL;
    else if (direct == "ДИРЕКТ")    tDir = TypeDirect::DIRECT;
    else if (direct == "ЕСЛИДА")    tDir = TypeDirect::ESLI_DA;
    else if (direct == "ЗАПРОС")    tDir = TypeDirect::ZAPROS;
    else if (direct == "КПРОГРАМ")  tDir = TypeDirect::KPROGRAM;
    else if (direct == "КСИНОНИМ")  tDir = TypeDirect::KSINONIM;
    else if (direct == "НА")        tDir = TypeDirect::NA;
    else if (direct == "ПОВТОР")    tDir = TypeDirect::POVTOR;
    else if (direct == "ПОДК_1М")   tDir = TypeDirect::PODKL_1M;
    else if (direct == "ПОДКСОЕД")  tDir = TypeDirect::PODKSOED;
    else if (direct == "ПРОВЕРКА")  tDir = TypeDirect::PROVERKA;
    else if (direct == "ПРОГРАМ")   tDir = TypeDirect::PROGRAM;
    else if (direct == "ПРЦ")       tDir = TypeDirect::PRC;
    else if (direct == "ПСИ")       tDir = TypeDirect::PSI;
    else if (direct == "ПСЦ")       tDir = TypeDirect::PSC;
    else if (direct == "ПСЦ_Р")     tDir = TypeDirect::PSC_R;
    else if (direct == "ПУСК")      tDir = TypeDirect::PUSK;
    else if (direct == "РВЫХОД")    tDir = TypeDirect::RVYHOD;
    else if (direct == "РР_ПАР")    tDir = TypeDirect::RR_PAR;
    else if (direct == "СИНОНИМ")   tDir = TypeDirect::SINONIM;
    else if (direct == "СООБЩ")     tDir = TypeDirect::SOOBCH;
    else if (direct == "СП")        tDir = TypeDirect::SP;
    else if (direct == "СТОП")      tDir = TypeDirect::STOP;
    else if (direct == "УВ")        tDir = TypeDirect::UV;
    else if (direct == "ПНЦ")       tDir = TypeDirect::PNC;
    else if (direct == "ПНЦ_Р")     tDir = TypeDirect::PNC_R;
    else                            tDir = TypeDirect::NO_DIRECT;

    return tDir;
}

bool DirectParser::parse(QTextStream &in){
    int numDir = 0;
    int numLine = -1;
    while (!in.atEnd()){
        numLine += 1;
        QString line = in.readLine();
        if (line[0] != "О") continue;
        DirectParser::Direct *direct = new DirectParser::Direct;
        numDir += 1;
        QList<QString> linePars = line.split("!");
        if (!linePars.isEmpty() && linePars.last().isEmpty()) linePars.removeLast();

        QList<QList<QString>> linePar;

        for (const QString& par : linePars){
            QList<QString> list2 = par.split(" ");
            list2.removeAll("");
            linePar.append(list2);
        }

        QString text;
        if (linePar.count() >= 2 && linePar[1].count() >= 1){text = linePar[1][0];}
        else{
            text = "";
            direct->direct = DirectParser::TypeDirect::NO_DIRECT;
            direct->directive = text;
            direct->numDirect = numDir;
            direct->numLine = numLine;
            directives.append(direct);
            continue;
        }
        if (text[0] == ":"){
            if (linePar[1].count() < 2){
                linePar[1].insert(2, "");
            }
            direct->metka = text;
            text = linePar[1][1];
        }
        linePar.removeAt(0);
        direct->paramDirect.append("");
        linePar[0].removeAt(0);
        if (!direct->metka.isEmpty()) linePar[0].removeAt(0);
        DirectParser::TypeDirect tDir = getDir(text);
        /*if (tDir == DirectParser::TypeDirect::NO_DIRECT){
            qDebug() << text << " IS NO DIRECT";
            return false;
        }*/
        direct->direct = tDir;
        direct->directive = text;
        direct->numDirect = numDir;
        direct->numLine = numLine;
        if (tDir == DirectParser::TypeDirect::NO_DIRECT){
            directives.append(direct);
            continue;
        }
        //test
        direct->testParamDirect.append(linePar);
        direct->testParamDirect[0].prepend(QList<QString>());
        //endTest
        for (int col = 0; col < linePar.count(); ++col){
            for (int block = 0; block < linePar[col].count(); ++block){
                if (linePar[col][block].isEmpty()) continue;
                direct->paramDirect.append(linePar[col][block]);
            }
            direct->paramDirect.append("");
        }
        direct->paramDirect.append("\n");

        while (!in.atEnd()){
            int curLine = numLine;
            qint64 curPos = in.pos();
            numLine += 1;
            QString line = in.readLine();
            if (line[0] == "О" /*|| line[0] == "К" || line[0] == "П"*/){
                numLine = curLine;
                in.seek(curPos);
                break;
            }
            if (line[0] == "К" || line[0] == "П"){
                continue;
            }
            if (QString(line).replace("!", "").count() == 0){
                /*in.seek(curPos);
                break;*/
                continue;
            }
            QList<QString> linePars = line.split("!");
            if (!linePars.isEmpty() && linePars.last().isEmpty()) linePars.removeLast();
            QList<QList<QString>> linePar;

            for (const QString& text : linePars){
                QList<QString> l1 = text.split(" ");
                l1.removeAll("");
                linePar.append(l1);
            }

            //test
            direct->testParamDirect.append(linePar);
            //endTest
            for (int col = 0; col < linePar.count(); ++col){
                for (int block = 0; block < linePar[col].count(); ++block){
                    if (linePar[col][block].isEmpty()) continue;
                    direct->paramDirect.append(linePar[col][block]);
                }
                direct->paramDirect.append("");
            }
            direct->paramDirect.append("\n");
        }
        directives.append(direct);

    }
    return true;
}

bool DirectParser::isOperatorDirect(DirectParser::TypeDirect dirType){
    bool result{false};
    switch (dirType) {
    case(DirectParser::TypeDirect::A_KONTR)     :   result = true;  break;
    case(DirectParser::TypeDirect::VARIANTK)    :   result = false; break;
    case(DirectParser::TypeDirect::VYBOR)       :   result = false; break;
    case(DirectParser::TypeDirect::VYBOR_100)   :   result = true;  break;
    case(DirectParser::TypeDirect::VYSVAT)      :   result = true;  break;
    case(DirectParser::TypeDirect::VYHOD)       :   result = true;  break;
    case(DirectParser::TypeDirect::VYCHISL)     :   result = true;  break;
    case(DirectParser::TypeDirect::DIRECT)      :   result = false; break;
    case(DirectParser::TypeDirect::ESLI_DA)     :   result = false; break;
    case(DirectParser::TypeDirect::ZAPROS)      :   result = true;  break;
    case(DirectParser::TypeDirect::COMMENT)     :   result = true;  break;
    case(DirectParser::TypeDirect::KPROGRAM)    :   result = false; break;
    case(DirectParser::TypeDirect::KSINONIM)    :   result = true;  break;
    case(DirectParser::TypeDirect::NA)          :   result = true;  break;
    case(DirectParser::TypeDirect::POVTOR)      :   result = true;  break;
    case(DirectParser::TypeDirect::PODKL_1M)    :   result = true;  break;
    case(DirectParser::TypeDirect::PODKSOED)    :   result = true;  break;
    case(DirectParser::TypeDirect::PROVERKA)    :   result = true;  break;
    case(DirectParser::TypeDirect::PROGRAM)     :   result = false; break;
    case(DirectParser::TypeDirect::PRC)         :   result = false; break;
    case(DirectParser::TypeDirect::PSI)         :   result = false; break;
    case(DirectParser::TypeDirect::PSC)         :   result = false; break;
    case(DirectParser::TypeDirect::PSC_R)       :   result = true;  break;
    case(DirectParser::TypeDirect::PUSK)        :   result = true;  break;
    case(DirectParser::TypeDirect::RVYHOD)      :   result = true;  break;
    case(DirectParser::TypeDirect::RR_PAR)      :   result = true;  break;
    case(DirectParser::TypeDirect::SINONIM)     :   result = true;  break;
    case(DirectParser::TypeDirect::SOOBCH)      :   result = false; break;
    case(DirectParser::TypeDirect::SP)          :   result = true;  break;
    case(DirectParser::TypeDirect::STOP)        :   result = false; break;
    case(DirectParser::TypeDirect::UV)          :   result = true;  break;
    case(DirectParser::TypeDirect::PNC)         :   result = false; break;
    case(DirectParser::TypeDirect::PNC_R)       :   result = true;  break;
    default                                     :   result = false; break;
    }
    return result;
}

bool DirectParser::isTableDirect(DirectParser::TypeDirect dirType){
    bool result{false};
    switch (dirType) {
    case(DirectParser::TypeDirect::A_KONTR)     :   result = true;  break;
    case(DirectParser::TypeDirect::VARIANTK)    :   result = true;  break;
    case(DirectParser::TypeDirect::VYBOR)       :   result = true;  break;
    case(DirectParser::TypeDirect::VYBOR_100)   :   result = true;  break;
    case(DirectParser::TypeDirect::VYSVAT)      :   result = true;  break;
    case(DirectParser::TypeDirect::VYHOD)       :   result = true;  break;
    case(DirectParser::TypeDirect::VYCHISL)     :   result = true;  break;
    case(DirectParser::TypeDirect::DIRECT)      :   result = true;  break;
    case(DirectParser::TypeDirect::ESLI_DA)     :   result = true;  break;
    case(DirectParser::TypeDirect::ZAPROS)      :   result = true;  break;
    case(DirectParser::TypeDirect::COMMENT)     :   result = false; break;
    case(DirectParser::TypeDirect::KPROGRAM)    :   result = true;  break;
    case(DirectParser::TypeDirect::KSINONIM)    :   result = true;  break;
    case(DirectParser::TypeDirect::NA)          :   result = true;  break;
    case(DirectParser::TypeDirect::POVTOR)      :   result = false; break;
    case(DirectParser::TypeDirect::PODKL_1M)    :   result = true;  break;
    case(DirectParser::TypeDirect::PODKSOED)    :   result = true;  break;
    case(DirectParser::TypeDirect::PROVERKA)    :   result = true;  break;
    case(DirectParser::TypeDirect::PROGRAM)     :   result = true;  break;
    case(DirectParser::TypeDirect::PRC)         :   result = true;  break;
    case(DirectParser::TypeDirect::PSI)         :   result = true;  break;
    case(DirectParser::TypeDirect::PSC)         :   result = true;  break;
    case(DirectParser::TypeDirect::PSC_R)       :   result = false; break;
    case(DirectParser::TypeDirect::PUSK)        :   result = false; break;
    case(DirectParser::TypeDirect::RVYHOD)      :   result = true;  break;
    case(DirectParser::TypeDirect::RR_PAR)      :   result = true;  break;
    case(DirectParser::TypeDirect::SINONIM)     :   result = true;  break;
    case(DirectParser::TypeDirect::SOOBCH)      :   result = true;  break;
    case(DirectParser::TypeDirect::SP)          :   result = false; break;
    case(DirectParser::TypeDirect::STOP)        :   result = true;  break;
    case(DirectParser::TypeDirect::UV)          :   result = true;  break;
    case(DirectParser::TypeDirect::PNC)         :   result = true;  break;
    case(DirectParser::TypeDirect::PNC_R)       :   result = false; break;
    default                                     :   result = false; break;
    }
    return result;
}

bool DirectParser::isVariantDirect(TypeDirect dirType){
    bool result{false};
    switch (dirType) {
    case(DirectParser::TypeDirect::A_KONTR)     :   result = true;  break;
    case(DirectParser::TypeDirect::VYBOR_100)   :   result = true;  break;
    case(DirectParser::TypeDirect::VYSVAT)      :   result = true;  break;
    case(DirectParser::TypeDirect::VYCHISL)     :   result = true;  break;
    case(DirectParser::TypeDirect::DIRECT)      :   result = true;  break;
    case(DirectParser::TypeDirect::ZAPROS)      :   result = true;  break;
    case(DirectParser::TypeDirect::KSINONIM)    :   result = true;  break;
    case(DirectParser::TypeDirect::NA)          :   result = true;  break;
    case(DirectParser::TypeDirect::PODKL_1M)    :   result = true;  break;
    case(DirectParser::TypeDirect::PODKSOED)    :   result = true;  break;
    case(DirectParser::TypeDirect::PROVERKA)    :   result = true;  break;
    case(DirectParser::TypeDirect::PSC_R)       :   result = true;  break;
    case(DirectParser::TypeDirect::RR_PAR)      :   result = true;  break;
    case(DirectParser::TypeDirect::SINONIM)     :   result = true;  break;
    case(DirectParser::TypeDirect::SOOBCH)      :   result = true;  break;
    case(DirectParser::TypeDirect::UV)          :   result = true;  break;
    case(DirectParser::TypeDirect::PNC_R)       :   result = true;  break;
    default                                     :   result = false; break;
    }
    return result;
}
