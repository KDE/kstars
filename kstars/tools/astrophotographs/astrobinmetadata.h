#ifndef ASTROBINMETADATA_H
#define ASTROBINMETADATA_H

#include <QString>

class AstroBinMetadata
{
    friend class AstroBinApiJson;
    friend class AstroBinApiXml;

public:
    int resultLimit() const { return m_Limit; }
    QString nextUrlPostfix() const { return m_NextUrlPostfix; }
    QString previousUrlPostfix() const { return m_PreviousUrlPostfix; }
    int resultOffset() const { return m_Offset; }
    int resultTotalCount() const { return m_TotalResultCount; }

private:
    int m_Limit;
    QString m_NextUrlPostfix;
    QString m_PreviousUrlPostfix;
    int m_Offset;
    int m_TotalResultCount;
};

#endif // ASTROBINMETADATA_H
