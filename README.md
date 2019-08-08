# collatz
Simple tools that check convergence of the Collatz problem.

## Details

This repository contains computer programs implementing a completely new approach to calculating the Collatz function.
The trick is that, when calculating the function iterates, we switch between two domains in such a way that we can always use the count trailing zeros (ctz) operation and a small lookup table.
Convergence for small indexes into that lookup table can be precalculated, which leads to some acceleration.
This approach differs significantly from the commonly used approach utilizing a space-time tradeoff using huge lookup tables.
The switching between two domains effectively reformulates the Collatz function to a mapping between pairs *(n,e)*, where *n* is an odd number and *e* is an exponent of three.
Moreover, the convergence for all *(n,e)* pairs below a certain limit can be precalculated, leading to huge speedup.

## Results

Checking the convergence for all numbers below *2<sup>32</sup>* on Intel Xeon E5-2680 v4 @ 2.40GHz (single-threaded program) takes less than one second:

    $ \time ./collatz4 32
    0.72user 0.00system 0:00.72elapsed 99%CPU (0avgtext+0avgdata 1676maxresident)k
    0inputs+0outputs (0major+88minor)pagefaults 0swaps

Verifying the convergence of all numbers below *2<sup>38</sup>* using all 56 cores takes less than five seconds:

    $ OMP_NUM_THREADS=56 \time ./collatz4 38
    173.79user 0.00system 0:04.55elapsed 3813%CPU (0avgtext+0avgdata 2160maxresident)k
    0inputs+0outputs (0major+402minor)pagefaults 0swaps

## Contact
David Barina <ibarina@fit.vutbr.cz>

## License
This project is licensed under the terms of the MIT license.
