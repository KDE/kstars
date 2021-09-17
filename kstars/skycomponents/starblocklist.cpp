/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "starblocklist.h"

#include "binfilehelper.h"
#include "deepstarcomponent.h"
#include "starblock.h"
#include "starcomponent.h"

#ifdef KSTARS_LITE
#include "skymaplite.h"
#include "kstarslite/skyitems/skynodes/pointsourcenode.h"
#endif

#include <QDebug>

StarBlockList::StarBlockList(const Trixel &tr, DeepStarComponent *parent)
{
    trixel       = tr;
    this->parent = parent;
    staticStars  = parent->hasStaticStars();
}

int StarBlockList::releaseBlock(StarBlock *block)
{
    if (block != blocks[nBlocks - 1].get())
        qDebug() << "ERROR: Trying to release a block which is not the last block! Trixel = " << trixel;

    else if (blocks.size() > 0)
    {
#ifdef KSTARS_LITE
        for (int i = 0; i < block->getStarCount(); ++i)
        {
            PointSourceNode *node = block->star(i)->starNode;
            if (node)
            {
                SkyMapLite::Instance()->deleteSkyNode(node);
                block->star(i)->starNode = 0;
            }
        }
#endif
        blocks.removeLast();
        nBlocks--;
        nStars -= block->getStarCount();

        readOffset -= parent->getStarReader()->guessRecordSize() * block->getStarCount();
        if (nBlocks <= 0)
            faintMag = -5.0;
        else
            faintMag = blocks[nBlocks - 1]->faintMag;

        return 1;
    }

    return 0;
}

bool StarBlockList::fillToMag(float maglim)
{
    // TODO: Remove staticity of BinFileHelper
    BinFileHelper *dSReader;
    StarBlockFactory *SBFactory;
    StarData stardata;
    DeepStarData deepstardata;
    FILE *dataFile;

    dSReader  = parent->getStarReader();
    dataFile  = dSReader->getFileHandle();
    SBFactory = StarBlockFactory::Instance();

    if (staticStars)
        return false;

    if (faintMag >= maglim)
        return true;

    if (!dataFile)
    {
        qDebug() << "dataFile not opened!";
        return false;
    }

    Trixel trixelId =
        trixel; //( ( trixel < 256 ) ? ( trixel + 256 ) : ( trixel - 256 ) ); // Trixel ID on datafile is assigned differently

    if (readOffset <= 0)
        readOffset = dSReader->getOffset(trixelId);

    Q_ASSERT(nBlocks == (unsigned int)blocks.size());

    BinFileHelper::unsigned_KDE_fseek(dataFile, readOffset, SEEK_SET);

    /*
    qDebug() << "Reading trixel" << trixel << ", id on disk =" << trixelId << ", currently nStars =" << nStars
             << ", record count =" << dSReader->getRecordCount( trixelId ) << ", first block = " << blocks[0]->getStarCount()
             << "to maglim =" << maglim << "with current faintMag =" << faintMag;
    */

    while (maglim >= faintMag && nStars < dSReader->getRecordCount(trixelId))
    {
        int ret = 0;

        if (nBlocks == 0 || blocks[nBlocks - 1]->isFull())
        {
            std::shared_ptr<StarBlock> newBlock = SBFactory->getBlock();

            if (!newBlock.get())
            {
                qWarning() << "ERROR: Could not get a new block from StarBlockFactory::getBlock() in trixel " << trixel
                           << ", while trying to create block #" << nBlocks + 1;
                return false;
            }
            blocks.append(newBlock);
            blocks[nBlocks]->parent = this;
            if (nBlocks == 0)
                SBFactory->markFirst(blocks[0]);
            else if (!SBFactory->markNext(blocks[nBlocks - 1], blocks[nBlocks]))
                qWarning() << "ERROR: markNext() failed on block #" << nBlocks + 1 << "in trixel" << trixel;

            ++nBlocks;
        }
        // TODO: Make this more general
        if (dSReader->guessRecordSize() == 32)
        {
            ret = fread(&stardata, sizeof(StarData), 1, dataFile);
            if (dSReader->getByteSwap())
                DeepStarComponent::byteSwap(&stardata);
            readOffset += sizeof(StarData);
            blocks[nBlocks - 1]->addStar(stardata);
        }
        else
        {
            ret = fread(&deepstardata, sizeof(DeepStarData), 1, dataFile);
            if (dSReader->getByteSwap())
                DeepStarComponent::byteSwap(&deepstardata);
            readOffset += sizeof(DeepStarData);
            blocks[nBlocks - 1]->addStar(deepstardata);
        }

        /*
          if( faintMag > -5.0 && fabs(faintMag - blocks[nBlocks - 1]->getFaintMag()) > 0.2 ) {
          qDebug() << "Encountered a jump from mag" << faintMag << "to mag"
          << blocks[nBlocks - 1]->getFaintMag() << "in trixel" << trixel;
          }
        */
        faintMag = blocks[nBlocks - 1]->getFaintMag();
        nStars++;
    }

    return ((maglim < faintMag) ? true : false);
}

void StarBlockList::setStaticBlock(std::shared_ptr<StarBlock> &block)
{
    if (!block)
        return;
    if (nBlocks == 0)
    {
        blocks.append(block);
    }
    else
        blocks[0] = block;

    blocks[0]->parent = this;
    faintMag          = blocks[0]->faintMag;
    nBlocks           = 1;
    staticStars       = true;
}
