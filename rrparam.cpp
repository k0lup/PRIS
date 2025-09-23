#include "rrparam.h"
#include "mainwindow.h"

const QString ID_TEMPLATE = "(FL.&lt;блок&gt;_&lt;имя&gt; )";
const QString RR_PARAM_TEMPLATE = R"(^FL.([A-Za-z0-9]{1,3})_([A-Za-z0-9_\-\/\.]{1,15})(?:\[(\d+)\])?(?:\((\d+)\))?$)"; //шаблон РР_ПАРАМЕТРА (должны быть имя блока и имя параметра)
const QString RR_PARAM_VARIABLE_TEMPLATE = R"(^FL.([A-Za-z0-9]{1,3})_?([A-Za-z0-9_\-\/\.]{1,15})?(?:\[(\d+)\])?(?:\((\d+)\))?$)"; //ШАБЛОН РР_ПАРАМЕТРА (должно быть только имя блока)
//второй шаблон используется для проверки РР_ПАРАМЕТРА в директиве ЗАПРОС (если указан только блок). Также данный шаблон может использоваться при удалении блока соотвествующем методом данного класса


RRParam::RRParam(const QString& paramName)
{
    QRegularExpression regex(RR_PARAM_TEMPLATE);
    QRegularExpressionMatch match = regex.match(paramName);
    if (!match.hasMatch()){
        m_errorMessage = "Не удалось распознать имя РР параметра";
        m_hasError = true;
        m_errorCode = ERROR_CODE::SYNTAX_ERROR;
        return;
    }

    m_block = match.captured(1);
    m_name = match.captured(2);
    m_index = -1;
    if (!match.captured(3).isEmpty()){
        bool status{false};
        m_index = match.captured(3).toInt(&status);
        if (m_index < 0 || !status){
            m_errorMessage = "Некоррекнтный индекс РР параметра";
            m_hasError = true;
            m_errorCode = ERROR_CODE::SYNTAX_ERROR;
            return;
        }
    }
    m_len = -1;
    if (!match.captured(4).isEmpty()){
        bool status{false};
        m_len = match.captured(4).toInt(&status);
        if (m_len < 1 || !status){
            m_errorMessage = "Некорректная длина массива";
            m_hasError = true;
            m_errorCode = ERROR_CODE::SYNTAX_ERROR;
            return;
        }
    }
    m_errorCode = ERROR_CODE::GOOD_STATE;
    m_hasError = false;
    m_errorMessage = "";
}

