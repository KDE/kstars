#ifndef GENERICFILTERDATA_H
#define GENERICFILTERDATA_H
#include <QAbstractItemModel>

QT_BEGIN_NAMESPACE
namespace OptimalExposure
{

class GenericFilterData
{
    public:
        GenericFilterData();

        const QString &getFilterId() const;
        void setFilterId(const QString &newFilterId);
        const QString &getFilterDescription() const;
        void setFilterDescription(const QString &newFilterDescription);
        int getFilterBandPassWidth() const;
        void setFilterBandPassWidth(int newFilterBandPassWidth);

        double getFilterCompensation() const;

    private:
        QString filterId;
        QString filterDescription;
        int filterBandPassWidth;
};
}

#endif // GENERICFILTERDATA_H
