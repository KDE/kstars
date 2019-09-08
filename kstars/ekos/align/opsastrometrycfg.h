
#pragma once

#include "ui_opsastrometrycfg.h"

#include <QDialog>

class KConfigDialog;

namespace Ekos
{
class Align;

class OpsAstrometryCfg : public QDialog, public Ui::OpsAstrometryCfg
{
    Q_OBJECT

  public:
    explicit OpsAstrometryCfg(Align *parent);
    virtual ~OpsAstrometryCfg() override = default;

  protected:
    void showEvent(QShowEvent *) override;

  private slots:
    void slotLoadCFG();
    void slotAddAstrometryIndexFileLocation();
    void slotRemoveAstrometryIndexFileLocation();
    void slotClickAstrometryIndexFileLocation(QListWidgetItem *item);
    void slotApply();
    void slotCFGEditorUpdated();

  private:
    KConfigDialog *m_ConfigDialog { nullptr };
    Align *alignModule { nullptr };
    QString currentCFGText;
};
}
