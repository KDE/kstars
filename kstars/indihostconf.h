/****************************************************************************
** Form interface generated from reading ui file './indihostconf.ui'
**
** Created: Sat Jan 31 22:34:05 2004
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef INDIHOSTCONF_H
#define INDIHOSTCONF_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;

class INDIHostConf : public QDialog
{
    Q_OBJECT

public:
    INDIHostConf( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~INDIHostConf();

    QLabel* textLabel1_3;
    QLabel* textLabel1;
    QLabel* textLabel1_2;
    QLineEdit* nameIN;
    QLineEdit* hostname;
    QLineEdit* portnumber;
    QPushButton* buttonOk;
    QPushButton* buttonCancel;

protected:
    QVBoxLayout* INDIHostConfLayout;
    QHBoxLayout* layout7;
    QVBoxLayout* layout6;
    QVBoxLayout* layout5;
    QHBoxLayout* Layout1;

protected slots:
    virtual void languageChange();

};

#endif // INDIHOSTCONF_H
