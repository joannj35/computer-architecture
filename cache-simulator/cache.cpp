#include "cache.h"
#include <iostream>
using std::cout;
using std::endl;


void set::updateLru(int way) {
    int x = lru[way];
    lru[way] = waysNum - 1;
    for (int i = 0; i < waysNum; i++) {
        if (lru[i] > x && i != way) {
            lru[i]--;
        }
    }

}

int set::evictWay() {

    for (int i = 0; i < waysNum; i++) {
        if (lru[i] == 0)
        {
            return i;
        }
    }
    return 0;// should not reach
}

bool cacheLevel::checkIfReadHit(int set, unsigned int tag) {

    //cout << "in check if read hit" << endl;
    //cout << " ways num in check if read hit" << waysNum ; 


    for (int i = 0; i < waysNum; i++)
    {
        if (setsMap[set]->ways[i].valid)
        {
            if (setsMap[set]->ways[i].tag == tag)//read hit
            {
                setsMap[set]->updateLru(i);
                hits++;
                return true;
            }
        }
    }

    //read miss , must fetch block that contains missing data from memory
    miss++;
    return false;
}

bool cacheLevel::checkIfWriteHit(int set, unsigned int tag) {

    for (int i = 0; i < waysNum; i++)
    {
        if (setsMap[set]->ways[i].valid)
        {
            if (setsMap[set]->ways[i].tag == tag)//read hit
            {
                setsMap[set]->ways[i].dirty = true;
                setsMap[set]->updateLru(i);
                hits++;
                return true;
            }
        }
    }

    //write miss
    miss++;
    return false;

}

void cacheLevel::insertBlock(int set, int way, unsigned int tag, bool dirty) {

    setsMap[set]->ways[way].valid = true;
    setsMap[set]->ways[way].dirty = dirty;
    setsMap[set]->ways[way].tag = tag;
    setsMap[set]->updateLru(way);
}

bool cacheLevel::evictBlock(int set, unsigned int& writeBackTag, int& wayToEvict) {

    wayToEvict = setsMap[set]->evictWay();
    writeBackTag = setsMap[set]->ways[wayToEvict].tag;
    if (setsMap[set]->ways[wayToEvict].dirty)
    {
        return true;
    }
    return false;
}


int cacheLevel::getEmptyWay(int set) {

    for (int i = 0; i < waysNum; i++)
    {
        if (setsMap[set]->ways[i].valid == false)//valid == false
        {
            return i;
        }
    }
    return -1;// all ways are full.
}

// method used by L1 to write back to L2.
void cacheLevel::writeBack(int set, unsigned int tag) {

    for (int i = 0; i < waysNum; i++)
    {
        if (setsMap[set]->ways[i].valid && setsMap[set]->ways[i].tag == tag)
        {
            setsMap[set]->ways[i].dirty = true;
            setsMap[set]->updateLru(i);
        }
    }
}

void  cacheLevel::snoop(int set, unsigned int tag) {
    for (int i = 0; i < waysNum; i++)
    {
        if (setsMap[set]->ways[i].valid && setsMap[set]->ways[i].tag == tag)
        {
            setsMap[set]->ways[i].valid = false;

        }
    }

}
