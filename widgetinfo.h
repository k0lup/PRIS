#ifndef WIDGETINFO_H
#define WIDGETINFO_H
#include <QtWidgets>

class WidgetInfo : public QWidget
{
    Q_OBJECT
public:
    WidgetInfo(QWidget *parent = nullptr);
    void setTitle(const QString& title) {this->title = title;};
    QString getTitle() {return title;}
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QString title;
};

#endif // WIDGETINFO_H
