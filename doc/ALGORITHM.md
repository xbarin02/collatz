For given task `N`, the convergence program runs the following algorithm
for each `n0` in the range `[2^N, 2^(N+1))` which is not filtered out by
some of sieves used for faster processing.

```
n = n0

while n >= n0 do
	n = n + 1
	alpha = ctz(n)
	n = n / 2^alpha * 3^alpha
	n = n - 1
	beta = ctz(n)
	n = n / 2^beta
```

where the `ctz(n)` is the number of trailing zero bits in binary
representation of `n`.

The sieves can be of two kinds: the power of two and power of three sieves.
The sieve `2^2` is used always, i.e. only `n0 == 3 (mod 4)` are tested.
In addition, the sieves `3^1` and `2^32` are used in the CPU implementation.

All `alpha`s occured during the convergence test of the range are summed
together to give raise the checksum (proof of work). These checksums are
recorded on the server.

The number of iteration of the above algorithm is called the number of
cycles. The maximum number of cycles for given interval is detected.

Finally, the maximum value of `n` occured during the convergence test for
given interval is also detected and recorded.
