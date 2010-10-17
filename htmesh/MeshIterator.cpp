#include <iostream>

#include "HTMesh.h"
#include "MeshBuffer.h"
#include "MeshIterator.h"

MeshIterator::MeshIterator(HTMesh *mesh, BufNum bufNum)
{
    cnt = 0;
    MeshBuffer* buffer = mesh->meshBuffer( bufNum );
    m_size = buffer->size();
    index = buffer->buffer();
}

