#include "core_api.h"
#include "sim_api.h"
#include <vector>
#include <iostream>
#include <stdio.h>

using namespace std;
enum MT_type {BLOCKED, FINRGRAINED};

class Thread {
        int id; // thread num
        int pc;
        bool is_halt; // true when we are done
        int waiting_cycles;

        public:
        int regs[REGS_COUNT];
        Thread(int _id): id (_id), pc(0),is_halt(false), waiting_cycles(0)
        {
            for(int i = 0; i < REGS_COUNT; ++i){
                regs[i] = 0;
            }
        }
        ~Thread()=default;
        Thread& operator=(Thread* thread)
        {
            id=thread->id;
            pc=thread->pc;
            waiting_cycles=thread->waiting_cycles;
            is_halt=thread->is_halt;
            for(int i = 0; i < REGS_COUNT; ++i){
                regs[i]=thread->regs[i] ;
            }
            return *this;
        }

        int getId()
        {
            return id;
        }
         int getPc()
        {
            return pc;
        }
        bool isHalt()
        {
            return is_halt;
        }
        int getWaittingCycles()
        {
            return waiting_cycles;
        }
        void reduceWaittingTime(){
            waiting_cycles--;
        }
        void increasePc() {
            pc++;
        }
        void threadIsDone() {
           is_halt=true;
        }
        void setWaittingCycles(int n) {
        waiting_cycles=n;
        }


};

class simulator{
        public:
        int type;
        vector<Thread*> threads;
        int cycles_count;
        int inst_num;
        int load_latency;
        int store_latency;
        int switch_penalty;
        int threads_num;
        int halt_treads_num;//number of threads in halt
        simulator(MT_type _type): type(_type), cycles_count(0), inst_num(0), load_latency(SIM_GetLoadLat()),
              store_latency(SIM_GetStoreLat()), switch_penalty(0),threads_num(SIM_GetThreadsNum()),
               halt_treads_num(0)
        {
            for(int i = 0; i < threads_num; ++i)
            {
                threads.push_back(  new Thread(i));
            }
            if(type == BLOCKED)
            {
                switch_penalty = SIM_GetSwitchCycles();
            }
        }
        int nextThread(Thread* current_thread)
        {
           int temp =(current_thread->getId() + 1) % threads_num;
            return temp;
        }
        void runCycle(); // run cycles
        void runInstruction(Thread* thread);
        Thread* switchThreadBlocked(Thread* current_thread); // switches to the next avaialble thread
         Thread* switchThreadFine(Thread* current_thread);
        bool AvailableThread();// checks if there is an available thread
        bool isDone(){
        return (halt_treads_num==threads_num);
        }
};

void simulator::runInstruction(Thread *thread){
    Instruction instruction;
    SIM_MemInstRead(thread->getPc(), &instruction, thread->getId());
    inst_num++;
    thread->increasePc();
    switch(instruction.opcode)
    {
        case CMD_NOP:
            break;
        case CMD_ADD:// dst <- src1 + src2
            thread->regs[instruction.dst_index]= thread->regs[instruction.src1_index] +thread->regs[instruction.src2_index_imm];
            break;
        case  CMD_SUB:// dst <- src1 - src2
            thread->regs[instruction.dst_index]= thread->regs[instruction.src1_index] -thread->regs[instruction.src2_index_imm];
            break;
        case CMD_ADDI:// dst <- src1 + imm
            thread->regs[instruction.dst_index] = thread->regs[instruction.src1_index] + instruction.src2_index_imm;
            break;
        case CMD_SUBI: // dst <- src1 - imm
            thread->regs[instruction.dst_index] = thread->regs[instruction.src1_index] - instruction.src2_index_imm;
            break;
        case CMD_LOAD:// dst <- Mem[src1 + src2]  (src2 may be an immediate)
            int32_t dst;
            if(instruction.isSrc2Imm)
            {
                SIM_MemDataRead(thread->regs[instruction.src1_index] + instruction.src2_index_imm, &dst);
                thread->regs[instruction.dst_index] = dst;
            }else{
                SIM_MemDataRead(thread->regs[instruction.src1_index] + thread->regs[instruction.src2_index_imm], &dst);
                thread->regs[instruction.dst_index] = dst;
            }
            thread->setWaittingCycles(load_latency + 1);
            break;
        case CMD_STORE:// Mem[dst + src2] <- src1  (src2 may be an immediate)
            if(instruction.isSrc2Imm){
                SIM_MemDataWrite( thread->regs[instruction.dst_index] + instruction.src2_index_imm, thread->regs[instruction.src1_index]);
            }else{
                SIM_MemDataWrite(thread->regs[instruction.dst_index] + thread->regs[instruction.src2_index_imm], thread->regs[instruction.src1_index]);
            }
            thread->setWaittingCycles( store_latency + 1);
            break;
        case CMD_HALT:

            halt_treads_num++;
            thread->threadIsDone();
            break;
        default:
            break;
    }
}

