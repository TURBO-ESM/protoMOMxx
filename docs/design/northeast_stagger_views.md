# Design note: calling the MOM kernels from the protoMOMxx driver (northeast stagger views)

Status: proposal (not implemented)
Date: 2026-11-03

## Problem

`src/mom_kernels/` holds AMReX ports of MOM6 kernels, generated mechanically from the
Fortran. MOM6 calls them through the `turbotmp_*_bridge` layer, and the results match
MOM6 bit for bit. That lets the existing MOM6 testing infrastructure validate them.

The kernels use **MOM6's northeast (NE) stagger convention**, shifted to 0-based
indices. For example, `uh(i) - uh(i-1)` is the divergence across cell `i`: `uh(i)` is the
east face of cell `i`.

The native protoMOMxx fields are `amrex::MultiFab`s built by `TIM::Domain::make_field`.
They use **AMReX's southwest (SW) convention**: nodal index `i` is the low side of cell `i`.
`MOM_domain_infra.hpp` currently documents that "transcribed stencils shift by one".

If the native driver passed raw SW storage to the kernels, every staggered access would be
off by one face. The results would look plausible and be wrong.

The MOM6 calling path is expected to live for at least four more years. Two options were
rejected:

- **Rewrite the kernels to SW.** MOM6 could no longer call them unchanged, so the MOM6
  bit-for-bit testing would be lost. Every future port would become a transform instead
  of a transcription.
- **A per-kernel adapter.** It would be a maintenance cost that grows with every kernel
  ported.

## Key observation

The SW/NE offset does not depend on the kernel. It depends only on the field's stagger:

| Stagger | MOM (NE) index `i` means       | Native SW storage index |
|---------|--------------------------------|-------------------------|
| h       | cell `i`                       | `i`                     |
| u       | east face of cell `i`          | `i+1`                   |
| v       | north face of cell `j`         | `j+1`                   |
| q       | NE corner of cell `(i,j)`      | `(i+1, j+1)`            |

An `amrex::Array4` is a pointer plus `begin`/`end`. Lowering `begin`/`end` by one along
each nodal direction gives `view(i) == storage(i+1)`. This is a zero-copy relabelling of
the same memory.

The Fortran bridge already does the equivalent: `make_array4` positions each array by its
own `lb - 1`. MOM6's symmetric-memory `u(IsdB:IedB)` therefore arrives in 0-based NE index
space. The kernels never see storage, only index meaning, so they cannot tell who called them.

Example: one box with 4 cells in x (i = 0..3) and a 2-point halo. The u field is nodal in x.

| Physical location                 | SW storage | NE view | MOM6 `I` |
|-----------------------------------|-----------:|--------:|---------:|
| west face of cell 0 (domain edge) | 0          | -1      | 0        |
| east face of cell 0               | 1          | 0       | 1        |
| east face of cell 3 (domain edge) | 4          | 3       | 4        |
| valid range                       | 0..4       | -1..3   | 0..4     |

## Decision (proposed)

**NE is the protoMOMxx kernel (stencil) convention. Storage stays AMReX-native SW.**

- Storage-level operations are unaffected, because they do not interpret index meaning:
  `FillBoundary` (`pass_var`/`pass_vector`), `OverrideSync`, `OwnerMask`/`sum_unique`,
  `ParallelCopy`, reductions and I/O.
- All stencil code sees NE views, whether it is ported or written natively.
- The convention is enforced by the type system, not by review discipline. Staggered
  fields expose stencil access only through the view.

## Implementation sketch

### 1. The view and typed fields: `config_src/infra/MOM_domain_infra.hpp`

`MOM::Domain` is documented as the home for MOM-specific domain policy. TIM stays
convention-agnostic.

