/* 046267 Computer Architecture- HW #1 */
/* This file holds the implementation of the predictor simulator */

#include "bp_api.h"
#include <iostream>
#include <cstdlib>
#include <math.h>

#define VALID_BIT_SIZE 1
#define TARGET_PC_SIZE 30
using namespace std;

enum FSM_STATE { SNT, WNT, WT, ST };
enum SHARE { NOT_USING_SHARE, USING_SHARE_LSB, USING_SHARE_MID };

uint32_t binaryToDecimal(bool* pc_arr, int len);
void Sim_stats_init();
void decimalToBinary(bool* arr, uint32_t num);

class BTB {
public:
    //structure made public for easier implementation (less set-get functions) 
    /*------------------------------------- BTB STRUCTURE -------------------------------------*/
    unsigned btb_size; //up to 32 bits
    unsigned history_size; //up to 8 bits
    unsigned tag_size; //tag field length 
    unsigned fsm_state; //initialized fsm state
    bool is_global_hist; //local or global history register
    bool is_global_table; // local or global fsm state table 
    int shared; //Lshare or Gshare (relevant for global table)


    uint32_t* target_buffer; //branch target buffer array
    uint32_t* tag_buffer; //branch ID array
    bool* valid_bit; //valid bit for branch instructions in history array


    int* history_regs; //pointer to a matrix: for LocalHist its a 2d matrix, for GlobalHist its once cell (register)
    int** fsm_tables; //pointer to a matrix: for LocalTable its a 2d matrix, for GlobalTable its a 1d array

    /*------------------------------------- CLASS FUNCTIONS & IMPLEMENTATIONS -------------------------------------*/
    BTB(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState, bool isGlobalHist, bool isGlobalTable, int Shared) {
        this->btb_size = btbSize;
        this->history_size = historySize;
        this->tag_size = tagSize;
        this->fsm_state = fsmState;
        this->is_global_hist = isGlobalHist;
        this->is_global_table = isGlobalTable;
        this->shared = Shared;

        this->target_buffer = (uint32_t*)malloc(sizeof(uint32_t) * btbSize);//
        this->tag_buffer = (uint32_t*)malloc(sizeof(uint32_t) * btbSize);// if NULL
        this->valid_bit = (bool*)malloc(sizeof(bool) * btbSize);//

        //history table and fsm tables will be initialized in the init function according to local / global cases.
    }
	
//destructor 
    ~BTB() {

        if (target_buffer != NULL) {
            free(target_buffer);
        }
        if (tag_buffer != NULL) {
            free(tag_buffer);
        }
        if (valid_bit != NULL) {
            free(valid_bit);
        }

        if (history_regs != NULL) {
            free(history_regs);
        }

        if (is_global_table && fsm_tables != NULL) {
            free(fsm_tables);
        }

        else {//local fsm table
            if (fsm_tables != NULL) {
                for (int i = 0; i < btb_size; i++) {
                    free(fsm_tables[i]);
                }
                free(fsm_tables);
            }
        }

    }
};

/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

