# Two-Phase Locking in C++

## Build

```shell
$ cmake -Bbuild -H.
$ cd build
$ make
$ ./2pl_test <capacityOfTheArray> <numOfRepetitionsForEachThread> <numberOfThreads>
```

## Implementation

The two-phase locking mechanism in this repository is implemented as follows:

1. each slot of the array(storage) has an individual mutex
2. if a transaction wants to read a slot, it must acquire the lock of the slot in shared mode, several transactions can hold the same shared_lock simulataneously. But if the lock has already been acquired in exclusive mode, then the reader must wait, waiting is achieved by creating a `shared_lock`, which is a blocking call.
3. when a transaction wants to write, it must acquire the lock in exclusive mode, other transactions must wait until the lock is released
4. if a transaction fails to acquire a write lock(exclusive lock), it must give up and rollback, although such operation is expensive, it solves the deadlock problem:
    - the transaction fails to acquire the write lock because some other transaction is writing the slot behind the lock, that means none of the slots that the current transaction has read is being written, otherwise they would haven't been read.
    - the transaction fails to acquire the write lock because some transactions are reading the slot behind the lock, the current transaction has to wait
    - it's hard to tell if the target slot is being read or written, so we might just let the transaction abort if it can't get the write lock
5. a transaction consists of a chain of operations, each operation acquires the locks it aspires to in a consecutive order, this is the growing phase
6. when the transaction commits itself, it releases the locks it holds in the reserve order as opposed to the growing phase, this is called the shrinking phase

## Benchmarks

the line `transaction time: 0.0000017461, retry: 407` reflects the average time for each transaction(rand + read + write) and the number of retries after running into deadlock

the line `1: 904789` shows that the occurrence of `1` in the test array is 904789

```shell
$ ./2pl_test 1000000 100000 0
1: 1000000
$ ./2pl_test 1000000 100000 1
transaction time: 0.0000019209, retry: 0
1: 904789
3: 82013
5: 11202
7: 1667
9: 280
11: 40
13: 9
$ ./2pl_test 1000000 100000 2
transaction time: 0.0000017461, retry: 407
transaction time: 0.0000017807, retry: 286
1: 819509
3: 134419
5: 33075
7: 9150
9: 2670
11: 792
13: 271
15: 67
17: 32
19: 11
21: 3
25: 1
$ ./2pl_test 1000000 100000 3
transaction time: 0.0000017524, retry: 2824
transaction time: 0.0000019801, retry: 2488
transaction time: 0.0000020819, retry: 712
1: 744014
3: 165638
5: 55820
7: 20824
9: 8095
11: 3245
13: 1395
15: 542
17: 247
19: 102
21: 50
23: 15
25: 10
27: 2
33: 1
$ ./2pl_test 1000000 100000 4
transaction time: 0.0000017347, retry: 4113
transaction time: 0.0000017649, retry: 2620
transaction time: 0.0000020069, retry: 3269
transaction time: 0.0000021289, retry: 2778
1: 676711
3: 182712
5: 74733
7: 34035
9: 16063
11: 7809
13: 3826
15: 2000
17: 996
19: 538
21: 271
23: 123
25: 69
27: 54
29: 25
31: 17
33: 8
35: 5
37: 2
39: 1
41: 2
$ ./2pl_test 1000000 100000 5
transaction time: 0.0000017999, retry: 6145
transaction time: 0.0000018344, retry: 4138
transaction time: 0.0000018320, retry: 6068
transaction time: 0.0000020232, retry: 4915
transaction time: 0.0000020642, retry: 3572
1: 616930
3: 190383
5: 89107
7: 45572
9: 24730
11: 13931
13: 7886
15: 4730
17: 2653
19: 1643
21: 996
23: 572
25: 364
27: 191
29: 127
31: 60
33: 42
35: 31
37: 18
39: 13
41: 10
43: 2
45: 4
47: 2
49: 2
55: 1
$ ./2pl_test 1000000 100000 6
transaction time: 0.0000018114, retry: 7318
transaction time: 0.0000018810, retry: 7361
transaction time: 0.0000020592, retry: 6143
transaction time: 0.0000021121, retry: 6301
transaction time: 0.0000021184, retry: 6504
transaction time: 0.0000021319, retry: 6328
1: 562453
3: 190947
5: 98408
7: 56539
9: 33575
11: 20921
13: 13087
15: 8334
17: 5438
19: 3591
21: 2260
23: 1537
25: 1029
27: 638
29: 402
31: 243
33: 215
35: 133
37: 78
39: 49
41: 35
43: 25
45: 17
47: 18
49: 7
51: 8
53: 5
55: 5
59: 1
61: 1
67: 1
```
