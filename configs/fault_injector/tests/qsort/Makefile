FILE1 = qsort_small.c
FILE2 = qsort_large.c

all: qsort_large.bin qsort_large_dup.bin qsort_large_partitioned.bin qsort_small.bin qsort_small_dup.bin qsort_small_partitioned.bin

small: qsort_small.bin qsort_small_dup.bin qsort_small_partitioned.bin
large: qsort_large.bin qsort_large_dup.bin qsort_large_partitioned.bin

qsort_small.bin: qsort_small.c Makefile
	gcc -static qsort_small.c -O3 -o qsort_small.bin -lm

qsort_small_dup.bin: qsort_small_dup.c Makefile
	gcc -static qsort_small_dup.c -O3 -o qsort_small_dup.bin -lm

qsort_small_partitioned.bin: qsort_small_partitioned.c Makefile layout.ld
	gcc -static  -T layout.ld qsort_small_partitioned.c -O3 -o qsort_small_partitioned.bin -lm

qsort_large.bin: qsort_large.c Makefile
	gcc -static qsort_large.c -O3 -o qsort_large.bin -lm

qsort_large_dup.bin: qsort_large_dup.c Makefile
	gcc -static qsort_large_dup.c -O3 -o qsort_large_dup.bin -lm

qsort_large_partitioned.bin: qsort_large_partitioned.c Makefile layout.ld
	gcc -static  -T layout.ld qsort_large_partitioned.c -O3 -o qsort_large_partitioned.bin -lm

clean:
	rm -rf qsort_small.bin qsort_large.bin qsort_large_dup.bin qsort_large_partitioned.bin output*
	