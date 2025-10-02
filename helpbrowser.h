#ifndef HELPBROWSER_H
#define HELPBROWSER_H
#include <QTextBrowser>
#include <QHelpEngine>

class HelpBrowser : public QTextBrowser
{
public:
    HelpBrowser(QHelpEngine *helpEngine, QWidget *parent = nullptr);
    QVariant loadResource (int type, const QUrl& name) override;
private:
    QHelpEngine* helpEngine;
};

#endif // HELPBROWSER_H
