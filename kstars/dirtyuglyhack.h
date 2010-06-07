
#ifndef DIRTY_UGLY_HACK
#define DIRTY_UGLY_HACK

class SkyPainter;

class DirtyUglyHack
{
public:
    static SkyPainter* painter();
    static void setPainter(SkyPainter* p);
private:
    static SkyPainter *m_p;
};

#endif // DIRTY_UGLY_HACK