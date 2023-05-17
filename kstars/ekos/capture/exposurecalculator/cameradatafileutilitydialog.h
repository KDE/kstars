#ifndef CAMERADATAFILEUTILITYDIALOG_H
#define CAMERADATAFILEUTILITYDIALOG_H

#include <QDialog>

namespace Ui {
class CameraDataFileUtilityDialog;
}

class CameraDataFileUtilityDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraDataFileUtilityDialog(QWidget *parent = nullptr);
    ~CameraDataFileUtilityDialog();

private:
    Ui::CameraDataFileUtilityDialog *ui;
};

#endif // CAMERADATAFILEUTILITYDIALOG_H
