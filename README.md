# collatz
Simple tools that check convergence of the Collatz problem.

This repository contains computer programs implementing a completely new approach to calculating the Collatz function.
The trick is that, when calculating the function iterates, we switch between two domains in such a way that we can always use the count trailing zeros (ctz) operation and a small lookup table.
Convergence for small indexes into that lookup table can be precalculated, which leads to a huge acceleration.
This approach differs significantly from the commonly used approach utilizing a space-time tradeoff using huge lookup tables.

# contact
David Barina <ibarina@fit.vutbr.cz>

# license
This project is licensed under the terms of the MIT license.
