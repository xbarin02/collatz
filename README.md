# Convergence verification of the Collatz problem

[![Build Status](https://travis-ci.com/xbarin02/collatz.svg?branch=master)](https://travis-ci.com/xbarin02/collatz)

## Details

This repository contains computer programs implementing a completely new approach to calculating iterates of <a href="https://en.wikipedia.org/wiki/Collatz_conjecture">the Collatz function</a>.
The trick is that, when calculating the function iterates, the programs switch between two domains in such a way that they can always use the count trailing zeros (ctz) operation and a small lookup table with pre-computed powers of three.
This approach differs significantly from the commonly used approach utilizing a space-time tradeoff using huge lookup tables.
Mathematical details on this approach are given [here](doc/ALGORITHM.md).
The programs can check 128-bit numbers.

## Contact
David Barina <ibarina@fit.vutbr.cz>

## License
This project is licensed under the terms of the [MIT license](LICENSE.md).
