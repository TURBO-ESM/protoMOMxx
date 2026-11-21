# Design note: preserve answers, not structure

Status: adopted principle
Date: 2026-11-03

## Context

The kernels in `src/mom_kernels/` were ported mechanically from MOM6's Fortran.
Each C++ kernel has the same call structure, arguments and loop extents as the
Fortran routine it replaces. That made the port tractable and lets MOM6 call
the kernels through the `turbotmp_*_bridge` layer. MOM6's own tests, and the
capture-replay fixtures in `tests/test_mom/`, then check them bit for bit.

The MOM6 calling path will stay for at least four more years. Over that time the
native AMReX driver runs on hardware, and with data structures, that MOM6 was not
designed for:

- GPUs, where kernel launches, stream syncs and halo exchanges in the middle of
  a computation are expensive, and redundant arithmetic is cheap;
- per-MultiFab ghost widths, where MOM6 has one global halo;
- several boxes per rank, and AMR.

Copying MOM6's *structure* exactly sometimes produces poor designs in that setting.
The first clear case is the `pass_var` inside `horizontal_viscosity_tr` (TB-016).
Matching MOM6's exchange placement would mean splitting the whole routine into
phases to serve one option. Doing the exchange up front, wider, keeps the kernel
whole.

## The principle

**The MOM6 bridge path guarantees bit-for-bit answers. It does not guarantee
identical structure.**

The AMReX side may compute the same results in a different way when the hardware,
the data structures or the driver's design call for it, provided:

1. results in the valid (compute) region stay bit for bit identical to MOM6's;
2. the divergence is validated by comparing results (see below);
3. the divergence and its reason are documented where the kernel is defined.

Mechanical, structure-preserving porting stays the **default**. It is cheap,
reviewable, and keeps capture replay working. Diverge only when there is a concrete
reason.

## What must be preserved

Bit-for-bit answers constrain arithmetic, not code shape:

- **The operations and their order at each point.** Floating-point addition is
  not associative, so `(a + b) + c` must stay `(a + b) + c`. This covers the
  `-ffp-contract=off` / `-fno-fast-math` build flags too.
- **The order of every reduction**, e.g. vertical sums and the Newton iteration
  in `*_flux_adjust`. A different loop nesting is fine; a different summation
  order is not.
- **The input values each point sees**, including halo values. Getting a halo
  value by redundant local computation instead of an exchange is fine only if
  the computation is bitwise identical to what the neighbour computed.
- **Masking and edge behaviour.** MOM6 often leaves halos outside a non-periodic
  domain untouched, or zeroes them explicitly. A divergent algorithm that
  computes into those cells must give the same masked result downstream.
- **Option and bug flags**, including answer dates. They are part of the answer.

## What may differ

- **Where communication happens:** exchanges moved to the root of a
  communication-free subtree, made wider, or batched across layers.
- **Redundant computation into halos** to avoid exchanges.
- **Launch and loop structure:** fusion of kernels (TB-008), loop nesting, k-blocking,
  tiling, as long as the arithmetic at each point is unchanged.
- **Ownership and lifetime of scratch memory:** arena, caller-owned workspaces.
- **Argument lists:** dropping arguments a kernel does not need, bundling
  arguments into structs.
- **Index convention**, through views. The northeast stagger views
  (`northeast_stagger_views.md`) are a zero-cost way to keep the MOM6 index
  convention; they are not a reason to keep other structure.

## How divergence is validated

| Kernel relationship to the Fortran | Validation |
|---|---|
| Same structure and arguments (the default) | Capture replay in `tests/test_mom/`, plus MOM6 regression with `*_MODE=AMREX` |
| Same arguments, different internals (e.g. fused) | Capture replay of the **outermost** kernel whose arguments still match (e.g. `continuity_PPM`); leaf captures no longer apply to the fused path |
| Different inputs (e.g. needs wider halos) | Result comparison: run MOM6 configured to supply what the kernel needs (e.g. `NIHALO = NJHALO = 8`), then compare the AMReX path with plain Fortran on the compute domain. Or compare the native driver against MOM6 directly |

In every case the comparison is on the compute domain. Values in halos and
outside the physical domain may legitimately differ, and must be shown not to
reach the compute domain.

## Costs of diverging

Each divergence gives something up, so weigh it:

- **Capture replay may stop applying** to that kernel, and validation moves to
  coarser, more expensive result comparisons.
- **MOM6 may no longer be able to call the kernel** through the bridge in its
  standard configuration. For example, a kernel that needs halo 8 aborts at
  MOM6's halo 4.
- **Upstream MOM6 changes no longer port by transcription.** Each change to the
  Fortran routine has to be re-derived for the divergent version.

## Decision checklist

Before diverging from the Fortran structure, write down:

1. the concrete reason (hardware cost, data-structure mismatch, design problem);
2. why the structure-preserving port is worse, with a measurement where the
   reason is performance (TB-008's rule: profile first);
3. why the answers stay bit for bit identical, specifically which per-point
   operations and reduction orders are unchanged;
4. which validation from the table applies;
5. whether MOM6 itself would benefit. If so, change the Fortran too, so the
   structures converge again (TB-001 is an example).

## Examples

- **TB-016, Leith+E smoothing in `horizontal_viscosity_tr`:** the exchanges
  inside the computation are replaced by one wide exchange up front plus
  redundant computation. The Fortran is unchanged. It is validated against MOM6
  run with `NIHALO = 8`.
- **TB-008, launch fusion in the continuity orchestrators:** structure-preserving
  `*_point` fusion is preferred because it keeps leaf captures. Where that does
  not reach the performance target, deeper restructuring is allowed under this
  principle.
- **TB-001, `isd`/`ied`:** an unused argument removed in **both** MOM6 and the
  C++, because it is a defect on both sides. This is a structure change made at
  the source, not a divergence.
