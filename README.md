# collatz
Distributed computing project to check convergence of the Collatz problem.

[![Build Status](https://travis-ci.com/xbarin02/collatz.svg?branch=master)](https://travis-ci.com/xbarin02/collatz)

## Details

This repository contains computer programs implementing a completely new approach to calculating the Collatz function.
The trick is that, when calculating the function iterates, we switch between two domains in such a way that we can always use the count trailing zeros (ctz) operation and a small lookup table with powers of three.
This approach differs significantly from the commonly used approach utilizing a space-time tradeoff using huge lookup tables.
Mathematical details on this approach are given in [this MSE question](https://math.stackexchange.com/questions/3311547/alternative-formulation-of-the-collatz-problem).
The processing time of individual *N*-bit blocks (*2<sup>N</sup>* numbers in total) is independent of their magnitude.
The programs use 128-bit arithmetic.

## Contact
David Barina <ibarina@fit.vutbr.cz>

## License
This project is licensed under the terms of the [MIT license](LICENSE.md).
