# computer-architecture
Computer Architecture course repository

1) Branch target predictor simulator:
   - 2 level predictor
   - bimodal state
   - direct mapping
   - Lshare/Gshare support
   - assumes blocks are aligned in memory
  
2) Cache Simulator:
   - 2 level inclusive cache
   - N-way set associative
   - supports WriteBack , Write Allocate/No Write Allocate
   - LRU cache replacement policy
   - calculates hit/miss rate, average memory access time
   - assumes blocks are aligned in memory
  
3) Data-flow dependency graph:
   - allows calculation of the theoretical parallelism which exists in the program
   - refers to to real dependencies(RAW), ignores false dependencies which can be solved by appropriate
     mechanisms, such as register renaming.
   - the interface is initialized with the CPU characteristics and traces
   - allows queries regarding the information dependencies of commands in the program, for example:
      which commands does a pareticular command / instruction depend on.      

4) Multi-threading simulator:
   - Blocked MT
   - Finegrained MT (interleaved)
