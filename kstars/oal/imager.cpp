#include "imager.h"

void Imager::setImager(const QString &id, const QString &model, const QString &vendor, const QString &remarks)
{
    m_Id = id;
    m_Model = model;
    m_Vendor = vendor;
    m_Remarks = remarks;
}
