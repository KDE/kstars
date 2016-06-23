#ifndef TYPEDEFLITE_H_
#define TYPEDEFLITE_H_
#include "typedef.h"
#include "skyopacitynode.h"

class SkyOpacityNode;

typedef SkyOpacityNode LabelTypeNode;
typedef SkyOpacityNode LineIndexNode; 

class TrixelNode : public SkyOpacityNode {
public:
    Trixel m_trixel;
};

#endif
