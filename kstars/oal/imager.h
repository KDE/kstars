#ifndef IMAGER_H
#define IMAGER_H

#include "oal.h"

class Imager
{
public:
    Imager(const QString &id, const QString &model, const QString &vendor, const QString &remarks)
    {
        setImager(id, model, vendor, remarks);
    }

    QString model() const { return m_Model; }
    QString vendor() const { return m_Vendor; }
    QString remarks() const { return m_Remarks; }

    void setImager(const QString &id, const QString &model, const QString &vendor, const QString &remarks);

private:
    QString m_Id;
    QString m_Model;
    QString m_Vendor;
    QString m_Remarks;
};

#endif // IMAGER_H
