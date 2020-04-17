#ifndef TESTEKOSWIZARD_H
#define TESTEKOSWIZARD_H

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

#endif // TESTEKOSWIZARD_H
