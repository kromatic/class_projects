Cache Memory Simulator completed as part of cs154-2015 at the University of Chicago (based on "Cache lab" from CMU)

csim.c: implementation of cache memory simulator

cachelab.h / cachelab.c: helper functions provided by University of Chicago staff for this lab

compilation: run make in this directory

usage: ./csim -s <s> -E <E> -b <b> -t <tracefile>
    * -s <s>: number of set index bits
    * -E <E>: associativity
    * -b <b>: number of block bit
    * -t <tracefile>: valgrind trace to be simulated
