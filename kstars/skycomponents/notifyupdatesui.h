#ifndef NOTIFYUPDATESUI_H
#define NOTIFYUPDATESUI_H

#include "skyobjects/skyobject.h"

#include <QDialog>

namespace Ui {
    class NotifyUpdatesUI;
}

class NotifyUpdatesUI : public QDialog
{
    Q_OBJECT

public:
    explicit NotifyUpdatesUI(QWidget *parent = 0);
    ~NotifyUpdatesUI();
    void addItems(QList<SkyObject*> updatesList);

private:
    Ui::NotifyUpdatesUI *ui;
};

#endif // NOTIFYUPDATESUI_H
