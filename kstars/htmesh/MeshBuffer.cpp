#include <iostream>
#include <cstdlib>

#include "HTMesh.h"
#include "MeshBuffer.h"

MeshBuffer::MeshBuffer(HTMesh *mesh) {

    m_size= 0;
    m_error = 0;
    maxSize = mesh->size();
    m_buffer = (Trixel*) malloc( sizeof(Trixel) * maxSize );

    if (m_buffer == NULL) {

        fprintf(stderr, "MeshBuffer: Could not allocate buffer sized %d\n",
                maxSize * (int) sizeof(Trixel) );
        exit(1);
    }
}

MeshBuffer::~MeshBuffer() {
    free(m_buffer);
}

int MeshBuffer::append(Trixel trixel)
{
    if (m_size >= maxSize) {
        m_error++;
        return 0;
    }
    m_buffer[m_size++] = trixel;
    return 1;
}

void MeshBuffer::fill() {
    for (Trixel i = 0; i < (unsigned int)maxSize; i++) {
        m_buffer[i] = i;
    }
    m_size = maxSize;
}