Thread* simulator::switchThreadBlocked(Thread* current_thread){
    if(current_thread->getWaittingCycles() == 0 && !(current_thread->isHalt()) )
    {
        // current thread is available
        return current_thread;
    }
    int next_thread =this->nextThread(current_thread); // check next thread

    for(int i = 0; i < threads_num; i++)
    {
        if((threads[next_thread]->getWaittingCycles() == 0) && !(threads[next_thread]->isHalt()))
        {
            // thread is available
            current_thread =  (threads[next_thread]);

            break;
        }
        next_thread= (1 + next_thread) % threads_num;// check next thread
    }

        // pay penalty
        for(int i = 0; i <  switch_penalty; i++)
        {
            this->runCycle(); // context switch overhead
        }

    return current_thread;
}


Thread* simulator::switchThreadFine(Thread* current_thread){
    int next_thread =(current_thread->getId() + 1) % threads_num; // check next thread

    for(int i = 0; i < threads_num; i++)
    {
        if((threads[next_thread]->getWaittingCycles() == 0) && !(threads[next_thread]->isHalt()))
        {
            // thread is available
            current_thread =  (threads[next_thread]);

            break;
        }
        next_thread= (1 + next_thread) % threads_num;// check next thread
    }
    return current_thread;
}


void simulator::runCycle()
{
   cycles_count++;
    for(int i = 0; i < threads_num; ++i)
    {
        if((threads[i])->getWaittingCycles() >0)
        {
            threads[i]->reduceWaittingTime();
        }
    }
}


bool simulator:: AvailableThread(){
    if(halt_treads_num==threads_num)
    {
        return false;
    }

    while (true){

        for (auto itr = threads.begin() ;itr != threads.end(); itr++)
        {

            if((*itr)->getWaittingCycles() == 0 && !((*itr)->isHalt()))
            {
                return true;
            }

        }
        this->runCycle(); // idle

    }
    return true;
}

/*GLOBAL*/
simulator* blocked = NULL;
simulator* fine_graind= NULL;


void CORE_BlockedMT() {
    blocked=new simulator(BLOCKED);
    Thread* current= blocked->threads[0];
    while(!(blocked->isDone()) && blocked->AvailableThread() )
    {
        while (current->getWaittingCycles()==0 && !(current->isHalt()))
        {
            blocked->runInstruction(current);
            blocked->runCycle();
        }
        if (!(blocked->AvailableThread()))
        {
            break;
        }
      current=blocked->switchThreadBlocked(current);
    }
}

void CORE_FinegrainedMT() {
    fine_graind=new simulator(FINRGRAINED);
    Thread* current=fine_graind->threads[0];
    while(!(fine_graind->isDone()) && (fine_graind->AvailableThread()))
    {
        while (current->getWaittingCycles()==0 && !(current->isHalt()))
        {
            fine_graind->runInstruction(current);
            fine_graind->runCycle();

        if (!(fine_graind->AvailableThread()))
        {
            break;
        }
     
        current=fine_graind->switchThreadFine(current);
        }
    }
}


double CORE_BlockedMT_CPI(){
    double ipc=(double)blocked->cycles_count/ (double)blocked->inst_num;
    delete blocked;
    return ipc;
}


double CORE_FinegrainedMT_CPI(){
    double pci=(double)fine_graind->cycles_count/ (double)fine_graind->inst_num;
    delete fine_graind;
    return pci;
}


void CORE_BlockedMT_CTX(tcontext* context, int threadid) {
    for(int i = 0; i < REGS_COUNT; ++i)
    {
        context[threadid].reg[i] = blocked->threads[threadid]->regs[i];
    }
}


void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
    for(int i = 0; i < REGS_COUNT; ++i)
    {
        context[threadid].reg[i] = fine_graind->threads[threadid]->regs[i];
    }
}
