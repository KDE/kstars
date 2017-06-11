#ifndef OPSASTROMETRYCFG_H
#define OPSASTROMETRYCFG_H

#include <QDialog>
#include "ui_opsastrometrycfg.h"

class KConfigDialog;

namespace Ekos
{
class Align;

class OpsAstrometryCfg : public QDialog, public Ui::OpsAstrometryCfg
{
    Q_OBJECT

  public:
    explicit OpsAstrometryCfg(Align *parent);
    ~OpsAstrometryCfg();

  private slots:
    void slotLoadCFG();
    void slotApply();
    void slotCFGEditorUpdated();

  private:
    KConfigDialog *m_ConfigDialog;
    Align *alignModule;
    QString currentCFGText;
};
}

#endif // OPSASTROMETRYCFG_H
