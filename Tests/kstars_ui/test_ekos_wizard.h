#ifndef TESTEKOSWIZARD_H
#define TESTEKOSWIZARD_H

#include "config-kstars.h"

#ifdef HAVE_INDI

#include <QObject>

class TestEkosWizard : public QObject
{
    Q_OBJECT

public:
    explicit TestEkosWizard(QObject *parent = nullptr);

private slots:
    void init();
    void cleanup();
    void testProfileWizard();

};

#endif // HAVE_INDI
#endif // TESTEKOSWIZARD_H
