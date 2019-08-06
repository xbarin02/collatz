# collatz
Simple tools that check convergence of the Collatz problem.

## Details

This repository contains computer programs implementing a completely new approach to calculating the Collatz function.
The trick is that, when calculating the function iterates, we switch between two domains in such a way that we can always use the count trailing zeros (ctz) operation and a small lookup table.
Convergence for small indexes into that lookup table can be precalculated, which leads to some acceleration.
This approach differs significantly from the commonly used approach utilizing a space-time tradeoff using huge lookup tables.
The switching between two domains effectively reformulates the Collatz function to a mapping between pairs *(n,e)*, where *n* is an odd number and *e* is an exponent of three.
Moreover, the convergence for all *(n,e)* pairs below a certain limit can be precalculated, leading to huge speedup.

## Contact
David Barina <ibarina@fit.vutbr.cz>

## License
This project is licensed under the terms of the MIT license.
