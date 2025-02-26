# dbase
In progress dbms from scratch with a focus on write centered workloads. Currently implementing the key-value store that will power the rest of the system.
This db is far from complete and should not be used. The architeture is a skip-list log-structured merge-tree with a compactor for garbage collection, along with a clock buffer pool. Compactor supports
leveled compaction currently.
There is no final end goal for this project but there are various "checkpoints" that ensure a "good enough" product always exists. 
The next checkpoints are as follows:

An expanded key-value store closer to a small scale production ready storage engine. The qualifiers for this include implementing my own IO/scheduling services/rate limiter, optimizing the compaction algorithm, robust crash recovery, and seperation of keys and values. Finally, the implemnetation of an aysnc io engine(io_uring) to drastically improve io preformance.

Next, I will decide between whether to build a sql or nosql database ontop, likely in a systems language better equipped for larger projects(c++ or rust). Handrolling vectors is.. not fun. This will include a distrbuted layer ontop (sharding and replication)


