# Details

Date : 2024-09-04 18:48:44

Directory /home/asemones/Documents/personal_code/dbase

Total : 48 files,  5252 codes, 479 comments, 747 blanks, all 6478 lines

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [Makefile](/Makefile) | Makefile | 29 | 8 | 10 | 47 |
| [README.md](/README.md) | Markdown | 8 | 0 | 5 | 13 |
| [src/db/backend/WAL.h](/src/db/backend/WAL.h) | C | 122 | 8 | 14 | 144 |
| [src/db/backend/api.h](/src/db/backend/api.h) | C | 14 | 3 | 4 | 21 |
| [src/db/backend/btree.h](/src/db/backend/btree.h) | C | 221 | 1 | 34 | 256 |
| [src/db/backend/compactor.h](/src/db/backend/compactor.h) | C | 474 | 48 | 64 | 586 |
| [src/db/backend/dal/metadata.h](/src/db/backend/dal/metadata.h) | C | 162 | 9 | 16 | 187 |
| [src/db/backend/indexer.h](/src/db/backend/indexer.h) | C | 135 | 26 | 12 | 173 |
| [src/db/backend/iter.h](/src/db/backend/iter.h) | C | 299 | 15 | 29 | 343 |
| [src/db/backend/json.h](/src/db/backend/json.h) | C | 181 | 2 | 18 | 201 |
| [src/db/backend/key-value.h](/src/db/backend/key-value.h) | C | 90 | 4 | 14 | 108 |
| [src/db/backend/lsm.h](/src/db/backend/lsm.h) | C | 290 | 25 | 31 | 346 |
| [src/db/backend/option.h](/src/db/backend/option.h) | C | 58 | 0 | 8 | 66 |
| [src/db/backend/sst_builder.h](/src/db/backend/sst_builder.h) | C | 56 | 1 | 10 | 67 |
| [src/db/backend/store.h](/src/db/backend/store.h) | C | 44 | 0 | 3 | 47 |
| [src/ds/arena.h](/src/ds/arena.h) | C | 55 | 0 | 10 | 65 |
| [src/ds/associative_array.h](/src/ds/associative_array.h) | C | 23 | 0 | 5 | 28 |
| [src/ds/bloomfilter.h](/src/ds/bloomfilter.h) | C | 173 | 0 | 14 | 187 |
| [src/ds/byte_buffer.h](/src/ds/byte_buffer.h) | C | 129 | 1 | 7 | 137 |
| [src/ds/cache.h](/src/ds/cache.h) | C | 149 | 5 | 25 | 179 |
| [src/ds/frontier.h](/src/ds/frontier.h) | C | 84 | 39 | 19 | 142 |
| [src/ds/hashtbl.h](/src/ds/hashtbl.h) | C | 169 | 54 | 21 | 244 |
| [src/ds/linkl.h](/src/ds/linkl.h) | C | 60 | 0 | 7 | 67 |
| [src/ds/list.h](/src/ds/list.h) | C | 211 | 88 | 24 | 323 |
| [src/ds/skiplist.h](/src/ds/skiplist.h) | C | 146 | 0 | 15 | 161 |
| [src/ds/structure_pool.h](/src/ds/structure_pool.h) | C | 80 | 0 | 3 | 83 |
| [src/ds/threadpool.h](/src/ds/threadpool.h) | C | 181 | 53 | 28 | 262 |
| [src/tests/test_iter.h](/src/tests/test_iter.h) | C | 83 | 1 | 32 | 116 |
| [src/tests/test_util_funcs.h](/src/tests/test_util_funcs.h) | C | 120 | 1 | 12 | 133 |
| [src/tests/testbloom.h](/src/tests/testbloom.h) | C | 106 | 0 | 21 | 127 |
| [src/tests/testbtree.h](/src/tests/testbtree.h) | C | 94 | 6 | 18 | 118 |
| [src/tests/testcompactor.h](/src/tests/testcompactor.h) | C | 175 | 2 | 22 | 199 |
| [src/tests/testfrontier.h](/src/tests/testfrontier.h) | C | 58 | 0 | 20 | 78 |
| [src/tests/testhashtble.h](/src/tests/testhashtble.h) | C | 71 | 10 | 27 | 108 |
| [src/tests/testio.h](/src/tests/testio.h) | C | 29 | 1 | 6 | 36 |
| [src/tests/testjson.h](/src/tests/testjson.h) | C | 65 | 0 | 14 | 79 |
| [src/tests/testlist.h](/src/tests/testlist.h) | C | 187 | 2 | 20 | 209 |
| [src/tests/testlru.h](/src/tests/testlru.h) | C | 45 | 0 | 5 | 50 |
| [src/tests/testlsm.h](/src/tests/testlsm.h) | C | 79 | 0 | 14 | 93 |
| [src/tests/testmetadata.h](/src/tests/testmetadata.h) | C | 40 | 0 | 11 | 51 |
| [src/tests/testrunner.c](/src/tests/testrunner.c) | C | 108 | 0 | 5 | 113 |
| [src/tests/testthreadp.h](/src/tests/testthreadp.h) | C | 57 | 0 | 9 | 66 |
| [src/tests/testwal.h](/src/tests/testwal.h) | C | 82 | 1 | 26 | 109 |
| [src/tests/unity/unityConfig.cmake](/src/tests/unity/unityConfig.cmake) | CMake | 1 | 0 | 0 | 1 |
| [src/util/alloc_util.h](/src/util/alloc_util.h) | C | 20 | 0 | 4 | 24 |
| [src/util/io.h](/src/util/io.h) | C | 46 | 54 | 6 | 106 |
| [src/util/maths.h](/src/util/maths.h) | C | 8 | 0 | 3 | 11 |
| [src/util/stringutil.h](/src/util/stringutil.h) | C | 135 | 11 | 22 | 168 |

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)