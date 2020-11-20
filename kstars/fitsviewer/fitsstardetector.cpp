#include "fitsstardetector.h"

//void FITSStarDetector::configure(QStandardItemModel const &settings)
//{
//    Q_ASSERT(2 <= settings.columnCount());

//    for (int row = 0; row < settings.rowCount(); row++)
//        configure(settings.item(row, 0)->data().toString(), settings.item(row, 1)->data());
//}

void FITSStarDetector::configure(const QString &key, const QVariant &value)
{
    m_Settings[key] = value;
}

QVariant FITSStarDetector::getValue(const QString &key, QVariant defaultValue) const
{
    if (m_Settings.contains(key))
        return m_Settings[key];
    else
        return defaultValue;
}