BTB* btb; //GLOBAL VARIABLE
SIM_stats* sim_stats; //GLOBAL VARIABLE

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState, bool isGlobalHist, bool isGlobalTable, int Shared) {

    btb = new BTB(btbSize, historySize, tagSize, fsmState, isGlobalHist, isGlobalTable, Shared); //btb initialization
    if (btb == NULL) {
        cout << "ALLOCATION ERROR 1" << endl;
        return -1;
    }

    for (int i = 0; i < btb->btb_size; i++) { //initialize valid bit to false
        btb->valid_bit[i] = false;
    }

    if (isGlobalHist) {//initialize history if it is global
        btb->history_regs = (int*)malloc(sizeof(int) * 1);

        if (btb->history_regs == NULL) { //SECURITY
            cout << "ALLOCATION ERROR 2 " << endl;
            delete btb;
            return -1;
        }
        btb->history_regs[0] = 0; //init reg
    }
    else { //initialize history if it is localHist
        btb->history_regs = (int*)malloc(sizeof(int) * btbSize);
        if (btb->history_regs == NULL) { //SECURITY
            cout << "ALLOCATION ERROR 3 " << endl;
            delete btb;
            return -1;
        }

        for (int i = 0; i < btb->btb_size; i++) { //init regs

            btb->history_regs[i] = 0;
        }
    }

    if (isGlobalTable) {//initialize fsm table if it is global
        btb->fsm_tables = (int**)malloc(sizeof(int*) * 1);
        btb->fsm_tables[0] = (int*)malloc(sizeof(int) * pow(2, historySize));

        if (btb->fsm_tables == NULL) { //SECURITY
            free(btb->history_regs);
            delete btb;
            return -1;
        }

        for (int i = 0; i < pow(2, historySize); i++) { //init table
            btb->fsm_tables[0][i] = fsmState;
        }
    }

    else { //initialize fsm table if it is LocalTable
        btb->fsm_tables = (int**)malloc(sizeof(int*) * btbSize);
        for (int i = 0; i < btbSize; i++) {
            btb->fsm_tables[i] = (int*)malloc(sizeof(int) * pow(2, historySize));
        }

        if (btb->fsm_tables == NULL) { //SECURITY
            free(btb->history_regs);
            delete btb;
            return -1;
        }
        for (int i = 0; i < btbSize; i++) { //init table
            for (int j = 0; j < pow(2, historySize); j++) {
                btb->fsm_tables[i][j] = fsmState;
            }
        }
    }
    Sim_stats_init();
    return 0;
}

 
bool BP_predict(uint32_t pc, uint32_t* dst) {
    bool pc_arr[32] = { 0 };
    decimalToBinary(pc_arr, pc);// pc_arr has pc in binary


    bool index_bits[32] = { 0 };
    for (int i = 2; i < 2 + log2(btb->btb_size); i++) {
        index_bits[i - 2] = pc_arr[i];// shift right pc_arr 2. 
    }
    int decimal_index = binaryToDecimal(index_bits, 32); 
	
    if (btb->valid_bit[decimal_index] == false) { // if valid is still false
        *dst = pc + 4;
        return false;
    }

    //else valid = true
    bool tag_bits[32] = { 0 };
    for (int i = 0; (i < btb->tag_size) && (i < 30 - log2(btb->btb_size)); i++) {
        tag_bits[i] = pc_arr[2 + (int)log2(btb->btb_size) + i];
    }
    int decimal_tag = binaryToDecimal(tag_bits, 32);
   
    if (btb->tag_buffer[decimal_index] == decimal_tag) { // valid is true and tag was found 
        int fsm_col;
        int hist_index = 0;

        if (!btb->is_global_hist){
            hist_index = decimal_index;
		}
        if (btb->is_global_table) {
            bool hist_arr[32] = { 0 };
            decimalToBinary(hist_arr, btb->history_regs[hist_index]);
            if (btb->shared == USING_SHARE_LSB) { // xor from bit i>=2 
                for (int i = 0; i < btb->history_size; i++) {
					
                    hist_arr[i] = hist_arr[i] ^ pc_arr[2 + i];
                }
                fsm_col = binaryToDecimal(hist_arr, 32);
            }
            else if (btb->shared == USING_SHARE_LSB) {//xor from bit i>16
                for (int i = 0; i < btb->history_size; i++) {
                    hist_arr[i] = hist_arr[i] ^ pc_arr[16 + i];
                }
                fsm_col = binaryToDecimal(hist_arr, 32);
            }
            else // if(btb->shared != NOT_USING_SHARE) we don't use xor 
                fsm_col = btb->history_regs[hist_index];
        }
        if (!(btb->is_global_table)) { //local table
            fsm_col = btb->history_regs[hist_index];
        }
		
        //check fsm table
        int fsm_current_state;
        if (btb->is_global_table) {
            fsm_current_state = btb->fsm_tables[0][fsm_col];
        }
        else { //local table
            fsm_current_state = btb->fsm_tables[decimal_index][fsm_col];
        }
        
        if (fsm_current_state == SNT || fsm_current_state == WNT) {
            *dst = pc + 4;
            return false;
        }
        else {
            *dst = btb->target_buffer[decimal_index];
            return true;
        }
    }
 // tag was not found 
    *dst = pc + 4;
    return false;
}


