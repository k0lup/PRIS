#ifndef CONSTVALUES_H
#define CONSTVALUES_H
#include <QtWidgets>
#include <QLocalSocket>

enum class NUCommand : unsigned char {
    PODKL_M = 0x01,
    PODKL_P = 0x02,
    ISM_NAPR = 0x06,
    ISM_SV = 0x08,
    ISM_SN = 0x09,
    SBR_PODKL = 0x0A,
    PODKL_100 = 0x0B,
    PODKSOED = 0x0D,
    PUCHOK = 0x20,
    SOPR_P = 0x21,
    PODKL_1M = 0x22,
    A_KONTR = 0x23

};

namespace constValues {
    extern const QMap<QString, QString> colorTranslate;
    extern const QMap<unsigned char, QString> NUDirectives;
    extern QAtomicInt isImitMode;
    extern QAtomicInt haveVnPr;
    extern QAtomicInt isNeedCheckKS;
}
/*
 * {0x0D, "ПОДКСОЕД"},
                                {0x0A, "СБР_ПОДКЛ"},
                                {0x01, "ПОДКЛ_М"},
                                {0x02, "ПОДКЛ_П"},
                                {0x09, "ИЗМ_СН"},
                                {0x08, "ИЗМ_СВ"},
                                {0x0B, "ПОДКЛ100"},
                                {0x06, "ИЗМ_НАПР"},
                                {0x20, "ПУЧОК"},
                                {0x21, "СОПР_П"},
                                {0x22, "ПОДК_1М"},
                                {0x23, "А_КОНТР"}
*/

enum class reactType{
    STOP,
    SLED,
    NO_REACT
};


QStringList getTimeWorkAppcp();
extern QLocalSocket *timeControlSocket;

extern QString ipAppcpServ;
extern int portAppcpWriteAndRead;
extern int portAppcpOnlyRead;


#endif // CONSTVALUES_H