bool RRParam::isExist(){
    if (m_hasError) return false;

    QString queryString = QString("SELECT COUNT(*) FROM RR_PAR WHERE Bl_Name = '%1' AND Par_Name = '%2'").arg(m_block).arg(m_name);
    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        m_errorMessage = QString("ОШИБКА ОБРАЩЕНИЯ К БД РР ПАРАМЕТРОВ: " + query.lastError().text());
        m_hasError = true;
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        return false;
    }
    if (!query.next()){
        m_errorMessage = QString("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        m_hasError = true;
        return false;
    }
    bool status{false};
    int count = query.value(0).toInt(&status);
    if (!status){
        m_errorMessage = QString("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        m_hasError = true;
        return false;
    }
    if (count == 0) return false;
    else return true;
}

bool RRParam::isArray(){
    if (m_hasError) return false;

    int len = getLength();
    if (m_hasError || len <= 0) return false;
    else return true;
}

int RRParam::getLength(){
    if (m_hasError) return false;

    QString queryString = QString("SELECT MAX(Index) FROM RR_PAR WHERE Bl_Name = '%1' AND Par_Name = '%2'").arg(m_block).arg(m_name);
    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        m_errorMessage = QString("ОШИБКА ОБРАЩЕНИЯ К БД РР ПАРАМЕТРОВ: " + query.lastError().text());
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        m_hasError = true;
        return -2;
    }
    if (!query.next()){
        m_errorMessage = QString("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        m_hasError = true;
        return -2;
    }
    if (query.value(0).isNull()) return 0;
    bool ok{false};
    int value = query.value(0).toInt(&ok);
    if (!ok){
        m_errorMessage = QString("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        m_hasError = true;
        return -1;
    }
    return value + 1;
}

double RRParam::getValue(bool *status){
    if (m_hasError){
        if (status != nullptr) *status = false;
        return 0;
    }

    if (!isExist()){
        if (status != nullptr) *status = false;
        if (m_hasError){
            return 0;
        } else{
            m_hasError = true;
            m_errorMessage = "Не удалось  найти РР ПАРАМЕТР с заданным именем";
            m_errorCode = ERROR_CODE::CONTAINS_ERROR;
            return 0;
        }
    }
    if (m_index != -1){
        if (!isArray()){
            if (status != nullptr) *status = false;
            if (m_hasError){
                return 0;
            } else{
                m_errorMessage = "РР Параметр не массив";
                m_errorCode = ERROR_CODE::CONTAINS_ERROR;
                m_hasError = true;
                return 0;
            }
        }
        if (m_index >= getLength()){
            if (status != nullptr) *status = false;
            if (m_hasError){
                return 0;
            } else{
                m_errorMessage = "Недопустимый индекс РР параметра";
                m_errorCode = ERROR_CODE::CONTAINS_ERROR;
                m_hasError = true;
                return 0;
            }
        }
    } else{
        if (isArray()){
            if (status != nullptr) *status = false;
            if (m_hasError){
                return 0;
            } else{
                m_errorMessage = "РР параметр является массивом. Для получения элемента необходимо указать индекс";
                m_errorCode = ERROR_CODE::CONTAINS_ERROR;
                m_hasError = true;
                return 0;
            }
        }
    }

    float value = 0.0;

    QString queryString = QString("SELECT Val FROM RR_PAR WHERE Bl_Name = '%1' AND Par_Name = '%2'").arg(m_block).arg(m_name);
    if (m_index != -1) queryString.append(QString(" AND Index = %3").arg(QString::number(m_index)));
    else queryString.append(QString(" AND Index IS NULL"));

    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        if (status != nullptr) *status = false;
        m_hasError = true;
        m_errorMessage = QString("ОШИБКА ОБРАЩЕНИЯ К БД РР ПАРАМЕТРОВ: " + query.lastError().text());
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        return 0;
    }
    if (!query.next()){
        if (status != nullptr) *status = false;
        m_hasError = true;
        m_errorMessage = QString("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        return 0;
    }
    bool ok = false;
    value = query.value(0).toFloat(&ok);
    if (!ok){
        if (status != nullptr) *status = false;
        m_hasError = true;
        m_errorMessage = QString("ОШИБКА ПОЛУЧЕНИЯ ЗНАЧЕНИЙ ИЗ БД РР ПАРАМЕТРОВ");
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        return 0;
    }
    if (status != nullptr) *status = true;
    return value;
}

bool RRParam::setValue(const double value){
    if (m_hasError){
        return false;
    }

    if (!isExist()){
        if (!m_hasError)
        {
            m_hasError = true;
            m_errorMessage = "Не удалось  найти РР ПАРАМЕТР с заданным именем";
            m_errorCode = ERROR_CODE::CONTAINS_ERROR;
        }
        return false;
    }
    if (m_index != -1){
        if (!isArray()){
            if (!m_hasError){
                m_errorMessage = "РР Параметр не массив";
                m_hasError = true;
                m_errorCode = ERROR_CODE::CONTAINS_ERROR;
            }
            return false;
        }
        if (m_index >= getLength()){
            if (!m_hasError)
            {
                m_errorMessage = "Недопустимый индекс РР параметра";
                m_hasError = true;
                m_errorCode = ERROR_CODE::CONTAINS_ERROR;
            }
            return false;
        }
    } else{
        if (isArray()){
            if (!m_hasError)
            {
                m_errorMessage = "РР параметр является массивом. Для получения элемента необходимо указать индекс";
                m_hasError = true;
                m_errorCode = ERROR_CODE::CONTAINS_ERROR;
            }
            return false;
        }
    }

    QString queryString;
    if (m_index == -1) queryString = QString("UPDATE RR_PAR SET Val = %1 WHERE Bl_Name = '%2' AND Par_Name = '%3'").arg(value).arg(m_block).arg(m_name);
    else queryString = QString("UPDATE RR_PAR SET Val = %1 WHERE Bl_Name = '%2' AND Par_Name = '%3' AND Index = %4").arg(value).arg(m_block).arg(m_name).arg(m_index);

    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        m_errorMessage = QString("ОШИБКА ОБРАЩЕНИЯ К БД РР ПАРАМЕТРОВ: " + query.lastError().text());
        m_hasError = true;
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        return false;
    }
    if (query.numRowsAffected() <= 0){
        m_errorMessage = QString("ОШИБКА ЗАПИСИ РЕЗУЛЬТАТА В БД");
        m_hasError = true;
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        return false;
    }
    query.clear();
    return true;
}

bool RRParam::create(const double value){
    if (m_hasError) return false;

    if (m_index >= 0){
        m_hasError = true;
        m_errorMessage = "ПРИ СОЗДАНИИ РР_ПАРАМЕТРА В ЕГО ИДЕНТИФИКАТОРЕ НЕ ДОЛЖНО БЫТЬ ИНДЕКСА";
        m_errorCode = ERROR_CODE::SYNTAX_ERROR;
        return false;
    }

    if (isExist()){
        m_hasError = true;
        m_errorMessage = "РР_ПАРАМЕТР УЖЕ СУЩЕСТВУЕТ";
        m_errorCode = ERROR_CODE::CONTAINS_ERROR;
        return false;
    }

    if (m_hasError) return false;

    QString queryString;
    if (m_len < 0){
        queryString = QString("INSERT INTO RR_PAR (Bl_Name, Par_Name, Val) VALUES ('%1', '%2', %3)").arg(m_block).arg(m_name).arg(value);
        QSqlQuery query = MainWindow::getQueryRRDB(queryString);
        if (!query.isActive()){
            m_hasError = true;
            m_errorMessage = ("ОШИБКА ОБРАЩЕНИЯ К БД: " + query.lastError().text());
            m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
            return false;
        }
        query.clear();
    } else{
        for (int index = 0; index < m_len; ++index){
            queryString = QString("INSERT INTO RR_PAR (Bl_Name, Par_Name, Index, Val) VALUES ('%1', '%2', %3, %4)").arg(m_block).arg(m_name).arg(index).arg(value);
            QSqlQuery query = MainWindow::getQueryRRDB(queryString);
            if (!query.isActive()){
                m_hasError = true;
                m_errorMessage = ("ОШИБКА ОБРАЩЕНИЯ К БД: " + query.lastError().text());
                m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
                return false;
            }
            query.clear();
            m_index = 0;
        }
    }

    if (!isExist()){
        if (m_hasError) return false;
        m_hasError = true;
        m_errorMessage = "Непредвиденная ошибка при создании РР_ПАРАМЕТРА";
        m_errorCode = ERROR_CODE::CONTAINS_ERROR;
        return false;
    }

    m_len = -1;
    return true;
}

bool RRParam::remove(){
    if (m_hasError) return false;

    if (!isExist()){
        m_hasError = true;
        m_errorMessage = "УКАЗАННОГО РР_ПАРАМЕТРА НЕТ В БД";
        m_errorCode = ERROR_CODE::CONTAINS_ERROR;
        return false;
    }

    QString queryString = QString("DELETE FROM RR_PAR WHERE Bl_Name = '%1' AND Par_Name = '%2'").arg(m_block).arg(m_name);
    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        m_hasError = true;
        m_errorMessage = ("ОШИБКА ОБРАЩЕНИЯ К БД: " + query.lastError().text());
        m_errorCode = ERROR_CODE::DB_QUERY_ERROR;
        return false;
    }
    query.clear();


    return true;
}