void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst) {
    
    sim_stats->br_num++; //branch number
   
    if (((pred_dst != pc + 4) && !taken) || ((pred_dst != targetPc) && taken)) {//flush
        sim_stats->flush_num++;
    }

    bool pc_arr[32] = { 0 };
    decimalToBinary(pc_arr, pc);
    bool index_bits[32] = { 0 };
    for (int i = 2; i < 2 + log2(btb->btb_size); i++) {
        index_bits[i - 2] = pc_arr[i];// shift right pc_arr 2.
    }
    int decimal_index = binaryToDecimal(index_bits, 32);

    bool tag_bits[32] = { 0 };
    for (int i = 0; (i < btb->tag_size) && (i < 30 - log2(btb->btb_size)); i++) {
        tag_bits[i] = pc_arr[2 + (int)log2(btb->btb_size) + i];
    }
    int decimal_tag = binaryToDecimal(tag_bits, 32);
	
    //////////////////////valid bit =0 ||  valid bit =1 && tag not found  ///////////////////////     
    if (btb->valid_bit[decimal_index] == 0 || ((btb->tag_buffer[decimal_index] != decimal_tag) && btb->valid_bit[decimal_index] == true)) {
        btb->valid_bit[decimal_index] = true;
        btb->tag_buffer[decimal_index] = decimal_tag;
        btb->target_buffer[decimal_index] = targetPc;
     
	   int fsm_col=0;
        if (!(btb->is_global_hist))
        {
            btb->history_regs[decimal_index] = 0;
			fsm_col = decimal_index;
        }
		
		if ((btb->is_global_table))
		{
			 
            bool hist_arr[32] = { 0 };
            decimalToBinary(hist_arr, btb->history_regs[fsm_col]);
            if (btb->shared == USING_SHARE_LSB) {// xor from bit i>2
                for (int i = 0; i < btb->history_size; i++) {
					
                    hist_arr[i] = hist_arr[i] ^ pc_arr[2 + i];
                }
                fsm_col = binaryToDecimal(hist_arr, 32);
            }
            else if (btb->shared == USING_SHARE_MID) {// xor from bit i>16
                for (int i = 0; i < btb->history_size; i++) {
                    hist_arr[i] = hist_arr[i] ^ pc_arr[16 + i];
                }
                fsm_col = binaryToDecimal(hist_arr, 32);

            }
            else{ // if(btb->shared != NOT_USING_SHARE)  - we don't use xor
			fsm_col = btb->history_regs[fsm_col];
			}
			// fsm update if global table
			  if (taken) {
                if (btb->fsm_tables[0][fsm_col] != ST)
                    btb->fsm_tables[0][fsm_col]++;
            }
            else // not taken
            {
                if (btb->fsm_tables[0][fsm_col] != SNT)
                    btb->fsm_tables[0][fsm_col]--;
           }
			
		}
        //check fsm table if local table 
        if (!(btb->is_global_table))
        {
	        fsm_col = btb->history_regs[fsm_col];
            for (int i = 0; i < pow(2, btb->history_size); i++)
            {
                btb->fsm_tables[decimal_index][i] = btb->fsm_state;
            }
            if (taken) {
                if (btb->fsm_tables[decimal_index][fsm_col] != ST)
                    btb->fsm_tables[decimal_index][fsm_col]++;
            }
            else // not taken
            {
                if (btb->fsm_tables[decimal_index][fsm_col] != SNT)
                    btb->fsm_tables[decimal_index][fsm_col]--;
           }
        }
         // update history
        if (taken)
        {
            if (btb->is_global_hist)
                btb->history_regs[0] = (btb->history_regs[0] *2 + 1) % (int)pow(2, btb->history_size);
		

            else
                btb->history_regs[decimal_index] = (btb->history_regs[decimal_index] *2+ 1) % (int)pow(2, btb->history_size);
		 
        }
        else // not taken
        {
            if (btb->is_global_hist)
                btb->history_regs[0] = (btb->history_regs[0] *2) % (int)pow(2, btb->history_size);

            else
                btb->history_regs[decimal_index] = (btb->history_regs[decimal_index] *2) % (int)pow(2, btb->history_size);
        }
    }
	
    //////////////////////////////valid bit =1 && tag is in///////////////////  
    else{//if (btb->valid_bit[decimal_index] == 1 && btb->tag_buffer[decimal_index] == decimal_tag)
        btb->target_buffer[decimal_index] = targetPc;
        int fsm_col=0;
        if (!(btb->is_global_hist))
        {
			fsm_col = decimal_index;
        }
		
if ((btb->is_global_table)) // if global table
		{	 
            bool hist_arr[32] = { 0 };
            decimalToBinary(hist_arr, btb->history_regs[fsm_col]);
            if (btb->shared == USING_SHARE_LSB) {
                for (int i = 0; i < btb->history_size; i++) {// xor from bit i>2
					
                    hist_arr[i] = hist_arr[i] ^ pc_arr[2 + i];
                }
                fsm_col = binaryToDecimal(hist_arr, 32);
            }
            else if (btb->shared == USING_SHARE_MID) {//xor from bit i>16
                for (int i = 0; i < btb->history_size; i++) {
                    hist_arr[i] = hist_arr[i] ^ pc_arr[16 + i];
                }
                fsm_col = binaryToDecimal(hist_arr, 32);

            }
            else{ // if(btb->shared != NOT_USING_SHARE) _ not using xor
			fsm_col = btb->history_regs[fsm_col];
			}
			// update history 
			  if (taken) {
                if (btb->fsm_tables[0][fsm_col] != ST)
                    btb->fsm_tables[0][fsm_col]++;
            }
            else // not taken
            {
                if (btb->fsm_tables[0][fsm_col] != SNT)
                    btb->fsm_tables[0][fsm_col]--;
           }
		}
 
        if (!(btb->is_global_table)) // update fsm table if using local table 
        {
	       fsm_col = btb->history_regs[fsm_col];
            if (taken) {
                if (btb->fsm_tables[decimal_index][fsm_col] != ST)
                    btb->fsm_tables[decimal_index][fsm_col]++;
            }
            else // not taken
            {
                if (btb->fsm_tables[decimal_index][fsm_col] != SNT)
                    btb->fsm_tables[decimal_index][fsm_col]--;
           }
        }
		// update history 
        if (taken)
        {
            if (btb->is_global_hist)
                btb->history_regs[0] = (btb->history_regs[0] *2 + 1) % (int)pow(2, btb->history_size);
		

            else
                btb->history_regs[decimal_index] = (btb->history_regs[decimal_index] *2+ 1) % (int)pow(2, btb->history_size);
		 
        }
        else // not taken
        {
            if (btb->is_global_hist)
                btb->history_regs[0] = (btb->history_regs[0] *2) % (int)pow(2, btb->history_size);

            else
                btb->history_regs[decimal_index] = (btb->history_regs[decimal_index] *2) % (int)pow(2, btb->history_size);
        }
    }
	
    return;
}

