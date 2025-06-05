# dbase
In progress dbms from scratch with a focus on write centered workloads. Currently implementing the key-value store that will power the rest of the system.
This db is far from complete and should not be used. The architeture is a skip-list log-structured merge-tree with a compactor for garbage collection, along with a clock buffer pool. Compactor supports
leveled compaction currently.
There is no final end goal for this project but there are various "checkpoints" that ensure a "good enough" product always exists. 
The next checkpoints are as follows:

An expanded key-value store closer to a small scale production ready storage engine. The qualifiers for this include implementing my own IO/scheduling services/rate limiter, optimizing the compaction algorithm, robust crash recovery, and seperation of keys and values. Finally, the implemnetation of an aysnc io engine(io_uring) to drastically improve io preformance.

~~Next, I will decide between whether to build a sql or nosql database ontop, likely in a systems language better equipped for larger projects(c++ or rust). Handrolling vectors is.. not fun. This will include a distrbuted layer ontop (sharding and replication)~~ distributed sql is the plan. 

range based sharding per core for each node to reduce contention?

coroutine based cooperative multitasking
consistient hashing between cores with a merge for range scans?
new channel type, consumer writes to an array, producer has spare memory in its queue. producer endlessly enques in a circle until it reaches a marked slot. check if that slot
has been read. if it has, overwrite. if full, wait on a cond variable the consumer should activate. 

Client Command
                        │
                        ▼
              ┌───────────────────┐
              │   Coordinator     │
              │     Thread        │
              └───────┬───────────┘
                      │
        ┌─────────────┼─────────────┐
        │             │             │
        ▼             ▼             ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│ Shard 1      │ │ Shard 2      │ │ Shard 3      │
└──────┬───────┘ └──────┬───────┘ └──────┬───────┘
       │                │                │
       ▼                ▼                ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│ KV Store     │ │ KV Store     │ │ KV Store     │
│ Coroutine    │ │ Coroutine    │ │ Coroutine    │
│ Scheduler    │ │ Scheduler    │ │ Scheduler    │
└──────────────┘ └──────────────┘ └──────────────┘


FEATURES:
LSM tree ran under a custom made async framework backed by coroutines
Supports several buffer pool strategies, all of which are VARIABLE size, one of which is a compeletely novel design, which locks huge page memory in place to exterminate page fauls
Supports io uring backend and kernel bypass Soon^TM
wal for durabiltiy, value log for key-value sep for values > 512b
Heirachal cache with record level granuality and block level granularity
partitioned cache friendly filters
limited copying and raw malloc