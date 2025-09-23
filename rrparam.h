#ifndef RRPARAM_H
#define RRPARAM_H
#include <QtWidgets>

class RRParam
{
public:
    enum class FORMAT_PARAM{
        FULL,
        BLOCK_NAME_INDEX,
        BLOCK_NAME_LEN,
        BLOCK_NAME,
        BLOCK,
        EMPTY
    };
    enum class ERROR_CODE{
        DB_QUERY_ERROR,
        SYNTAX_ERROR,
        CONTAINS_ERROR,
        GOOD_STATE
    };


    RRParam(const QString &paramName);
    bool isExist();
    bool isValid();
    double getValue(bool *status = nullptr);
    bool setValue(const double value);

    int getLength();
    bool isArray();

    bool create(const double value = 0);
    bool remove();

    static bool removeBlock(const QString& blockName, QString *errorMessage = nullptr);
    static QStringList getAllBlockName(QString *errorMessage = nullptr);
    static QStringList getAllParamInBlock(const QString &blockName, QString *errorMessage = nullptr);

    static FORMAT_PARAM getFormatParam(const QString& param);
    static QStringList parseParam(const QString& param); //возвращаем массив строк, где [0] - имя блока; [1] - имя параметра; [2] - индекс; [3] - длина. (если что-то отстутвует, то будет пустая строка)




    //double getMin(bool &status/*, int beginIndex = 0, int len = -1*/);
    //double getMax(bool &status/*, int beginIndex = 0, int len = -1*/);

    QString getErrorText();
    bool isHasError();
    ERROR_CODE getErrorCode();

    void setBlockName(const QString& blockName);
    void setParamName(const QString& paramName);
    void setIndex(const int index);
    void setLen(const int len);

    QString getBlockName();
    QString getParamName();
    int getIndex();
    int getLen();

    QString getFullParamName(FORMAT_PARAM format = FORMAT_PARAM::FULL);

    void resetError();


private:
    QString m_block;
    QString m_name;
    int m_index;
    int m_len;

    bool m_hasError;
    QString m_errorMessage;

    ERROR_CODE m_errorCode;
};

#endif // RRPARAM_H
