#include "widgetinfo.h"

WidgetInfo::WidgetInfo(QWidget *parent) : QWidget(parent)
{
    setContentsMargins(20, 20, 20, 20);
}

void WidgetInfo::paintEvent(QPaintEvent *event){
    QWidget::paintEvent(event);

    QPainter painter(this);

    QColor backgroundColor = this->palette().color(QPalette::Window);

    //QRect titleRect = this->geometry();
    //titleRect = titleRect.adjusted(20, 20, -20, -20);
    QRect titleRect = QRect(10, 10, this->width() - 20, this->height() - 20);

    QPen pen;
    pen.setColor(Qt::black);
    pen.setStyle(Qt::SolidLine);
    pen.setWidth(2);

    painter.setPen(pen);

    //painter.fillRect(titleRect, backgroundColor);
    painter.drawRect(titleRect);

    QFont font("Arial", 12, QFont::Bold);
    painter.setFont(font);
    QSize textSize = QFontMetrics(font).boundingRect(title + QString(" ")).size();
    //qDebug() << textSize;
    QRect textRect = QRect(30, 1, textSize.width(), textSize.height());
    //qDebug() << textRect;
    painter.fillRect(textRect, backgroundColor);
    painter.drawText(textRect, title);
}
