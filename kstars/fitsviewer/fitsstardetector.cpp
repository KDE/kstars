#include "fitsstardetector.h"

FITSStarDetector& FITSStarDetector::configure(QStandardItemModel const &settings)
{
    Q_ASSERT(2 <= settings.columnCount());

    for (int row = 0; row < settings.rowCount(); row++)
        configure(settings.item(row, 0)->data().toString(), settings.item(row, 1)->data());

    return *this;
}