// updating sim_stats
void BP_GetStats(SIM_stats* curStats) {
    curStats->flush_num = sim_stats->flush_num;
    curStats->br_num = sim_stats->br_num;
    curStats->size = sim_stats->size;
    free(sim_stats);
    return;
}

/*------------------------------------------------------------- AUX FUNCTIONS -------------------------------------------------------------------*/
// gets a number and an array , the function converts the num to binary and puts the result in the array .
void decimalToBinary(bool* arr, uint32_t num) {
    for (int i = 0; i <32 ; i++) {
        arr[i] = num % 2;
        num = num / 2;
    }
}

//gets a number and an array , the function converts the binary num which is in the array to decimal . 
uint32_t binaryToDecimal(bool* pc_arr, int len) {
    uint32_t num = 0;
    for (int i = 0; i < len; i++) {
        if (pc_arr[i]) {
            num += pow(2, i);
        }
    }
    return num;
}

// initialize sim_stats and calculates the size 
void Sim_stats_init() {
    sim_stats = (SIM_stats*)malloc(sizeof(SIM_stats) * 1);
    if (sim_stats == NULL) {
        return;
    }

    sim_stats->br_num = 0;
    sim_stats->flush_num = 0;
    sim_stats->size = 0;

    if ((btb->is_global_table) && (btb->is_global_hist)) {
        sim_stats->size = btb->btb_size * (btb->tag_size + VALID_BIT_SIZE + TARGET_PC_SIZE) + 2*pow(2, btb->history_size) + btb->history_size;
    }
    if (!(btb->is_global_table) && (btb->is_global_hist)) {
        sim_stats->size = btb->btb_size * (btb->tag_size + VALID_BIT_SIZE + TARGET_PC_SIZE + 2*pow(2, btb->history_size)) + btb->history_size;
    }
    if (btb->is_global_table && !(btb->is_global_hist)) {
        sim_stats->size = btb->btb_size * (btb->tag_size + VALID_BIT_SIZE + TARGET_PC_SIZE + btb->history_size) + 2*pow(2, btb->history_size);
    }
    if (!(btb->is_global_table) && !(btb->is_global_hist)) {
        sim_stats->size = btb->btb_size * (btb->tag_size + VALID_BIT_SIZE + TARGET_PC_SIZE + btb->history_size + 2*pow(2, btb->history_size));
    }
}
