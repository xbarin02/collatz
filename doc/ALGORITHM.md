For a given task *N*, the convergence program runs the following algorithm
for each *n<sub>0</sub>* in the range
*N&times;2<sup>40</sup> &hellip; (N+1)&times;2<sup>40</sup>* which is not
filtered out by some of the sieves used for faster processing. Thus
*2<sup>40</sup>* numbers form a single work unit.

```
n = n0

while n >= n0 do
	n = n + 1
	α = ctz(n)
	n = n / 2^α * 3^α
	n = n - 1
	β = ctz(n)
	n = n / 2^β
```

where the `ctz(n)` is the number of trailing zero bits in binary
representation of `n`.

The sieves can be of two kinds: the power of two and the power of three sieves.
The sieves *2<sup>32</sup>* and *3<sup>1</sup>* are used in the CPU implementation,
and the sieve *2<sup>16</sup>* on the GPU.

All &alpha;s occurred during the convergence test of the range are summed
together to give raise the checksum (proof of work). These checksums are
recorded on the server.

The number of iteration of the above algorithm is called the number of
cycles. The maximum number of cycles for a given interval is detected and
recorded on the server.

Finally, the maximum value of *n* occurred during the convergence test for
a given interval is detected and recorded as well.
