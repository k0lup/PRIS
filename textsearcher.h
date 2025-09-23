#ifndef TEXTSEARCHER_H
#define TEXTSEARCHER_H
#include <QObject>
#include <QTextEdit>
#include <QRegularExpression>
#include <QMessageBox>

class TextSearcher : public QObject
{
    Q_OBJECT
public:
    explicit TextSearcher(QTextEdit *edit, QObject *parent = nullptr);

    void setSearchTerm(const QString& term, Qt::CaseSensitivity cs = Qt::CaseInsensitive);

public slots:
    void findNext();
    void findPrevious();

private:
    void highlightAll();
    void ensureVisible(const QTextCursor &c);
    void feedbackNotFound(bool backwards);

    QTextDocument::FindFlags makeFindFlags() const;

    QTextEdit *m_edit;
    QString m_term;
    Qt::CaseSensitivity m_cs = Qt::CaseInsensitive;

    QList<QTextEdit::ExtraSelection> m_extra;
    QTextCursor m_current;
};

#endif // TEXTSEARCHER_H
