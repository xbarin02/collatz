# collatz
Simple tools that check convergence of the Collatz problem.

## Details

This repository contains computer programs implementing a completely new approach to calculating the Collatz function.
The trick is that, when calculating the function iterates, we switch between two domains in such a way that we can always use the count trailing zeros (ctz) operation and a small lookup table with powers of three.
Powers for small exponents in that lookup table are therefore precalculated, which leads to some acceleration.
This approach differs significantly from the commonly used approach utilizing a space-time tradeoff using huge lookup tables.
Mathematical details on this approach are given in [this MSE question](https://math.stackexchange.com/questions/3311547/alternative-formulation-of-the-collatz-problem).

## Results

Testing *2<sup>40</sup>* numbers from 0 up to *2<sup>40</sup>* takes approximatelly 3 minutes and 23 seconds on Intel Xeon E5-2680 v4 @ 2.40GHz machine.
This corresponds to the throughput *8.67 &times; 10<sup>10</sup>* numbers per seconds.
The program is single-threaded and uses 128-bit arithmetic.

    $ \time ./worker 0
    TASK_SIZE 40
    TASK_ID 0
    RANGE 0x0000000000000000:0000000000000003 .. 0x0000000000000000:0000010000000003
    HALTED
    202.99user 0.00system 3:22.99elapsed 99%CPU (0avgtext+0avgdata 1388maxresident)k
    0inputs+0outputs (0major+69minor)pagefaults 0swaps

Testing *2<sup>40</sup>* numbers starting from *2<sup>68</sup>* takes approximatelly 1 hour and 14 minutes on the same machine.
This gives the throughput about *3.99 &times; 10<sup>9</sup>* 128-bit numbers per seconds.
The program is still single-threaded.

    $ \time ./worker 268435456
    TASK_SIZE 40
    TASK_ID 268435456
    RANGE 0x0000000000000010:0000000000000003 .. 0x0000000000000010:0000010000000003
    HALTED
    4410.54user 0.00system 1:13:30elapsed 99%CPU (0avgtext+0avgdata 1344maxresident)k
    0inputs+0outputs (0major+67minor)pagefaults 0swaps

## Contact
David Barina <ibarina@fit.vutbr.cz>

## License
This project is licensed under the terms of the [MIT license](LICENSE.md).