```cpp
/// The MOM index convention: a staggered point (i,j) is the east/north face
/// (u/v) or NE corner (q) of cell (i,j), as in MOM6. Storage stays AMReX-native
/// (nodal point i = low side of cell i); this view moves the index origin so
/// that view(i) == storage(i+1) along each nodal direction. Zero-copy.
template <class T>
AMREX_FORCE_INLINE
amrex::Array4<T> mom_view(const amrex::Array4<T>& a, const Stagger s) {
  const amrex::IntVect n = TIM::nodality(s);       // u:(1,0,0) v:(0,1,0) q:(1,1,0)
  amrex::Box b(a);                                 // the storage index extent
  b.shift(-n);                                     // relabel: storage i -> view i-1
  return amrex::Array4<T>(a.p, amrex::begin(b), amrex::end(b), a.nComp());
}

/// A field whose stencil-facing accessors speak only the MOM convention.
template <Stagger S>
class Field {
public:
  explicit Field(amrex::MultiFab&& mf) : mf_(std::move(mf)) {}

  /// Northeast view for kernels. The only way stencil code reaches the data.
  amrex::Array4<amrex::Real> array(const amrex::MFIter& mfi) {
    return mom_view(mf_.array(mfi), S);
  }
  amrex::Array4<const amrex::Real> const_array(const amrex::MFIter& mfi) const {
    return mom_view(mf_.const_array(mfi), S);
  }

  /// Raw storage, for convention-blind operations only: halo exchange,
  /// OverrideSync, copies, reductions, I/O. Never index this in a stencil.
  amrex::MultiFab& storage() { return mf_; }
  const amrex::MultiFab& storage() const { return mf_; }

private:
  amrex::MultiFab mf_;
};

using HField = Field<Stagger::Cell>;
using UField = Field<Stagger::XFace>;
using VField = Field<Stagger::YFace>;
using QField = Field<Stagger::Node>;
```

To check during implementation:

- `Box(Array4)` is already used in `mom_continuity_ppm.cpp` (`Box h_box(h);`).
- The `Array4(p, begin, end, ncomp)` constructor is the one `turbotmp_helper.cpp` already uses.

Further changes in this header and its users:

- `Domain::make_{h,u,v,q}_field` return typed fields.
- `pass_var`/`pass_vector` take typed fields and act on `.storage()`.

### 2. The caller: new `src/core/MOM_continuity.{hpp,cpp}`

This is the analogue of MOM6's `MOM_continuity.F90`. It owns:

- the config structs (`reconstruction_CS_C`, `transport_adjust_CS_C`), read once from
  `RuntimeParams`;
- the halo contract;
- the MFIter loop.

`src/mom_kernels/` is unchanged.

```cpp
void Continuity::step(const UField& u, const VField& v,
                      const HField& hin, HField& h, UField& uh, VField& vh,
                      amrex::Real dt) const {
  // The kernel computes into halos instead of exchanging mid-call, so the
  // driver owns the halo contract (inputs exchanged by the caller beforehand).
  AMREX_ALWAYS_ASSERT(hin.storage().nGrowVect().min() >= required_halo_);

  for (amrex::MFIter mfi(hin.storage()); mfi.isValid(); ++mfi) {   // no tiling
    const amrex::Box bx0 = mfi.validbox();       // cell-centred core box
    const amrex::Box gbx = amrex::grow(bx0, hin.storage().nGrowVect());

    MOM::continuity_PPM(
        u.const_array(mfi), v.const_array(mfi),
        hin.const_array(mfi), h.array(mfi), uh.array(mfi), vh.array(mfi),
        dt, bx0, stencil_, x_first_,
        grid_.mask2dT().const_array(mfi), grid_.dy_Cu().const_array(mfi),
        grid_.IareaT().const_array(mfi), /* ...remaining grid fields, all via views... */
        gbx.smallEnd(0), gbx.bigEnd(0),          // isd/ied: derived, not passed in
        vgrid_.Angstrom_H(), vgrid_.H_subroundoff(),
        recon_cs_, adjust_cs_, /*obc=*/nullptr,
        por_u_.const_array(mfi), por_v_.const_array(mfi),
        {}, {}, {}, {},                          // uhbt, vhbt, visc_rem_u/v: absent
        {}, {}, /* ...BT_cont outputs absent until barotropic lands... */ {}, {});
  }
}
```

Notes on the loop:

- **One MFIter serves every field.** The u/v/q MultiFabs share h's BoxArray (converted)
  and DistributionMapping, so `uh.array(mfi)` returns the matching box.
- **Iteration boxes always come from the cell-centred valid box, never from a staggered
  fab's box.** Kernels build face loops internally, e.g. `growLo(bxC,0,1)`. In view indices
  that covers -1..3, which is exactly the u view's valid range.
- **2-D grid fields are stored at `k=0`.** This already matches the kernels'
  `mask2dT(i,j,0)` convention.
- **`isd`/`ied` are derived from the fab box**, not passed in as in the Fortran.

### 3. Other changes

