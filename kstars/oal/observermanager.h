#ifndef OBSERVERMANAGER_H
#define OBSERVERMANAGER_H

#include "ui_observermanager.h"

class KStars;
class QStandardItemModel;


class ObserverManagerUi : public QFrame, public Ui::ObserverManager
{
    Q_OBJECT
public:
    explicit ObserverManagerUi(QWidget *parent = 0);
};


class ObserverManager : public KDialog
{
    Q_OBJECT
public:
    ObserverManager(QWidget *parent = 0);
    ~ObserverManager();

public slots:
    void addObserver();
    void saveChanges();
    void deleteObserver();
    void showObserver(QModelIndex idx);

private:
    void createModel();
    void saveChangesToObsList();

    ObserverManagerUi *mUi;

    KStars *mKstars;
    QStandardItemModel *mModel;
};

#endif // OBSERVERMANAGER_H
