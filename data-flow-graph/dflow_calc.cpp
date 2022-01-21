/* Implementation for the dataflow statistics calculator */

#include "dflow_calc.h"
#include <vector>
using namespace std;

/*                                                                                                                            (entry)
class represents a node in the dependency graph                                                                              /       \
* index: instruction index in trace file                                                                          (instruction1)   (instruction2)
* src1: dependent 1 (operand1)                                                                                            /              /
* src2: dependent 2 (opernd2)                                                                                     (instruction3)        /
* exe_time: execution time for this instruction                                                                            \           /
* exe_sum: sum of execution time for all instructions in path "entry" -> this specific instruction                        (instruction4)
* is_exit: flag for graph's end - instead of inserting a spesific node for "end"                                                |
*                                                                                                                            (exit)               */
class DependencyNode {
public:
    int index;
    int src1;
    int src2;
    int exe_time;
    int exe_sum;
    bool is_exit;

    DependencyNode(int index, int exe_time) : index(index), src1(-1), src2(-1), exe_time(exe_time), exe_sum(0), is_exit(true) {}
    DependencyNode(int index, int exe_time, bool is_exit) : index(index), src1(-1), src2(-1), exe_time(exe_time), exe_sum(0), is_exit(is_exit) {}
    ~DependencyNode() {}
};


/* the data flow graph: implemented by using a vector of dependency nodes, the instructions from the trace file are pushed back
   in-order to the vector which maintains the original order for commit later onting later but still allows an OoO execution */
class flowGraph {
public:
    vector<DependencyNode*> flow_graph;
    unsigned int size;

    flowGraph() :flow_graph(vector<DependencyNode*>()), size(0) {}
    ~flowGraph() {
        for (unsigned int i = 0; i < size; i++) {
            delete (flow_graph)[i];
        }
    }

    void addInst(DependencyNode* theInst) {
        flow_graph.push_back(theInst);
        size = flow_graph.size();
    }
};



ProgCtx analyzeProg(const unsigned int opsLatency[], const InstInfo progTrace[], unsigned int numOfInsts) {

    flowGraph* graph = new flowGraph();
    DependencyNode* entry = new DependencyNode(0, 0, false); //create entry point for program (first node in graph)
    graph->addInst(entry);

    //iterate over instructions file and create a new node for each instruction
    for (unsigned int i = 0; i < numOfInsts; i++) {
        DependencyNode* node = new DependencyNode(i + 1, opsLatency[progTrace[i].opcode]);
        graph->addInst(node);
    }

    for (unsigned int i = 0; i < numOfInsts; i++) {
        for (unsigned int j = i + 1; j < numOfInsts; j++) {
            DependencyNode dep_node = *(graph->flow_graph[i + 1]);

            //instuction src1 depends on instruction dest
            if ((unsigned int)progTrace[j].src1Idx == (unsigned int)progTrace[i].dstIdx) {
                dep_node.is_exit = false; //node cannot be exit node because another instuction depends on it
                graph->flow_graph[j + 1]->src1 = dep_node.index - 1; //-1 because entry is in offset 0
                int new_time_sum = dep_node.exe_sum + dep_node.exe_time; //update exec time for current node
                if (graph->flow_graph[j + 1]->exe_sum < new_time_sum) { //check if execution path time sum needs updating
                    graph->flow_graph[j + 1]->exe_sum = new_time_sum;
                }
            }

            //instuction src2 depends on instruction dest
            if ((unsigned int)progTrace[j].src2Idx == (unsigned int)progTrace[i].dstIdx) {
                dep_node.is_exit = false; //node cannot be exit node because another instuction depends on it
                graph->flow_graph[j + 1]->src2 = dep_node.index - 1; //-1 because entry is in offset 0
                int new_time_sum = dep_node.exe_sum + dep_node.exe_time; //update exec time for current node
                if (graph->flow_graph[j + 1]->exe_sum < new_time_sum) { //check if execution path time sum needs updating
                    graph->flow_graph[j + 1]->exe_sum = new_time_sum;
                }
            }

            //break loop if found an instruction with a greater index which writres to the same register
            if (progTrace[j].dstIdx == progTrace[i].dstIdx)
                break;
        }
    }
    return (ProgCtx)graph;
}

void freeProgCtx(ProgCtx ctx) {

    flowGraph* graph = (flowGraph*)ctx;
    delete (graph);
}

int getInstDepth(ProgCtx ctx, unsigned int theInst) {

    flowGraph* graph = (flowGraph*)ctx;

    //check input
    if (theInst < 0) return -1;
    if (theInst > (graph->size - 1)) return -1;

    return (graph->flow_graph[theInst + 1]->exe_sum);
}

int getInstDeps(ProgCtx ctx, unsigned int theInst, int* src1DepInst, int* src2DepInst) {

    flowGraph* graph = (flowGraph*)ctx;

    //check input
    if (theInst < 0) return -1;
    if (theInst > (graph->size - 1)) return -1;
    if (src1DepInst == nullptr || src2DepInst == nullptr) return -1;

    *src1DepInst = graph->flow_graph[theInst + 1]->src1;
    *src2DepInst = graph->flow_graph[theInst + 1]->src2;

    return 0;
}

int findMax(flowGraph* graph) {

    int max = graph->flow_graph[0]->exe_sum + graph->flow_graph[0]->exe_time;
    for (unsigned int i = 0; i < graph->size; i++) {
        int curr_exe_time = graph->flow_graph[i]->exe_sum + graph->flow_graph[i]->exe_time;
        if (curr_exe_time > max && graph->flow_graph[i]->is_exit)
            max = curr_exe_time;
    }
    return max;
}

int getProgDepth(ProgCtx ctx) {

    flowGraph* graph = (flowGraph*)ctx;
    return findMax(graph);
}
