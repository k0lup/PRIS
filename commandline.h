#ifndef COMMANDLINE_H
#define COMMANDLINE_H
#include <QtWidgets>

class CommandLine : public QWidget
{
    Q_OBJECT
public:
    CommandLine(QWidget *parent = nullptr);
    ~CommandLine();
    void clearText();
    void setText(const QString& text);
    void appendText(const QString& text);
    QString getText() {return lineEdit->text();}
    const QStringList& getHistory() {return histrory;}

    QString getPrevCommand();
    QString getNextCommand();

    void updateHistory(const QStringList& newHistory);

    bool hasCancelAction();
    bool hasSelectedText();

    void cancelLastAction();
    //QString getSelectedText();

    //void removeSelectedText();
    void selectedAll();

    QLineEdit* getCommandLineEdit();
signals:
    void commandSet(const QString& command);

    void historyUpdated();
protected:
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
private:
    QLineEdit *lineEdit;
    QStringList histrory;
    QListWidget *list;
    QMenu *menu;
    QToolButton *historyBtn;

    QSettings settings;

    int indexLastCommand;

    void updateListPos();
    void saveLastCommand();
    void loadLastCommand();

    QString lastTextForCancelAction;
    QString currentTextForCancelAction;
    bool isHasCancelAction;
};

#endif // COMMANDLINE_H
