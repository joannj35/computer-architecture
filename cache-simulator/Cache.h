
#ifndef CACHE_H
#define CACHE_H
#include <iostream>
#include <vector>
#include <map>


class setEntry {
public:
    unsigned int tag;
    bool valid;
    bool dirty;
    setEntry() : tag(0), valid(false), dirty(false) {};
    ~setEntry() = default;
    void setTag(unsigned int new_tag)// use just if valid is false
    {
        tag = new_tag;
        valid = true;
    }
    void changeTage(unsigned int new_tag)
    {
        dirty = true;
        tag = new_tag;
        valid = true;
    }
};

class set {
public:
    int setNum;
    int waysNum;
    std::vector<setEntry> ways;
    std::vector<int> lru;

    set(int set, int ways_num) : setNum(set), waysNum(ways_num) {
        for (int i = 0; i < ways_num; i++) {
            ways.push_back(setEntry());
            lru.push_back(i);
        }
    }

    ~set() = default;
    void updateLru(int way);
    int evictWay();
};


class cacheLevel {
public:
    int waysNum;
    int setsNum;
    int access_time;
    int hits;
    int miss;
    std::map<int, set*> setsMap;

    cacheLevel(int ways, int sets, int time) : waysNum(ways), setsNum(sets), access_time(time), hits(0), miss(0){
        for (int i = 0; i < setsNum; i++){
            setsMap[i] = new set(i, waysNum);
        }
    }

    ~cacheLevel(){
        for (int i = 0; i < setsNum; i++) {
            delete setsMap[i];
        }
    }
    bool checkIfReadHit(int set, unsigned int tag); //return true if hit , false if miss
    bool checkIfWriteHit(int set, unsigned int tag);//return true if hit and updates block , false if miss
    void insertBlock(int set, int way, unsigned int tag, bool dirty);
    bool evictBlock(int set, unsigned int& writeBackTag, int& wayToEvict);
    int  getEmptyWay(int set);
    void writeBack(int set, unsigned int tag);
    void snoop(int set, unsigned int tag);
};

#endif //CACHE_H
