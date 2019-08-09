# collatz
Simple tools that check convergence of the Collatz problem.

## Details

This repository contains computer programs implementing a completely new approach to calculating the Collatz function.
The trick is that, when calculating the function iterates, we switch between two domains in such a way that we can always use the count trailing zeros (ctz) operation and a small lookup table with powers of three.
Powers for small exponents in that lookup table are therefore precalculated, which leads to some acceleration.
This approach differs significantly from the commonly used approach utilizing a space-time tradeoff using huge lookup tables.
The switching between two domains effectively reformulates the Collatz function to a mapping between pairs *(n,e)*, where *n* is an odd number and *e* is an exponent of three.
Importantly, the convergence for all *(n,e)* pairs below a certain limit can be precalculated, leading to significant speedup.

## Results

Checking the convergence for all numbers below *2<sup>32</sup>* on Intel Xeon E5-2680 v4 @ 2.40GHz (single-threaded program) takes less than one second:

    $ \time ./collatz4 32
    0.76user 0.00system 0:00.76elapsed 99%CPU (0avgtext+0avgdata 1708maxresident)k
    0inputs+0outputs (0major+87minor)pagefaults 0swaps

Verifying the convergence of all numbers below *2<sup>38</sup>* using all 56 cores takes less than two seconds:

    $ OMP_NUM_THREADS=56 \time ./collatz4 38
    102.37user 0.05system 0:01.86elapsed 5504%CPU (0avgtext+0avgdata 2160maxresident)k
    0inputs+0outputs (0major+288minor)pagefaults 0swaps

Verifying the convergence of all numbers below *2<sup>40</sup>* using all 56 cores takes less than 20 seconds:

    $ OMP_NUM_THREADS=56 \time ./collatz4 40
    727.07user 0.01system 0:19.03elapsed 3819%CPU (0avgtext+0avgdata 2160maxresident)k
    0inputs+0outputs (0major+374minor)pagefaults 0swaps

## Contact
David Barina <ibarina@fit.vutbr.cz>

## License
This project is licensed under the terms of the MIT license.
