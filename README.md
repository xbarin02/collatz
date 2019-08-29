# collatz
Simple tools that check convergence of the Collatz problem.

## Details

This repository contains computer programs implementing a completely new approach to calculating the Collatz function.
The trick is that, when calculating the function iterates, we switch between two domains in such a way that we can always use the count trailing zeros (ctz) operation and a small lookup table with powers of three.
Powers for small exponents in that lookup table are therefore precalculated, which leads to some acceleration.
This approach differs significantly from the commonly used approach utilizing a space-time tradeoff using huge lookup tables.
Mathematical details on this approach are given in [this MSE question](https://math.stackexchange.com/questions/3311547/alternative-formulation-of-the-collatz-problem).

## Performance

![Performance for 32-bit blocks](https://github.com/xbarin02/collatz/blob/master/figures/perftest.png)

The processing time of individual *N*-bit blocks (*2<sup>N</sup>* numbers in total) is independent of their magnitude.
For instance, the processing time of *2<sup>32</sup>* 128-bit numbers on Intel Xeon E5-2680 v4 @ 2.40GHz machine is about 32 seconds.
This corresponds to the throughput *1.34 &times; 10<sup>8</sup>* numbers per seconds.
The program is single-threaded and uses 128-bit arithmetic.

## Contact
David Barina <ibarina@fit.vutbr.cz>

## License
This project is licensed under the terms of the [MIT license](LICENSE.md).
