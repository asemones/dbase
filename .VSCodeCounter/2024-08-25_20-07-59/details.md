# Details

Date : 2024-08-25 20:07:59

Directory /home/asemones/Documents/personal_code/dbase

Total : 47 files,  4931 codes, 480 comments, 679 blanks, all 6090 lines

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [Makefile](/Makefile) | Makefile | 29 | 8 | 10 | 47 |
| [README.md](/README.md) | Markdown | 8 | 0 | 5 | 13 |
| [src/db/backend/WAL.h](/src/db/backend/WAL.h) | C++ | 23 | 0 | 11 | 34 |
| [src/db/backend/api.h](/src/db/backend/api.h) | C++ | 14 | 3 | 4 | 21 |
| [src/db/backend/btree.h](/src/db/backend/btree.h) | C++ | 221 | 1 | 34 | 256 |
| [src/db/backend/compactor.h](/src/db/backend/compactor.h) | C++ | 513 | 46 | 55 | 614 |
| [src/db/backend/dal/metadata.h](/src/db/backend/dal/metadata.h) | C++ | 162 | 9 | 16 | 187 |
| [src/db/backend/indexer.h](/src/db/backend/indexer.h) | C++ | 106 | 26 | 10 | 142 |
| [src/db/backend/iter.h](/src/db/backend/iter.h) | C++ | 299 | 15 | 25 | 339 |
| [src/db/backend/json.h](/src/db/backend/json.h) | C++ | 168 | 2 | 12 | 182 |
| [src/db/backend/key-value.h](/src/db/backend/key-value.h) | C++ | 12 | 3 | 13 | 28 |
| [src/db/backend/lsm.h](/src/db/backend/lsm.h) | C++ | 274 | 28 | 34 | 336 |
| [src/db/backend/option.h](/src/db/backend/option.h) | C++ | 52 | 0 | 7 | 59 |
| [src/db/backend/sst_builder.h](/src/db/backend/sst_builder.h) | C++ | 119 | 2 | 12 | 133 |
| [src/db/backend/store.h](/src/db/backend/store.h) | C++ | 44 | 0 | 3 | 47 |
| [src/ds/arena.h](/src/ds/arena.h) | C++ | 55 | 0 | 10 | 65 |
| [src/ds/associative_array.h](/src/ds/associative_array.h) | C++ | 22 | 0 | 4 | 26 |
| [src/ds/bloomfilter.h](/src/ds/bloomfilter.h) | C++ | 173 | 0 | 14 | 187 |
| [src/ds/byte_buffer.h](/src/ds/byte_buffer.h) | C++ | 98 | 0 | 7 | 105 |
| [src/ds/cache.h](/src/ds/cache.h) | C | 146 | 5 | 26 | 177 |
| [src/ds/frontier.h](/src/ds/frontier.h) | C++ | 84 | 39 | 19 | 142 |
| [src/ds/hashtbl.h](/src/ds/hashtbl.h) | C++ | 169 | 54 | 21 | 244 |
| [src/ds/linkl.h](/src/ds/linkl.h) | C++ | 60 | 0 | 6 | 66 |
| [src/ds/list.h](/src/ds/list.h) | C++ | 203 | 88 | 21 | 312 |
| [src/ds/skiplist.h](/src/ds/skiplist.h) | C++ | 142 | 0 | 12 | 154 |
| [src/ds/structure_pool.h](/src/ds/structure_pool.h) | C++ | 80 | 0 | 3 | 83 |
| [src/ds/threadpool.h](/src/ds/threadpool.h) | C++ | 181 | 53 | 28 | 262 |
| [src/tests/test_iter.h](/src/tests/test_iter.h) | C++ | 84 | 1 | 31 | 116 |
| [src/tests/test_util_funcs.h](/src/tests/test_util_funcs.h) | C++ | 108 | 1 | 14 | 123 |
| [src/tests/testbloom.h](/src/tests/testbloom.h) | C++ | 106 | 0 | 21 | 127 |
| [src/tests/testbtree.h](/src/tests/testbtree.h) | C++ | 94 | 6 | 18 | 118 |
| [src/tests/testcompactor.h](/src/tests/testcompactor.h) | C++ | 161 | 2 | 13 | 176 |
| [src/tests/testfrontier.h](/src/tests/testfrontier.h) | C++ | 58 | 0 | 20 | 78 |
| [src/tests/testhashtble.h](/src/tests/testhashtble.h) | C++ | 71 | 10 | 27 | 108 |
| [src/tests/testio.h](/src/tests/testio.h) | C++ | 29 | 1 | 6 | 36 |
| [src/tests/testjson.h](/src/tests/testjson.h) | C++ | 65 | 0 | 14 | 79 |
| [src/tests/testlist.h](/src/tests/testlist.h) | C++ | 187 | 2 | 20 | 209 |
| [src/tests/testlru.h](/src/tests/testlru.h) | C++ | 45 | 0 | 5 | 50 |
| [src/tests/testlsm.h](/src/tests/testlsm.h) | C++ | 73 | 3 | 9 | 85 |
| [src/tests/testmetadata.h](/src/tests/testmetadata.h) | C++ | 40 | 0 | 11 | 51 |
| [src/tests/testrunner.c](/src/tests/testrunner.c) | C | 101 | 7 | 5 | 113 |
| [src/tests/testthreadp.h](/src/tests/testthreadp.h) | C++ | 57 | 0 | 9 | 66 |
| [src/tests/unity/unityConfig.cmake](/src/tests/unity/unityConfig.cmake) | CMake | 1 | 0 | 0 | 1 |
| [src/util/alloc_util.h](/src/util/alloc_util.h) | C | 20 | 0 | 4 | 24 |
| [src/util/io.h](/src/util/io.h) | C++ | 39 | 54 | 4 | 97 |
| [src/util/maths.h](/src/util/maths.h) | C++ | 6 | 0 | 4 | 10 |
| [src/util/stringutil.h](/src/util/stringutil.h) | C++ | 129 | 11 | 22 | 162 |

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)