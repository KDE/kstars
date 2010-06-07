
#include "dirtyuglyhack.h"
#include "skypainter.h"

SkyPainter* DirtyUglyHack::m_p = 0;

SkyPainter* DirtyUglyHack::painter()
{
    return m_p;
}

void DirtyUglyHack::setPainter(SkyPainter* p)
{
    m_p = p;
}
