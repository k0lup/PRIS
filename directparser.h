#ifndef DIRECTPARSER_H
#define DIRECTPARSER_H
#include <QString>
#include <QMap>
#include <QList>
#include <QTextStream>

class DirectParser
{
public:
    enum class TypeDirect{
        COMMENT,
        A_KONTR,
        VARIANTK,
        VYBOR,
        VYBOR_100,
        VYSVAT,
        VYHOD,
        VYCHISL,
        DIRECT,
        ESLI_DA,
        ZAPROS,
        KPROGRAM,
        KSINONIM,
        NA,
        POVTOR,
        PODKL_1M,
        PODKSOED,
        PROVERKA,
        PROGRAM,
        PRC,
        PSI,
        PSC,
        PSC_R,
        PUSK,
        RVYHOD,
        RR_PAR,
        SINONIM,
        SOOBCH,
        SP,
        STOP,
        UV,
        PNC,
        PNC_R,
        NO_DIRECT
    };
    struct Direct{
      int numDirect;
      int numLine;
      QString metka;
      QString directive;
      TypeDirect direct;
      QList<QString> paramDirect;
      QList<QList<QList<QString>>> testParamDirect;
    };
    DirectParser();
    const QList<Direct*>& parseFile(const QString& fileName);
    const QList<Direct*>& parseString(const QString& strForParse);
    const QList<Direct*>& parseKO(const QString& KOString);
    static DirectParser::TypeDirect getDir(const QString& directive);
    static bool isOperatorDirect(TypeDirect dirType);
    static bool isTableDirect(TypeDirect dirType);
    static bool isVariantDirect(TypeDirect dirType);
private:
    bool isDirect;
    QList<Direct*> directives;
    bool parse(QTextStream& in);
};

#endif // DIRECTPARSER_H
