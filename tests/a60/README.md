# Algol 60 conformance programs

Programs of Milestone 3, each with its expected output written by hand (`NAME.out`), input
(`NAME.in`) or a second unit (`NAME.unit.alg`) where it has one. `run_test` compiles and
runs them on x86_64, aarch64 and riscv64. `.a60` files are in the publication form.

| Program | What |
|---|---|
| revised_report | the procedures of the Revised Report 5.4.2 (Spur, Transpose, Step, Absmax, Innerproduct, euler) and the for list of 4.6.2 |
| man_or_boy | Knuth's man or boy test as he wrote it, k = 0…12 (k = 12 needs about 1 MB of stack) |
| classics | Ackermann, Euclid, eight queens, Hanoi, sieve |
| parameters | Jensen's device, conversions of name parameters, formal procedures, formals without specifications, arrays and labels by value |
| for | the for statement of 4.6: subscripted variable, element order, goto out |
| own | own variables and arrays |
| program_form | unsigned integer labels, `) text: (` delimiters |
| publication | the publication form |
| intdiv | `÷` |
| ifip | the IFIP input and output procedures |
| gdr_io | Algol-GDR units, formats, `_encode`, `_decode` (in run_test.cpp: a unit file of its own) |
| goto_arrays | arrays freed by non-local gotos (also run in 1 GB of address space) |
| units | separate compilation |

The Algol examples of `dubna` are in `../algol/`, run against the original's output.
Wichmann's Algol 60 test programs are not available here.