| Location | Change |
|---|---|
| `MOM_domain_infra.hpp` | Add `mom_view` and `Field<S>`. Rewrite the class comment: storage is SW, all stencil code is NE. Remove the "transcribed stencils shift by one" paragraph. |
| `MOM_grid_fields.hpp`, `MOM_grid.hpp`, `MOM_state.hpp` | `amrex::MultiFab` becomes `HField`/`UField`/`VField`/`QField`. |
| `MOM_grid_initialize.cpp`, `set_masks` | The only existing SW stencil code; convert to views. E.g. `geoLonCu(i) = west_lon + dLon*i` becomes `west_lon + dLon*(i+1)`, and `maskCu(i) = maskT(i-1)*maskT(i)` becomes `maskT(i)*maskT(i+1)`. These now read like MOM6. |
| `src/core/MOM_continuity.*` | New. Called from the future `step_MOM`. |
| `src/mom_kernels/`, the bridge, `lessons.md` | Unchanged. |

Prerequisites the native path still needs (independent of this decision):

- `Grid` lacks `IareaT`, `IdxT`, `IdyT`, `dy_Cu`, `dx_Cv` and `por_face_areaU/V`.
- `VerticalGrid` lacks `H_subroundoff`.

### 4. Validation with existing captures

The `tests/test_mom` captures are already in NE index space. Add a test that:

1. loads a capture into SW-storage typed fields, shifting +1 on load along each nodal
   direction;
2. calls the kernel through `.array(mfi)`;
3. compares against the captured `_after` arrays.

This validates `mom_view`, the typed fields and the call layer against the same Fortran
results, with no new reference data. Land it before converting the grid-init code.

## Performance

**The views themselves are performance-neutral on CPU and GPU.**

- **Inside kernels:** `Array4::operator()` computes `(i - begin.x) + (j - begin.y)*jstride + ...`
  on GPU. On optimized CPU builds it uses the equivalent `i + j*jstride + ... - offset0`.
  A shifted `begin` changes the constants in that arithmetic, not the work done. Nothing
  else changes:
  - the data pointer, strides and memory layout are the same;
  - the access pattern is the same, so GPU coalescing and CPU vectorization are unaffected;
  - the lambda captures are the same size.
- **On the host:** one view is built per field, per box, per call. That is a few integer
  operations, negligible next to a kernel launch. `Field<S>` is a non-virtual, inline wrapper
  and adds no indirection.
- **Compared with the alternatives:** a copying adapter would add a full memory pass per
  field per call. Rewriting the kernels to SW would have the same cost as views (none).

**Performance issues that exist today, independent of this decision.** Address them when
the native continuity path is built:

1. **Scratch allocation.** Kernels allocate scratch `FArrayBox`/`IArrayBox` from
   `The_Arena()` and free it at scope exit, while their kernels may still be running.
   On GPU under an MFIter loop (multiple streams), memory freed on one stream can be
   reused by another while a kernel is still reading it.
   - **Chosen fix (backlog TB-007):** allocate the scratch from `The_Async_Arena()`,
     which frees in stream order. This is a one-token change per site, with no kernel
     signature changes, so MOM6 parity is kept. Add a rule to `lessons.md` so that
     future ports follow it.
   - A per-box `Gpu::streamSynchronize()` in the caller remains available as a debug
     switch only.
   - Peak scratch memory is about `min(boxes per rank, GPU streams)` x per-box
     scratch, dominated by `continuity_PPM`'s four whole-fab buffers.
2. **Launch count.** The orchestrators (`zonal_mass_flux` and similar) launch several
   separate kernels over scratch. `lessons.md` §9 already lists fusing the Newton iteration
   into a per-thread device function as a backlog item.
3. **CPU threading.** The kernels are box-level and grow their boxes internally, so the loop
   above uses no MFIter tiling. Without many boxes per rank there is no OpenMP parallelism
   inside a rank. Use more boxes per rank (`DomainSpec::n_boxes`) or revisit tiling later.
4. **The bridge path (MOM6 only).** `turbotmp_helper`'s "Fortran->C transpose" is an
   identity permutation, because `Array4` is already column-major. Each array is copied
   into a second buffer and a kernel is launched for no effect. On CPU builds the bridge
   could wrap the Fortran pointer directly (zero copy). On GPU it needs one H2D memcpy per
   array. This does not affect the native driver.

## When to revisit

Revisit when the MOM6 calling path is retired (no earlier than ~2030), or if native stencil
code starts to outweigh mechanical ports. Because the convention lives in `mom_view` and
`Field<S>`, switching then is one mechanical sweep, verified by the tests current at that time.
