#include "cameradatafileutilitydialog.h"
#include "ui_cameradatafileutilitydialog.h"

CameraDataFileUtilityDialog::CameraDataFileUtilityDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CameraDataFileUtilityDialog)
{
    ui->setupUi(this);
}

CameraDataFileUtilityDialog::~CameraDataFileUtilityDialog()
{
    delete ui;
}
