# dbase
In progress production-grade dbms from scratch with a focus on write centered workloads. Currently implementing the key-value store that will power the rest of the system.
This db is far from complete and should not be used.
There is no final end goal for this project but there are various "checkpoints" that ensure a "good enough" product always exists. 
Currently, the MVP of a simple key-value engine supporting get/set, delete, and scan is nearly complete. The next checkpoints are as follows:

An expanded key-value store closer to a small scale production ready storage engine. The qualifiers for this include implementing my own IO/scheduling services/rate limiter, optimizing the compaction algorithm, robust crash recovery, compression and seperation of keys and values, proper alignment of data blocks. Finally, the implemntation of an aysnc io engine to drastically improve write preformance.

The next checkpoint is a MVP database of either SQL or NOSQL. It involves a basic query language or subset of sql and preforms the most basic of operations. Miminal query optimization/planning, if any

The next checkpoint from here will be written out when the final storage engine checkpoint is finished. 