bool RRParam::removeBlock(const QString &blockName, QString *errorMessage){
    QRegularExpression regex(R"(^FL.([A-Za-z0-9]{1,3})$)");
    QRegularExpressionMatch match = regex.match(blockName);
    if (!match.hasMatch()){
        if (errorMessage != nullptr) *errorMessage = (QString("ОШИБКА В ИДЕНТИФИКАТОРЕ ") + ID_TEMPLATE);
        return false;
    }

    QString queryString = QString("DELETE FROM RR_PAR WHERE Bl_Name = '%1'").arg(match.captured(1));
    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        if (errorMessage != nullptr) *errorMessage = ("ОШИБКА ОБРАЩЕНИЯ К БД: " + query.lastError().text());
        return false;
    }
    query.clear();

    return true;
}


QString RRParam::getErrorText(){
    return m_errorMessage;
}

RRParam::ERROR_CODE RRParam::getErrorCode(){
    return m_errorCode;
}

bool RRParam::isHasError(){
    return m_hasError;
}

bool RRParam::isValid(){
    if (m_hasError) return false;

    if (m_len >= 0 && m_index < 0){
        m_index = 0;
    }

    if (!isExist()){
        if (m_hasError) return false;
        m_hasError = true;
        m_errorMessage = "РР_ПАРАМЕТР ОТСУТСТВУЕТ В БД";
        m_errorCode = ERROR_CODE::CONTAINS_ERROR;
        return false;
    }

    if (m_index >= 0 && !isArray()){
        if (m_hasError) return false;
        m_hasError = true;
        m_errorMessage = "РР_ПАРАМЕТР НЕ МАССИВ";
        m_errorCode = ERROR_CODE::CONTAINS_ERROR;
        return false;
    }

    int len = getLength();
    if (m_hasError) return false;

    if (m_index >= len){
        m_hasError = true;
        m_errorMessage = "ИНДЕКС ПРЕВЫШАЕТ РАЗМЕРНОСТЬ МАССИВА РР_ПАРАМЕТРА";
        m_errorCode = ERROR_CODE::CONTAINS_ERROR;
        return false;
    }

    if (m_index + m_len > len){
        m_hasError = true;
        m_errorMessage = "ДЛИНА ЗАПРАШИВАЕМОГО МАССИВА С ИНДЕКСА " + QString::number(m_index) + " ПРЕВЫШАЕТ РАЗМЕР МАССИВА РР_ПАРАМЕТРА";
        m_errorCode = ERROR_CODE::CONTAINS_ERROR;
        return false;
    }

    return true;
}

