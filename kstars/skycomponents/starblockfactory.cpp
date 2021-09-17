/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "starblockfactory.h"

#include "starblock.h"
#include "starobject.h"

#include <kstars_debug.h>

// TODO: Implement a better way of deciding this
#define DEFAULT_NCACHE 12

StarBlockFactory *StarBlockFactory::pInstance = nullptr;

StarBlockFactory *StarBlockFactory::Instance()
{
    if (!pInstance)
        pInstance = new StarBlockFactory();
    return pInstance;
}

StarBlockFactory::StarBlockFactory()
{
    first   = nullptr;
    last    = nullptr;
    nBlocks = 0;
    drawID  = 0;
    nCache  = DEFAULT_NCACHE;
}

StarBlockFactory::~StarBlockFactory()
{
    deleteBlocks(nBlocks);
    if (pInstance)
        pInstance = nullptr;
}

std::shared_ptr<StarBlock> StarBlockFactory::getBlock()
{
    std::shared_ptr<StarBlock> freeBlock;

    if (nBlocks < nCache)
    {
        freeBlock.reset(new StarBlock);
        if (freeBlock.get())
        {
            ++nBlocks;
            return freeBlock;
        }
    }
    if (last && (last->drawID != drawID || last->drawID == 0))
    {
        //        qCDebug(KSTARS) << "Recycling block with drawID =" << last->drawID << "and current drawID =" << drawID;
        if (last->parent->block(last->parent->getBlockCount() - 1) != last)
            qCDebug(KSTARS) << "ERROR: Goof up here!";
        freeBlock = last;
        last      = last->prev;
        if (last)
        {
            last->next = nullptr;
        }
        if (freeBlock == first)
        {
            first = nullptr;
        }
        freeBlock->reset();
        freeBlock->prev = nullptr;
        freeBlock->next = nullptr;
        return freeBlock;
    }
    freeBlock.reset(new StarBlock);
    if (freeBlock.get())
        ++nBlocks;

    return freeBlock;
}

bool StarBlockFactory::markFirst(std::shared_ptr<StarBlock>& block)
{
    if (!block.get())
        return false;

    //    fprintf(stderr, "markFirst()!\n");
    if (!first)
    {
        //        qCDebug(KSTARS) << "INFO: Linking in first block";
        last = first = block;
        first->prev = first->next = nullptr;
        first->drawID             = drawID;
        return true;
    }

    if (block == first) // Block is already in the front
    {
        block->drawID = drawID;
        return true;
    }

    if (block == last)
        last = block->prev;

    if (block->prev)
        block->prev->next = block->next;

    if (block->next)
        block->next->prev = block->prev;

    first->prev = block;
    block->next = first;
    block->prev = nullptr;
    first       = block;

    block->drawID = drawID;

    return true;
}

bool StarBlockFactory::markNext(std::shared_ptr<StarBlock>& after, std::shared_ptr<StarBlock>& block)
{
    //    fprintf(stderr, "markNext()!\n");
    if (!block.get() || !after.get())
    {
        qCDebug(KSTARS) << "WARNING: markNext called with nullptr argument";
        return false;
    }

    if (!first.get())
    {
        qCDebug(KSTARS) << "WARNING: markNext called without an existing linked list";
        return false;
    }

    if (block == after)
    {
        qCDebug(KSTARS) << "ERROR: Trying to mark a block after itself!";
        return false;
    }

    if (block->prev == after) // Block is already after 'after'
    {
        block->drawID = drawID;
        return true;
    }

    if (block == first)
    {
        if (block->next == nullptr)
        {
            qCDebug(KSTARS) << "ERROR: Trying to mark only block after some other block";
            return false;
        }
        first = block->next;
    }

    if (after->getFaintMag() > block->getFaintMag() && block->getFaintMag() != -5)
    {
        qCDebug(KSTARS) << "WARNING: Marking block with faint mag = " << block->getFaintMag() << " after block with faint mag "
                 << after->getFaintMag() << "in trixel" << block->parent->getTrixel();
    }

    if (block == last)
        last = block->prev;

    if (block->prev)
        block->prev->next = block->next;
    if (block->next)
        block->next->prev = block->prev;

    block->next = after->next;
    if (block->next)
        block->next->prev = block;
    block->prev = after;
    after->next = block;

    if (after == last)
        last = block;

    block->drawID = drawID;

    return true;
}

/*
bool StarBlockFactory::groupMove( StarBlock *start, const int nblocks ) {

    StarBlock * end = nullptr;

    // Check for trivial cases
    if( !start || nblocks < 0 )
        return false;

    if( nblocks == 0 )
        return true;

    if( !first )
        return false;

    // Check for premature end
    end = start;
    for( int i = 1; i < nblocks; ++i ) {
        if( end == nullptr )
            return false;
        end = end->next;
    }
    if( end == nullptr )
        return false;

    // Update drawIDs
    end = start;
    for( int i = 1; i < nblocks; ++i ) {
        end->drawID = drawID;
        end = end->next;
    }
    end->drawID = drawID;

    // Check if we are already in the front
    if( !start->prev )
        return true;

    start->prev->next = end->next;
    end->next->prev = start->prev;

    first->prev = end;
    end->next = first;
    start->prev = nullptr;
    first = start;
}
*/

int StarBlockFactory::deleteBlocks(int nblocks)
{
    int i           = 0;
    std::shared_ptr<StarBlock> temp;

    while (last != nullptr && i != nblocks)
    {
        temp = last->prev;
        last.reset();
        last = temp;
        i++;
    }
    if (last)
        last->next = nullptr;
    else
        first = nullptr;

    qCDebug(KSTARS) << nblocks << "StarBlocks freed from StarBlockFactory";

    nBlocks -= i;
    return i;
}

void StarBlockFactory::printStructure() const
{
    std::shared_ptr<StarBlock> cur;
    Trixel curTrixel = 513; // TODO: Change if we change HTMesh level
    int index        = 0;
    bool draw        = false;

    cur = first;
    do
    {
        if (curTrixel != cur->parent->getTrixel())
        {
            qCDebug(KSTARS) << "Trixel" << cur->parent->getTrixel() << "starts at index" << index;
            curTrixel = cur->parent->getTrixel();
        }
        if (cur->drawID == drawID && !draw)
        {
            qCDebug(KSTARS) << "Blocks from index" << index << "are drawn";
            draw = true;
        }
        if (cur->drawID != drawID && draw)
        {
            qCDebug(KSTARS) << "Blocks from index" << index << "are not drawn";
            draw = false;
        }
        cur = cur->next;
        ++index;
    } while (cur != last);
}

int StarBlockFactory::freeUnused()
{
    int i           = 0;
    std::shared_ptr<StarBlock> temp;

    while (last != nullptr && last->drawID < drawID && i != nBlocks)
    {
        temp = last->prev;
        last.reset();
        last = temp;
        i++;
    }
    if (last)
        last->next = nullptr;
    else
        first = nullptr;

    qCDebug(KSTARS) << i << "StarBlocks freed from StarBlockFactory";

    nBlocks -= i;
    return i;
}
