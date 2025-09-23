#ifndef DIALOGWGT_H
#define DIALOGWGT_H
#include <QtWidgets>

class DialogWgt : public QWidget
{
    Q_OBJECT
public:
    DialogWgt(bool autoClose = false, QWidget *parent = nullptr) : QWidget(parent), autoClose(autoClose), programClose(false) {}
    void setProgramCloseFlag(bool flag){this->programClose = flag;}
signals:
    void userCloseWindow();
private:
    bool programClose;
    bool autoClose;
protected:
    void closeEvent(QCloseEvent *event) override{
        if (this->programClose){
            event->accept();
        } else{
            emit this->userCloseWindow();
            if (!autoClose) event->ignore();
            else event->accept();
        }
        return;
    }
};

#endif // VARIANTDIALOGWGT_H