void RRParam::resetError(){
    m_errorCode = ERROR_CODE::GOOD_STATE;
    m_hasError = false;
    m_errorMessage = "";
}

void RRParam::setBlockName(const QString &blockName){
    m_block = blockName;
}

void RRParam::setParamName(const QString &paramName){
    m_name = paramName;
}

void RRParam::setIndex(const int index){
    m_index = index;
}

void RRParam::setLen(const int len){
    m_len = len;
}

QString RRParam::getBlockName(){
    return m_block;
}

QString RRParam::getParamName(){
    return m_name;
}

int RRParam::getIndex(){
    return m_index;
}

int RRParam::getLen(){
    return m_len;
}

QString RRParam::getFullParamName(RRParam::FORMAT_PARAM format){
    if (format == RRParam::FORMAT_PARAM::EMPTY) return QString();
    QString paramName("FL." + m_block);
    if (format == RRParam::FORMAT_PARAM::BLOCK) return paramName;
    paramName.append("_" + m_name);
    if (format == RRParam::FORMAT_PARAM::BLOCK_NAME) return paramName;
    if (m_index != -1 && format != RRParam::FORMAT_PARAM::BLOCK_NAME_LEN) paramName.append("[" + QString::number(m_index) + "]");
    if (format == RRParam::FORMAT_PARAM::BLOCK_NAME_INDEX) return paramName;
    if (m_len != -1 && format != RRParam::FORMAT_PARAM::BLOCK_NAME_INDEX) paramName.append("(" + QString::number(m_len) + ")");
    return paramName;
}

QStringList RRParam::getAllBlockName(QString *errorMessage){
    QString queryString = (QString("SELECT DISTINCT Bl_Name FROM RR_PAR ORDER BY Bl_Name"));
    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        if (errorMessage != nullptr) *errorMessage = ("ОШИБКА ОБРАЩЕНИЯ К БД: " + query.lastError().text());
        return QStringList();
    }
    QStringList result;
    while (query.next()){
        QString blockName = query.value(0).toString();
        result << blockName;
    }
    return result;
}

QStringList RRParam::getAllParamInBlock(const QString &blockName, QString *errorMessage){
    QString queryString = (QString("SELECT DISTINCT Par_Name FROM RR_PAR WHERE Bl_Name = '%1' ORDER BY Par_Name").arg(blockName));
    QSqlQuery query = MainWindow::getQueryRRDB(queryString);
    if (!query.isActive()){
        if (errorMessage != nullptr) *errorMessage = ("ОШИБКА ОБРАЩЕНИЯ К БД: " + query.lastError().text());
        return QStringList();
    }
    QStringList result;
    while (query.next()){
        QString parName = query.value(0).toString();
        result << parName;
    }
    return result;
}


RRParam::FORMAT_PARAM RRParam::getFormatParam(const QString &param){
    QRegularExpression regex(RR_PARAM_VARIABLE_TEMPLATE);
    QRegularExpressionMatch match = regex.match(param);
    if (!match.hasMatch()){
        return RRParam::FORMAT_PARAM::EMPTY;
    }
    RRParam::FORMAT_PARAM format = RRParam::FORMAT_PARAM::BLOCK;

    if (!match.captured(2).isEmpty()){
        format = RRParam::FORMAT_PARAM::BLOCK_NAME;
    }

    if (!match.captured(3).isEmpty()){
        if (format == RRParam::FORMAT_PARAM::BLOCK_NAME) format = RRParam::FORMAT_PARAM::BLOCK_NAME_INDEX;
        else return RRParam::FORMAT_PARAM::EMPTY;
    }
    if (!match.captured(4).isEmpty()){
        if (format == RRParam::FORMAT_PARAM::BLOCK_NAME) format = RRParam::FORMAT_PARAM::BLOCK_NAME_LEN;
        else if (format == RRParam::FORMAT_PARAM::BLOCK_NAME_INDEX) format = RRParam::FORMAT_PARAM::FULL;
        else return RRParam::FORMAT_PARAM::EMPTY;
    }
    return format;
}

QStringList RRParam::parseParam(const QString &param){
    QRegularExpression regex(RR_PARAM_VARIABLE_TEMPLATE);
    QRegularExpressionMatch match = regex.match(param);
    QStringList result({"", "", "", ""});
    if (!match.hasMatch()){
        return result;
    }
    if (!match.captured(1).isEmpty()){
        result[0] = match.captured(1);
    }
    if (!match.captured(2).isEmpty()){
        result[1] = match.captured(2);
    }
    if (!match.captured(3).isEmpty()){
        result[2] = match.captured(3);
    }
    if (!match.captured(4).isEmpty()){
        result[3] = match.captured(4);
    }

    return result;
}
