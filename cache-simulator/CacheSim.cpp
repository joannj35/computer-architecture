/* 046267 Computer Architecture - Winter 20/21 - HW #2 */

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include "cache.h"
#include <cmath>

using std::FILE;
using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::ifstream;
using std::stringstream;

enum writePolicy { writeAllocate = 0, NoWriteAllocate = 1 };

int main(int argc, char** argv) {

    if (argc < 19) {
        cerr << "Not enough arguments" << endl;
        return 0;
    }

    // Get input arguments
    // File
    // Assuming it is the first argument
    char* fileString = argv[1];
    ifstream file(fileString); //input file stream
    string line;
    if (!file || !file.good()) {
        // File doesn't exist or some other error
        cerr << "File not found" << endl;
        return 0;
    }

    unsigned MemCyc = 0, BSize = 0, L1Size = 0, L2Size = 0, L1Assoc = 0,
        L2Assoc = 0, L1Cyc = 0, L2Cyc = 0, WrAlloc = 0;

    for (int i = 2; i < 19; i += 2) {
        string s(argv[i]);
        if (s == "--mem-cyc") {
            MemCyc = atoi(argv[i + 1]);
        }
        else if (s == "--bsize") {
            BSize = atoi(argv[i + 1]);
        }
        else if (s == "--l1-size") {
            L1Size = atoi(argv[i + 1]);
        }
        else if (s == "--l2-size") {
            L2Size = atoi(argv[i + 1]);
        }
        else if (s == "--l1-cyc") {
            L1Cyc = atoi(argv[i + 1]);
        }
        else if (s == "--l2-cyc") {
            L2Cyc = atoi(argv[i + 1]);
        }
        else if (s == "--l1-assoc") {
            L1Assoc = atoi(argv[i + 1]);
        }
        else if (s == "--l2-assoc") {
            L2Assoc = atoi(argv[i + 1]);
        }
        else if (s == "--wr-alloc") {
            WrAlloc = atoi(argv[i + 1]);
        }
        else {
            cerr << "Error in arguments" << endl;
            return 0;
        }
    }

    int l1_sets_bits_num = L1Size - BSize - L1Assoc;
    int l1_sets_num = (int)pow(2, l1_sets_bits_num);
    int l2_sets_bits_num = L2Size - BSize - L2Assoc;
    int l2_sets_num = (int)pow(2, l2_sets_bits_num);
    int l1_ways_num = (int)pow(2, L1Assoc);
    int l2_ways_num = (int)pow(2, L2Assoc);

    cacheLevel l1 = cacheLevel(l1_ways_num, l1_sets_num, L1Cyc);
    cacheLevel l2 = cacheLevel(l2_ways_num, l2_sets_num, L2Cyc);

    int commands_count = 0;
    int cycles_total_count = 0;
    int l1_access_count = 0;
    int l2_access_count = 0;

    while (getline(file, line)) {

        commands_count++;
        stringstream ss(line);
        string address;
        char operation = 0; // read (R) or write (W)
        if (!(ss >> operation >> address)) {
            // Operation appears in an Invalid format
            cout << "Command Format error" << endl;
            return 0;
        }

        // DEBUG - remove this line
        //cout << "operation: " << operation;

        string cutAddress = address.substr(2); // Removing the "0x" part of the address

        // DEBUG - remove this line
        //cout << ", address (hex)" << cutAddress;

        unsigned long int num = 0;
        num = strtoul(cutAddress.c_str(), NULL, 16);

        // DEBUG - remove this line
        //cout << " (dec) " << num << endl;

        unsigned int l1tag = num >> (l1_sets_bits_num + BSize);
        unsigned int l2tag = num >> (l2_sets_bits_num + BSize);
        unsigned temp_1 = num >> BSize;// remove off set from command
        int temp_pow = (int)pow(2, 32 - l1_sets_bits_num);
        unsigned temp_2 = temp_1 * temp_pow;
        unsigned command_set_l1 = temp_2 / temp_pow;
        temp_1 = num >> BSize;
        temp_pow = (int)pow(2, 32 - l2_sets_bits_num);
        temp_2 = temp_1 * temp_pow;
        unsigned command_set_l2 = temp_2 / temp_pow;

        if (l1_sets_bits_num == 0){
            command_set_l1 = 0;
        }
        if (l2_sets_bits_num == 0){
            command_set_l2 = 0;
        }

        if (operation == 'r') {
            bool read_result_l1 = l1.checkIfReadHit(command_set_l1, l1tag);
            cycles_total_count += L1Cyc;
            if (!read_result_l1){//read miss in l1
                l2_access_count++;
                bool read_result_l2 = l2.checkIfReadHit(command_set_l2, l2tag);
                cycles_total_count += L2Cyc;
                if (read_result_l2){ // read l2 hit , bring to l1
                    int empty_way = l1.getEmptyWay(command_set_l1);
                    if (empty_way != -1){// there is an empty way
                        l1.insertBlock(command_set_l1, empty_way, l1tag, false);
                    }
                    else{// there is no empty way
                        unsigned int writeBackTag;
                        if (l1.evictBlock(command_set_l1, writeBackTag, empty_way)){
                            // the block that we are deleting is dirty,therefore we need to update it in l2
                            unsigned int temp = writeBackTag << l1_sets_bits_num;
                            temp = temp | command_set_l1;
                            unsigned int l2_tag = temp >> l2_sets_bits_num;
                            int l2_set = temp << (32 - l2_sets_bits_num);
                            l2_set = l2_set >> (32 - l2_sets_bits_num);
                            l2.writeBack(l2_set, l2_tag);
                            l1.insertBlock(command_set_l1, empty_way, l1tag, false);
                        }
                    }
                }
                else { // read l2 miss  (and read l1 miss)
                    // write to l2
                    cycles_total_count += MemCyc;
                    int empty_way = l2.getEmptyWay(command_set_l2);
                    if (empty_way != -1){// there is an empty way in l2
                        l2.insertBlock(command_set_l2, empty_way, l2tag, false);
                    }
                    else{// there is no empty way in l2
                        unsigned int writeBackTag;
                        if (l2.evictBlock(command_set_l2, writeBackTag, empty_way)){
                            // the block that we are deleting is dirty,therefore we need to update it in memory
                            unsigned int temp = writeBackTag << l2_sets_bits_num;
                            temp = temp | command_set_l2;
                            unsigned int l1_tag = temp >> l1_sets_bits_num;
                            int l1_set = temp << (32 - l1_sets_bits_num);
                            l1_set = l1_set >> (32 - l1_sets_bits_num);
                            l1.snoop(l1_set, l1_tag);
                        }
                    }
                    // write in l1
                    empty_way = l1.getEmptyWay(command_set_l1);
                    if (empty_way != -1){// there is an empty way
                        l1.insertBlock(command_set_l1, empty_way, l1tag, false);
                    }
                    else{// there is no empty way{
                        unsigned int writeBackTag;
                        if (l1.evictBlock(command_set_l1, writeBackTag, empty_way)){
                            // the block that we are deleting is dirty,therefore we need to update it in l2
                            unsigned int temp = writeBackTag << l1_sets_bits_num;
                            temp = temp | command_set_l1;
                            unsigned int l2_tag = temp >> l2_sets_bits_num;
                            int l2_set = temp << (32 - l2_sets_bits_num);
                            l2_set = l2_set >> (32 - l2_sets_bits_num);
                            l2.writeBack(l2_set, l2_tag);
                            l1.insertBlock(command_set_l1, empty_way, l1tag, false);
                        }
                    }
                }
            }
        }
        if (operation == 'w'){
            cycles_total_count += L1Cyc;
            bool write_result_l1 = l1.checkIfWriteHit(command_set_l1, l1tag);
            if (write_result_l1) {
                l1.writeBack(command_set_l1, l1tag);
            }
            else if (!write_result_l1 && WrAlloc == 1) {// if write l1 miss
                l2_access_count++;
                bool read_result_l2 = l2.checkIfReadHit(command_set_l2, l2tag);
                cycles_total_count += L2Cyc;
                if (read_result_l2) {// if write l2 hit amd write allocate
                    int empty_way = l1.getEmptyWay(command_set_l1);
                    if (empty_way != -1){// there is an empty way
                        l1.insertBlock(command_set_l1, empty_way, l1tag, true);
                    }
                    else{// there is no empty way
                        unsigned int writeBackTag;
                        if (l1.evictBlock(command_set_l1, writeBackTag, empty_way)){
                            // the block that we are deleting is dirty,therefore we need to update it in l2
                            unsigned int temp = writeBackTag * ((int)pow(2, l1_sets_bits_num));
                            temp = temp | command_set_l1;
                            unsigned int l2_tag = temp / ((int)pow(2, l2_sets_bits_num));
                            unsigned int l2_set = temp * ((int)pow(2, (32 - l2_sets_bits_num)));
                            l2_set = l2_set / ((int)pow(2, (32 - l2_sets_bits_num)));

                            l2.writeBack(l2_set, l2_tag);
                            l1.insertBlock(command_set_l1, empty_way, l1tag, true);
                        }
                    }
                }
                else {// l2 miss, add to l1 and l2
                    // add l2
                    cycles_total_count += MemCyc;
                    int empty_way = l2.getEmptyWay(command_set_l2);
                    if (empty_way != -1){// there is an empty way in l2
                        l2.insertBlock(command_set_l2, empty_way, l2tag, false);
                    }
                    else{// there is no empty way in l2
                        unsigned int writeBackTag;
                        if (l2.evictBlock(command_set_l2, writeBackTag, empty_way)){
                            // the block that we are deleting is dirty,therefore we need to update it in memory
                            unsigned int temp = writeBackTag << l2_sets_bits_num;
                            temp = temp | command_set_l2;
                            unsigned int l1_tag = temp >> l1_sets_bits_num;
                            int l1_set = temp << (32 - l1_sets_bits_num);
                            l1_set = l1_set >> (32 - l1_sets_bits_num);
                            l1.snoop(l1_set, l1_tag);
                        }
                        l2.insertBlock(command_set_l2, empty_way, l2tag, false);
                    }
                    // add to l1
                    empty_way = l1.getEmptyWay(command_set_l1);
                    if (empty_way != -1){// there is an empty way
                        l1.insertBlock(command_set_l1, empty_way, l1tag, false);
                    }
                    else{// there is no empty way
                        unsigned int writeBackTag;
                        if (l1.evictBlock(command_set_l1, writeBackTag, empty_way)){
                            // the block that we are deleting is dirty,therefore we need to update it in l2
                            unsigned int temp = writeBackTag << l1_sets_bits_num;
                            temp = temp | command_set_l1;
                            unsigned int l2_tag = temp >> l2_sets_bits_num;
                            int l2_set = temp << (32 - l2_sets_bits_num);
                            l2_set = l2_set >> (32 - l2_sets_bits_num);
                            l2.writeBack(l2_set, l2_tag);
                            l1.insertBlock(command_set_l1, empty_way, l1tag, false);
                        }
                    }
                }
            }
            else if (!write_result_l1 && WrAlloc == 0) {
                cycles_total_count += L2Cyc;
                if (!(l2.checkIfWriteHit(command_set_l2, l2tag))) {
                    cycles_total_count += MemCyc;
                }
            }
        }
    }

    l1_access_count = commands_count;
    double L1MissRate = (double)l1.miss / (double)l1_access_count;
    double L2MissRate = (double)l2.miss / (double)l2_access_count;
    double avgAccTime = (double)cycles_total_count / (double)commands_count;

    printf("L1miss=%.03f ", L1MissRate);
    printf("L2miss=%.03f ", L2MissRate);
    printf("AccTimeAvg=%.03f\n", avgAccTime);

    return 0;
}
