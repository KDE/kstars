/****************************************************************************
** Form interface generated from reading ui file './indiconf.ui'
**
** Created: Sat Jan 31 22:34:05 2004
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef INDICONF_H
#define INDICONF_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QPushButton;
class QGroupBox;
class QCheckBox;

class INDIConf : public QDialog
{
    Q_OBJECT

public:
    INDIConf( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~INDIConf();

    QPushButton* buttonOk;
    QPushButton* buttonCancel;
    QGroupBox* displayGroup;
    QCheckBox* crosshairCheck;
    QCheckBox* messagesCheck;
    QGroupBox* autoGroup;
    QCheckBox* timeCheck;
    QCheckBox* GeoCheck;

protected:
    QGridLayout* INDIConfLayout;
    QHBoxLayout* Layout1;
    QVBoxLayout* displayGroupLayout;
    QVBoxLayout* layout2;
    QVBoxLayout* autoGroupLayout;
    QVBoxLayout* layout3;

protected slots:
    virtual void languageChange();

};

#endif // INDICONF_H
