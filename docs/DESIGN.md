# protoMOMxx Design Principles

protoMOMxx is a provisional reimplementation of MOM6 in C++, built on AMReX,
with TIM replacing FMS as the infrastructure layer. This document collects the
design principles and constraints behind it. It is a living document:
principles will be added, refined, or retired as the project matures.

## 1. Core constraints

These constraints define the project. Some pull against each other; the later
sections say how we resolve that.

- **Verifiable against MOM6.** protoMOMxx keeps MOM6's verification invariants
  and matches legacy MOM6 answers bit for bit wherever the numerics allow.
  Where that is not possible (e.g., on GPUs), we test for statistical
  consistency with MOM6 instead, e.g., with ensemble consistency testing (§5).
- **Accessible to scientists.** Contributors with limited C++ experience and no
  AMReX background should be able to read and write the science code (§4, §7,
  §11).
- **Familiarity.** Legacy MOM6 developers should feel at home in protoMOMxx
  (§3).
- **No inherited shortcomings.** We don't carry over MOM6's design
  shortcomings, or the workarounds Fortran forced on it (§2).
- **Modern C++.** We use C++ features (classes, templates) where they cost
  nothing at run time and make the code safer or clearer, and we manage the
  risks C++ brings (§4).
- **AMReX with an exit path.** Some decisions depend on AMReX, but the design
  stays general: eventually an abstraction layer (TIM) separates protoMOMxx
  from AMReX, though not from day one (§7).
- **Portability.** One source builds and runs on CPUs and on GPUs from
  different vendors.

When the later sections don't settle a conflict, we weigh verification
(including bit-for-bit parity with MOM6) first, then readability for
scientists, and performance last. This is a guideline, not a strict ranking:
For instance, a big performance gain justifies a small readability loss.

## 2. What we are leaving behind

Here, we identify MOM6's shortcomings so that we don't repeat them. They fall
into two groups: design shortcomings of MOM6, and workarounds that Fortran
forced on an otherwise well-designed code. The first group needs different
decisions; the second mostly goes away with the language change.

**Design shortcomings of MOM6:**

- **Hand-rolled OOP with module-level encapsulation.** The `*_CS` control
  structures imitate objects, and nearly all of them declare `private`
  components, but privacy stops at the module boundary. The modules run to
  thousands of lines, so each of their dozens of procedures can change any CS
  field, and invariants are not kept in a small set of methods. Lifecycle is by
  convention too: `init`/`end` pairs and `initialized = .false.` flags stand in
  for constructors and destructors.
- **Argument trains.** Signatures like `(G, GV, US, CS, ...)` hide
  dependencies, make interfaces brittle, and make refactoring expensive.
- **Limited unit-test reach.** MOM6 unit-tests its self-contained numerics
  (e.g., remapping, EOS), but the dynamical core and most parameterizations
  need a live `G` and `CS`, so they can only be tested in whole-model runs.
- **Dual memory modes.** MOM6 supports both symmetric and non-symmetric memory
  (and, through `MOM_memory.h`, static and dynamic sizing, the latter a
  vector-era performance feature). The duality has real uses, but every piece
  of code must be written, reasoned about, and tested under both conventions.
  protoMOMxx does not have to inherit it (§10).
- **Repetition.** MOM6 sometimes violates the DRY principle: `max_depth` is
  kept on both `G` and `GV`; the geographic extents are copied from `G` into
  the surface forcing control structure; `dyn_horgrid_type` and
  `ocean_grid_type` carry the same fields, with a copy routine between them
  (§10); a parameter read at several sites repeats its default and description
  at each one (the repeated read itself is fine, §12); and `PI` is recomputed
  locally in about twenty files. We avoid this where we can (§3).
- **Globally stateful infrastructure.** FMS keeps a lot of module-global state
  (the mpp clock table, diag_manager's single file registry, module init
  flags). Ensembles are supported, but only by splitting PEs across members
  with `ensemble_manager`; two independent model instances cannot share the
  same PEs in one process.

**Workarounds Fortran forced on MOM6:**

- **Runtime unit scaling (`US%`):** a runtime stand-in for dimensional
  checking, because Fortran has no practical way to carry units in its types
  (no generic dimension algebra, and wrapper types are not reliably free).
- **Code duplication:** without templates or generics, the same logic is
  repeated across ranks, types, and kinds. Over a hundred rank- or
  kind-specific procedures sit behind generic interfaces (`post_data`,
  `pass_var`, `register_restart_field`, `get_param`, the checksums, the
  reproducing sums). This is infrastructure code, where §11 allows templates,
  and for fields AMReX's rank-generic arrays remove most of it outright.
- **Optional arguments in place of defaults and overloads:** Fortran has no
  default argument values, so MOM6 carries over 2,700 `optional` arguments and
  over 2,000 `present()` tests, most of them `if (present(x))` boilerplate that
  only supplies a default. Default arguments, overloads, or option structs
  replace this.
- **Weak lifetime automation:** Fortran allocatables are freed automatically
  and F2003 has `final` procedures, but this is much weaker than C++ RAII, and
  MOM6 (like most Fortran of its era, when compiler support for finalization
  was unreliable) doesn't use it: control structures are mostly pointers,
  teardown is paired `*_init`/`*_end` routines, and `final ::` appears nowhere
  in MOM6 or TIM. In C++, automatic lifetime is the default.
- **Limited const-correctness:** Fortran has `parameter`, `protected`, and
  `intent(in)`, but no read-only view of runtime data like `const T&` and no
  `const` methods. With private fields and `const` accessors, a `const` object
  is read-only all the way down.
- **No standard containers or strings:** hand-rolled linked lists and
  fixed-size arrays (`MAX_FIELDS_`) where a `std::vector` or `std::map` would
  do; fixed-length `character` variables with `trim()` everywhere, and a module
  of hand-written string functions, where `std::string` would do.
- **Integers as enumerations:** choices like `OBC_DIRECTION_N = 100` or the
  regridding schemes are named integer constants, so nothing stops a boundary
  direction from being passed where a regridding scheme is expected. An
  `enum class` makes each set of choices its own type.

C++ mostly removes the second group; the units check (§6) is the exception,
since the language only makes a compile-time version possible. The first group
can come back in C++ just as easily, unless the principles below prevent it.

## 3. Resolving "familiar" vs. "no inherited shortcomings"

Familiarity means the same vocabulary and layout, not the same API.

- **Keep:** file names and module boundaries (`src/core`, `src/framework`,
  `src/initialization`, `config_src/{infra,drivers}`, `MOM_*` prefixes),
  concepts and names (`h`, continuity, barotropic/baroclinic split, ALE,
  `G`/`GV`), and the split of the model into subsystems. A MOM6 developer
  should know right away *where* they are.
- **Replace:** MOM6's calling conventions. Classes, constructors, and
  encapsulation replace the `(G, GV, US, CS, ...)` argument trains and the
  `*_CS` control structures. Ported modules map traceably to their MOM6
  counterparts, but their APIs can, and should, be better.
- **Data lives where it belongs,** not where MOM6 happens to produce or first
  need it. Some MOM6 placements and duplicates come from its initialization
  order, not from the data model (e.g., `max_depth` on both `G` and `GV`). Such
  a value gets one home here, and code that needs it elsewhere re-reads the
  parameter (§12) or takes it as an argument.

## 4. C++: opportunities and threats

The strengths and opportunities of C++ (strong types, RAII, templates,
containers, encapsulation) show up throughout this document. The threats matter
just as much, because most of the intended contributors are ocean scientists,
not C++ specialists.

**Threats:**

- **Template debuggability.** Deep templates produce unreadable compiler
  errors, slow builds, and debugger sessions that step through machinery
  instead of science. Expression-template libraries are the worst.
- **Hidden costs.** Implicit conversions, unintended copies, allocations, and
  operator overloading can hide expensive work in innocent-looking code. In
  Fortran, cost is mostly visible (array temporaries are the main exception).
- **Object lifetime bugs.** Dangling references and views (e.g., an `Array4`
  outliving its `MultiFab`, or a view into resized storage) cause silent
  undefined behavior.
- **Undefined behavior in general.** An unchecked out-of-bounds access corrupts
  memory silently in both languages, but C++ has more ways to reach undefined
  behavior (dangling views, uninitialized reads, signed overflow), and its
  optimizers assume it never happens.
- **The size of the language.** Without an agreed subset, every contributor
  brings their own dialect, and the code becomes hard for everyone to read.
- **Toolchain variance on HPC.** Compiler and GPU-compiler (nvcc, hipcc)
  support for newer C++ features is uneven, so the subset must be what the
  *weakest* required toolchain supports.
- **Over-abstraction.** OOP and generic programming invite abstraction for its
  own sake. Every abstraction must earn its place, judged by one question: does
  the science kernel stay readable to a scientist?

**Mitigations (binding, not advisory):**

- **Agreed C++ subset:** C++20, limited to what gcc, icpx, nvc++, and
  nvcc all accept. Within that, science code uses a small dialect close to
  MOM6's: free functions, plain structs and value-semantic classes, standard
  containers, and lambdas for `ParallelFor`. It avoids inheritance hierarchies,
  virtual dispatch, raw `new`/`delete`, and macros beyond AMReX's GPU
  annotations.
- **Views are short-lived:** an `Array4` (or any view into a field) is created
  inside the loop that uses it, and is never stored as a member or kept after
  that loop.
- **No hidden copies of large data:** fields and grids are moved or passed by
  reference (`MultiFab` is already non-copyable), and single-argument
  constructors are `explicit`.
- **Debug builds check bounds:** array views are bounds-checked in debug builds
  (`AMREX_DEBUG` or `AMREX_BOUND_CHECK`), like building MOM6 with bounds
  checking.
- **Template discipline** (§11): template machinery stays in `src/framework`;
  science code only uses it.
- **A debuggable units layer** (§6): it stays **shallow enough to debug**, and
  has a plain-`double` build mode for debugging and performance triage, the C++
  equivalent of running MOM6 with unit scaling off.

## 5. Verification invariants are the spec

protoMOMxx adopts MOM6's verification invariants as hard constraints, to be
enforced in CI. They force good design and correct implementation (no hidden
state, no order-dependent reductions, no uninitialized halos):

- **Layout independence:** bitwise-identical answers across PE layouts.
- **Restart exactness:** a restarted run matches a continuous run bit for bit.
- **Rotational symmetry:** answers don't change when the grid is rotated.
- **Dimensional consistency:** checked at compile time (§6), replacing MOM6's
  runtime scaling tests.

MOM6's `.testing` suite checks more than these four: symmetric vs.
non-symmetric memory, OpenMP thread-count invariance, robustness to NaN
initialization, DEBUG vs. REPRO build equivalence, and regression against
reference answers. We should adopt or drop each one deliberately. The
memory-mode test goes away by design (§10); the thread-count, NaN-init, and
build-equivalence tests likely carry over.

**Reproducibility is a mode, not a permanent cost.** The invariants must hold
in verification configurations. Production configurations may trade some of
them for speed where the physics allows, but not all: restart exactness holds
in production too, because production runs are restarted, and
layout-independent sums stay the production default, as in MOM6, with
non-reproducing sums as an explicit opt-out. Production may give up rotational
symmetry and bitwise parity with MOM6, e.g., through FMA contraction and
aggressive optimization flags. So:

- Reductions go through a **pluggable reduction service** (reproducing and fast
  versions behind one interface), chosen at build or run time, and never
  hand-coded in science code.
- The fast and reproducing paths share the same code structure, so verification
  exercises the production code.
- CI checks both: the invariants hold in verification mode, and fast mode stays
  physically consistent (not bitwise) with it. This is another place where
  ensemble consistency testing (see the fallback below) may be used.

The reproducing sum is *not* an FMS service. MOM6 implements it itself
(`src/framework/MOM_coms.F90`, the extended-fixed-point algorithm of Hallberg &
Adcroft 2014), using only plain integer reductions and a broadcast from the
infra layer, which TIM supplies (`config_src/infra/TIM/MOM_coms_infra.F90`).
protoMOMxx can port the algorithm directly, or use a library that implements
it.

**Parity harness:** Fortran MOM6 is assumed to exist indefinitely. Running the
same configuration side by side (same `MOM_input`, comparable
`MOM_parameter_doc`) and comparing the results should therefore remain possible
in the long term.

**Bit-for-bit parity with legacy MOM6, wherever possible.** The target is
bitwise, not approximate, wherever the numerics allow. B4b may stop being
possible for some configurations (e.g., on GPUs, whose math libraries differ
from the CPU ones, or once protoMOMxx's numerics deliberately depart from
MOM6's). For every configuration where it is still possible, it is the rule,
with these disciplines:

- **Source contract:** ported MOM6 expressions are copied operation for
  operation: same operations, same association and evaluation order, same
  intermediate quantities (e.g., MOM6's `dL_di` form of the zonal spacing, not
  an algebraically equivalent rewrite), and same constants (`std::numbers::pi`
  is bit-equal to MOM6's `4*atan(1)` under gnu, intel, and nvhpc, so we use
  it). Deliberate deviations are noted in a comment beside the expression.
- **No algebraic rewrites for small gains:** reassociating or folding constants
  in a ported expression should not cost b4bness for a small speedup. Rewrites
  that keep the sequence of floating-point operations (control flow, loop
  structure, data layout) are fine, especially when they make the code clearer
  or simpler.
- **Build contract:** the same source order is not enough. When protoMOMxx is
  compared with MOM6 for b4bness, both are built with value-safe floating-point
  flags: no FMA contraction and no value-unsafe optimizations. (Turning
  contraction off on both sides is simpler than matching it, since compilers
  fuse per expression.) The same flags apply to every library in the
  comparison, e.g., TIM and AMReX. Transcendental functions (`sin`, `cos`,
  `atan`, ...) are only reproducible bit for bit when both sides use the same
  math library, since IEEE-754 does not specify them, so a comparison also
  fixes the toolchain pair. Note: this does NOT constrain the flags protoMOMxx
  uses in production; it applies to parity comparisons only.

Order-dependent reductions are outside the pointwise contract and go through
the reproducing-sum service. Where exact parity is impossible, the parity
harness falls back to statistical consistency testing (e.g., ensemble
consistency testing, as used for CESM components), but for pointwise ported
arithmetic the default expectation is bit equality. Two rules come with the
fallback:

- **A difference is explained before it is accepted.** When answers stop being
  b4b, we should know with certainty why (e.g., a different math library on the
  GPU, or a deliberate numerical change), not just that they differ. A passing
  statistical test is not an explanation.
- **The fallback is per configuration, not a permanent switch.** Moving one
  configuration to statistical testing does not move the others. Every
  configuration that can still be b4b (model setup, machine, hardware, software
  stack, compiler) keeps its b4b test, and we keep striving for b4b wherever it
  is achievable.

**Answer changes:** protoMOMxx ports only the latest MOM6 answers, i.e., with
all of MOM6's bug fixes. The older answers that MOM6 keeps reachable through
its `*_ANSWER_DATE` and `*_BUG` parameters are not ported. So the MOM6 side of
a parity comparison also runs the latest answers: the default answer dates,
`ENABLE_BUGS_BY_DEFAULT = False` (its default is True, and many bug flags take
their default from it), and False for any bug flag whose own default is True.
When a module is ported, its answer-date and bug-flag reads are ported too, as
guards: a configuration that asks for an older answer aborts (§9) instead of
being silently ignored, and bug flags default to False. For its own answer
changes, protoMOMxx follows MOM6: a change that alters the answers of existing
configurations goes in behind an answer date or a bug flag, so those
configurations can keep their answers. The mechanism will be designed with the
first such change.

## 6. Dimensional consistency

MOM6 checks dimensional consistency at run time, by rescaling units (`US%`) and
comparing answers, because Fortran's type system cannot practically express
units. In C++, much of this check can move to compile time, e.g., with strong
types for dimensioned quantities, at little or no runtime cost. The form it
takes is still open (§18). Whatever we choose should meet the requirements
below, most of which come from earlier prototyping:

- **Early:** it should be in place before there are many science kernels.
- **MOM6's dimensions:** it uses MOM6's dimensional vocabulary (`L`, `Z`, `H`,
  `T`, `R`, ...), so the `[L T-1 ~> m s-1]` annotations already in the code map
  onto it directly. Note that MOM6 distinguishes quantities with the same SI
  unit, e.g., horizontal (`L`) and vertical (`Z`) lengths, and `H`, which is a
  length or a mass per area depending on `BOUSSINESQ`.
- **Expressive enough for the numerics:** products and quotients of quantities,
  square roots (fractional exponents), and the difference between a position
  and a displacement (e.g., an interface height vs. a layer thickness).
- **Portable:** it compiles in device code with every supported toolchain (§1).
  Not every units library does, so this is worth checking before adopting one.
- **No runtime cost, verified:** typed and plain builds produce the same
  answers bit for bit, and the zero-cost claim is checked against the generated
  code rather than assumed.
- **Readable errors, fast builds:** a dimension error points at the offending
  expression with a short message, ideally in MOM6's notation (e.g., `[L T-1]`
  vs. `[H]`), and the layer adds little to compile time (§4).
- **Strippable:** a build mode reduces everything to plain `double`, for
  debugging and performance triage (§4).

## 7. Science kernels and the AMReX boundary

A **kernel** computes over a box from array views and index extents only (i.e.,
`amrex::Array4` and `amrex::Box` today). The **harness** around it loops over
boxes, fills halos, posts diagnostics, and launches kernels; all infrastructure
calls happen there. A one-off loop, as in the initialization code, may keep
both in one place. How fields are stored and handed to kernels is still open
(§18), and these rules should hold for any choice:

- **No MPI, no parameter reads, no diagnostic calls** in kernels.
- **GPU-compatible:** no virtual dispatch, no allocation, no exceptions in
  device code.
- **Visible and inlined:** code a kernel calls is defined in the same file, or
  in a header when several files share it. Across files, GPU code needs
  relocatable device code and is not inlined without device link-time
  optimization, and on CPUs, kernels that are not inlined run much slower.
- **Pointwise functions are optional:** arithmetic can be pulled out into a
  small function of scalars (e.g., the PPM limiters in TIM's `mom/cpp`) when
  several kernels share it or it is worth unit-testing alone. Otherwise it
  stays in the kernel, since extra layers only add indirection.

This gives unit-testable kernels (MOM6 unit-tests only its self-contained
numerics) and GPU portability, and makes the kernel signature the AMReX boundary
for the numerics.

**The boundary:** the eventual target is AMReX contained in TIM, with
`config_src/infra` (the protoMOMxx-side wrappers over TIM) as the only
exception, and only for a really good reason. Until then, the team agreed to
use AMReX types directly (e.g., `amrex::Real`) to move fast, keep a thin
boundary where that is cheap, and reassess the structure once ~1,000 lines of
non-trivial AMReX code exist. Harness code counts in that reassessment as much
as kernels, since it is often the first thing a new contributor writes (e.g.,
an idealized topography). Abstractions should evolve with the code rather than
be retrofitted later.

**What `amrex::` signals:** a reader who sees `amrex::` in the code should be
able to assume it has to do with infrastructure: distributed fields, box
arithmetic, `ParallelFor`, or device portability. So incidental facilities the
library also happens to ship (math constants, `min`/`max`) come from the
standard library instead (§15), and `amrex::Real` should give way to a MOM-side
real type once one exists.

## 8. No implicit global state

FMS is full of singletons (mpp, diag_manager, clocks). A singleton assumes one
model per process, so a second instance, concurrent (e.g., a nest) or in
sequence (e.g., a test that builds several models), cannot be accomplished (§2).

- Every subsystem (parameter table, diag mediator, domain) is an object passed
  down explicitly, never a global or a singleton.
- Design for **several model instances at once** from the start.
- This also applies to what protoMOMxx requires of TIM, and TIM's C++ layer
  satisfies it by design (§15).
- **One declared exception: the logger.** `MOM::logger` is a static-member
  global, accepted for convenience (passing a logger handle through every call
  defeats its purpose) and because logging holds no model state. The exception
  covers logging only. Ensemble members on separate PEs only need the instance
  in the log file names. When several instances share one process, log output
  has to be routed per instance; then the logger becomes instance-based with a
  global default, and the call sites don't change.

**AMReX has global state too.** `amrex::Initialize`/`Finalize`, the global
`ParmParse` table, `ParallelDescriptor`'s communicator, and the default memory
`Arena`s are all process-global. (The newer `amrex::AMReX` instance class holds
almost no state, so these globals remain.) So:

- AMReX initialization and global configuration are wrapped in one place
  (`TIM::Runtime`, through `MOM::Infra` in `config_src/infra`) and never
  touched from science code.
- Runtime parameters go through the MOM parameter system (§12), not
  `ParmParse`.
- The multiple-instance requirement must be *tested* against AMReX's actual
  behavior early. This is one of the questions for the §7 reassessment.

## 9. No silent failures

- **Error handling policy:** Host-side initialization and configuration errors
  **throw**: `logger::fatal` logs the message and throws `FatalError`, which
  the driver catches at the top level and turns into a nonzero exit. Device and
  kernel code cannot throw and uses `amrex::Abort` (or `AMREX_ASSERT` in debug
  builds). The kernel launch (§7) is where one policy ends and the other
  begins. §15 describes how this works with TIM, which uses no exceptions.
- **Deferred features are guarded, not just commented:** when a ported code
  path leaves out a branch that a runtime parameter can still select (a
  coordinate option, a mode flag like `BOUSSINESQ`), the gap is an executable
  `logger::fatal` at the branch point, not a `defer:` comment, which would let
  the wrong branch run silently. Guards are **layered**: every site that
  depends on the missing choice has its own fatal, even when an earlier guard
  makes it unreachable (e.g., the non-Boussinesq fatal in
  `set_coord_from_gprime`, behind the `VerticalGrid` constructor's guard).
  Lifting one guard then stops the model at the next site that hasn't been
  handled, so implementing a deferred feature is a walk from fatal to fatal,
  not an audit of comments. The cost: a guard behind another guard is never
  exercised by a test, so it has to be re-read, not trusted, when the guard
  above it is lifted. A `defer:` comment alone is for a whole subsystem that is
  absent and has no reachable branch to guard. Settings that are *permanently*
  unsupported and can't change answers (indexing conventions, decomposition and
  I/O-layout hints) are read with `unsupported_param` and produce a warning,
  because a fatal would promise an implementation that is never coming. A
  parameter that is set but never read, including one whose branch was not
  taken (e.g., TOPOG_SLOPE_SCALE with TOPO_CONFIG = flat), is caught by the
  unused-parameter check of §12, not by a guard.
- **Constructor arguments:** value objects with more than a few constructor
  parameters take a spec struct filled with designated initializers
  (`TIM::DomainSpec`, `MOM::DomainSpec`), the C++ equivalent of the Fortran
  keyword arguments MOM6's `*_init` routines use. Long runs of same-typed
  positional arguments get swapped silently and turn into physics bugs. Naming
  every value at the call site keeps configuration reviewable, makes defaults
  explicit, and replaces sentinel values (0 = "one box per rank") with
  `std::optional`. Specs are plain aggregates (`GridExtents`, `TopoSpec`,
  `RotationSpec`, `FieldSpec`), one per setup concern. They are not classes
  with a parameter-reading constructor: such a class would need a default
  constructor for the plain-values path, which makes an unusable object
  possible and moves validation out of construction (§14). The functions that
  read parameters into them are free functions.

## 10. One memory convention; staggering in the type system

- **One memory convention** (effectively symmetric). AMReX's index types carry
  what MOM6's symmetric/non-symmetric modes and `MOM_memory.h` express, so code
  is written and tested once (§2).
- **One horizontal grid type,** unlike MOM6, which has `dyn_horgrid_type` and
  `ocean_grid_type`. The grid-rotation test (§5) will need a hook here
  eventually; we note where, but don't build it ahead of need.
- **Staggering** uses AMReX's nodal and face-centered index types, with
  `TIM::Stagger` (Cell/XFace/YFace/Node, §15) as the vocabulary. AMReX nodality
  is a runtime value, so a u-point and an h-point MultiFab have the same C++
  type, and a mix-up is only caught at run time. Compile-time safety would need
  thin wrapper types (§18).
- **Index origin:** AMReX puts a face or node on the low (southwest) side of
  its cell, MOM6 on the northeast side. Only the labels shift: MOM6's `u(I=i)`
  is AMReX's node `i+1`, so `uh(I) - uh(I-1)` becomes `uh(i+1) - uh(i)`. Values
  follow the physical position, never the label.
- **Staggering is stated once,** where a field is created (`make_h_field`,
  `make_u_field`, etc.).

## 11. Template discipline

Most contributors are ocean scientists. Templates should only be used for
zero-cost abstraction that kernel authors don't see.

- Template machinery lives in `src/framework`; science code only *uses* it. A
  scientist must be able to read and write a kernel without understanding the
  templated machinery behind it.
- §4 lists the debuggability requirements for any template layer.

## 12. Self-documenting parameters with a lifecycle

MOM6's `get_param` and `MOM_parameter_doc` self-documentation is one of its
best features, and we keep it (`src/framework`). We add a defined way to
*retire* options, so the parameter space stays manageable as the model grows.

- **Keep:** default logging, generated parameter documentation, and detection
  of unused or misspelled parameters. The first two exist. Unused-parameter
  detection is still to come, as the equivalent of MOM6's `close_param_file`
  check, run by the driver after initialization (`REPORT_UNUSED_PARAMS` warns
  by default, `FATAL_UNUSED_PARAMS` aborts on request); §9 relies on it.
- **Add:** deprecation metadata (introduced-in, deprecated-in, obsolete) and a
  policy for removing deprecated options.
- **Reading rules:** MOM6 parameters keep MOM6's names, defaults, units, and
  descriptions verbatim, except the bug-flag defaults (§5), and stay unlogged
  where MOM6 doesn't log them. A parameter may be read wherever it is needed
  before the time loop (§14). The doc writer merges repeated reads and warns if
  their units or defaults differ, so nothing has to be passed around just to
  avoid a second `get`.

## 13. Testability as a design requirement

Every module can be constructed and tested on its own (constructor injection,
no hidden dependencies). The per-module unit tests under `tests/` enforce this,
and new modules are expected to come with them. In MOM6, unit tests reach only
the self-contained numerics (remapping, EOS, reproducing sums, a few
parameterizations); the dynamical core and most parameterizations need a live
model. protoMOMxx extends unit testing to all of it, as a direct result of §7
and §8.

Conventions: a test checks what its own layer adds and leaves lower-layer
behavior to that layer's tests (e.g., ghost widths belong to TIM's tests). A
header with no MPI or AMReX dependency is tested without starting the runtime.

## 14. Object lifecycle and initialization

These conventions replace MOM6's `*_init`/`*_end` pairs and `initialized =
.false.` flags (§2):

- **Constructors fully initialize.** If an object exists, it is usable: no
  two-phase initialization, no `init()` methods, no `initialized` flags. This
  applies to classes. A plain struct (`State`) is created and filled by its
  `make_*` factory and not used before that.
- **Class or struct:** a type is a class when it has an invariant to maintain,
  and a plain struct when its members can change independently (C++ Core
  Guidelines C.2). E.g., `VerticalGrid` derives `Rlay` from `g_prime` and
  validates its values, and `Grid` is read-only after construction, so both are
  classes; `State` is written by the time stepping every step, so it is a
  struct. Trivial getters and setters that hand out modifiable references are
  avoided (C.131); the owner of a struct controls write access to it.
- **Dependencies come through the constructor** (§13): a class gets the
  parameters, domain, and grids it needs as constructor arguments. It never
  reaches for globals (§8) or reads files it wasn't given.
- **Who reads the parameters depends on the type:**
   - a type that must be constructible from plain values (e.g., `TIM::Domain`,
     which never sees `MOM_input`) takes a spec struct, and a `make_*` factory
     on the protoMOMxx side reads the parameters and fills it;
   - a type that several setup functions fill step by step (`Grid`) takes the
     filled precursor struct by move and checks that it is complete; the reads
     live in the setup functions, and a `make_*` factory calls them;
   - a type with a single producer (`VerticalGrid`) reads its own parameters in
     its constructor, as MOM6's `verticalGridInit` does, with no spec and no
     factory.
- Note: three ways of constructing a type will be confusing and hard to enforce
  as the model grows. `State` turned out to be the plain-struct case above,
  filled by a factory (`make_state`), so the remaining question is mostly
  between the `Grid` and `VerticalGrid` styles; the forcing types should help
  settle it.
- **Construction failure is an exception** (the host-side policy of §9), so
  there are no half-built objects to check for.
- **Teardown is RAII.** Explicit `*_end`-style routines exist only where an
  external resource needs ordered shutdown (e.g., closing parallel I/O before
  finalizing the communicator), and even those should be driven by destructors
  where possible.
- **MOM6 invariant carried over: the parameter file closes before the time
  loop.** Parameters are read during construction, and nothing reads the
  parameter table after initialization. MOM6 enforces this by convention
  (`close_param_file` in the driver); in C++ the driver can enforce it by
  scope, letting the parameter object go out of scope (or locking it) once the
  model is built.

## 15. Division of infrastructure work: TIM, AMReX, protoMOMxx

TIM replaces FMS as the infrastructure layer, but any given service could
plausibly come from TIM, AMReX, or protoMOMxx. The division:

- **Process startup: a TIM service, with the mode chosen by the driver.**
  `TIM::Runtime` owns MPI and AMReX startup and ordered shutdown (RAII), with
  two modes: *owner* (calls `MPI_Init`/`MPI_Finalize`) and *guest* (adopts a
  communicator it is given, for CESM/NUOPC and ensembles, and never finalizes
  MPI). The solo driver uses owner mode through `MOM::Infra`, a thin shim kept
  for the familiar `MOM_infra_init` name and as the home for future
  MOM-specific communicator policy. This restores MOM6's convention, where the
  infrastructure layer (`fms_init`/`mpp_init`) owns MPI for the model, in
  object-based, singleton-free form. Shutdown order lives in `TIM::Runtime`'s
  destructor (`amrex::Finalize` before `MPI_Finalize`, with parallel I/O
  shutdown before both, §14).
- **From AMReX:** memory arenas, `MultiFab`, and host/device execution
  (`ParallelFor`), wrapped per §7 and §8. Domain decomposition and halo
  exchange (`BoxArray`, `DistributionMapping`, `Geometry`, `FillBoundary`) are
  behind `TIM::Domain` (`make_field`, `pass_var`).
- **From TIM's C++ layer** (namespace `TIM`, called directly, not through the
  Fortran `bind(C)` shims that exist for legacy MOM6): the horizontal domain
  (`TIM::Domain`, `TIM::Stagger`), time and calendar (`TIM::Time`,
  `TIM::Duration`, `TIM::Date`, `TIM::Calendar`), parallel netCDF I/O and
  diagnostics (both planned, no C++ code yet), checksums (`TIM::checksum`),
  and, once the §5 reproducing-sum service is
  built, the plain reductions beneath it. TIM's C++ layer is object-based and
  singleton-free by design (communicators and calendars are passed explicitly),
  so it satisfies §8.
- **Owned by protoMOMxx:** the runtime parameter system and its
  self-documentation (§12), logging, and directory/namelist handling, in
  `src/framework`.
- **TIM's scope:** TIM is limited to what MOM6 needs (legacy and protoMOMxx);
  otherwise we carry features we won't need, use, or test. `tim/cpp` stays
  MOM-free so protoMOMxx can use it on its own. The layer that lets legacy MOM6
  call TIM is permanent; the C++ kernels extracted into `mom/cpp` are a
  separate, temporary component.
- **Kernel ownership:** the C++ kernels extracted from MOM6 live in TIM's
  `mom/cpp` for now, where the bridge lets legacy MOM6 run them. Their
  permanent home is protoMOMxx: they move there once protoMOMxx's field and
  units types are in place, and `mom/cpp` retires along with the bridge. So
  they follow §7's kernel rules from the start, which makes the move cheap, and
  the move is a chance to drop layers of indirection they no longer need.
- **Shared vocabularies:** don't define protoMOMxx copies of types TIM already
  provides; alias the TIM type (e.g., `MOM::Stagger = TIM::Stagger`).
  `TIM::Stagger` (Cell/XFace/YFace/Node) is the staggering vocabulary of the
  whole stack (§10), and TIM's time types are its time vocabulary, so both
  models share one vocabulary. This does not extend to configuration: parsing
  and validating protoMOMxx's own inputs (e.g., a calendar name in `MOM_input`)
  stays on the protoMOMxx side, as MOM6 does relative to FMS. TIM never sees
  `MOM_input`.
- **Vocabulary:** protoMOMxx uses MOM6 vocabulary. TIM uses AMReX vocabulary;
  for concepts AMReX doesn't name, MOM6 vocabulary; and for concepts neither
  names, FMS vocabulary, or its own as a last resort, with the MOM6/FMS
  correspondence documented. This covers concept names, not identifier
  spelling, which follows each repo's convention. When a word means different
  things in AMReX and MOM6 (e.g., "level"), TIM uses the AMReX meaning and
  avoids the word for the MOM6 concept (hence `nk`, not `nlevel`). The
  translation happens in protoMOMxx's wrappers (e.g., `make_h_field` over
  `Stagger::Cell`).
- **Standard library first:** when the C++ standard library has the exact
  facility, use it instead of the AMReX version (e.g., `std::numbers::pi_v`
  over `amrex::Math::pi()`, `std::min`/`std::max` over
  `amrex::min`/`amrex::max`, and `std::abs` over `amrex::Math::abs`), since the
  AMReX versions forward to `std` or exist for backward compatibility. Every
  distinct AMReX facility in use adds to what the AMReX reassessment must
  review, so incidental uses are avoided, even in files that already include
  AMReX headers. The exception is an AMReX version that exists for portability,
  i.e., because the `std` one doesn't work correctly on some device backend.
  That case should be rare: use the AMReX version only after confirming the
  `std` one fails on a supported backend, and document.
- **Error composition:** TIM's C++ layer uses no exceptions (a rank that throws
  inside an MPI collective hangs the job, and device code cannot throw).
  Recoverable outcomes come back as return codes or
  `std::optional`, which protoMOMxx turns into its own policy (§9) at the call
  site; unrecoverable conditions abort through `TIM::abort`, which calls
  `MPI_Abort`. protoMOMxx accepts aborts during infrastructure startup (a
  failed startup ends the process anyway); its own throw-based policy applies
  to protoMOMxx host code, and exceptions never cross the TIM boundary in
  either direction.
- **Version discipline:** protoMOMxx pins a TIM commit and updates it
  deliberately, and one AMReX version serves the whole stack: TIM's `mom/cpp`
  kernels and protoMOMxx must link the same AMReX.
- **Build:** protoMOMxx builds TIM's `tim/cpp` as a CMake subproject
  (`TIM::tim`) from a pinned submodule, with a `TIM_SOURCE_DIR` override so a
  stack build (e.g., turbo-stack) can supply its own TIM checkout. TIM's MPI
  and AMReX lookups are guarded, so the consumer's AMReX is the one used.

## 16. Documentation and comment style

Doc comments, design notes, and PR and commit descriptions use plain,
matter-of-fact prose. Say what the code does and give the reason in the same
sentence ("..., but the grid itself should be immutable, so ..."). When
comparing with MOM6, state what MOM6 does and what we do as facts, and let the
reader draw the conclusion. Avoid essay-style phrasing, rhetorical emphasis,
and long dash-linked sentences; prefer short declarative ones. A good doc
comment reads like a colleague explaining the design across a desk, not like a
paper abstract. The comments in `src/types/MOM_grid.hpp` and
`src/types/MOM_grid_fields.hpp` are the reference example.

A PR that changes a design decision recorded here, or adds a new one, updates
this document in the same PR.

## 17. Source layout and library graph

Each source directory is one CMake library, and the dependency graph has no
cycles: `config_src/infra` (over `TIM::tim`) <- `src/framework` <- `src/types`
<- `src/initialization` <- `src/core` <- the driver. Fortran MOM6 is a single
link unit, which hides its directory-level cycles (the core calls
parameterizations, which use the core's grid and variable types; initialization
fills containers the core owns). `src/types` is our one addition to MOM6's
layout, for this reason:
it holds the containers that initialization fills and everything above reads
(`Grid`, `VerticalGrid`, `State`, and later the forcing types), which MOM6
keeps in `src/core`. Otherwise we keep MOM6's file homes, and each
configuration dispatch lives where MOM6 puts it (e.g., `GRID_CONFIG` in
`MOM_grid_initialize`, `TOPO_CONFIG` in `MOM_fixed_initialization`), together
with the parameter reads it needs, so adding a parameter to a concern touches
one module, as in MOM6.

## 18. Open questions

Questions the design hasn't settled yet, each with the point by which it needs
an answer.

- **Vertical coordinate data layout:** ALE remapping works column by column;
  AMReX works in 3D boxes. How columns are traversed, whether `nz` is fixed at
  compile time or at run time, and how remapping interacts with tiling and GPU
  launches is the biggest AMReX-fit risk in the project. **Prototype ALE
  remapping over MultiFabs early**, before the porting order forces the
  decision.
- **Mesh refinement scope:** regional mesh refinement (static at the least,
  possibly adaptive) is one of AMReX's main strengths and something the MOM6
  community strongly wants, so we need to keep it in mind. Refinement touches
  everything (per-level grids and fields, coarse-fine interpolation,
  conservative flux matching at coarse-fine interfaces, load balancing,
  restart), so it is easier to accommodate before the single-level types
  (`Domain`, `Grid`, `State`) have many users.
- **Units layer:** which mechanism meets the requirements of §6: compile-time
  strong types (in-house or from a library), MOM6-style runtime rescaling, or a
  mix, e.g., types for kernel arithmetic and a rescaling test as a backstop. It
  also decides how units reach the kernels (typed array views, or typed values
  inside the kernel), so it interacts with field storage and access below. It
  needs an answer before there are many science kernels (§6).
- **AMReX boundary:** the §7 reassessment, including whether several model
  instances can coexist with AMReX's process-global state (§8).
- **Stagger types:** the shape of the thin wrapper types for compile-time u/h
  safety (§10), and the hook the grid-rotation test needs, when that test is
  built.
- **Field storage and access:** whether each field is its own MultiFab (as
  today) or several fields share one MultiFab as components (e.g., grouped by
  staggering), and how code gets at them: plain functions, templates over a
  component enum, or accessors generated with C++ reflection. Grouping can cut
  boilerplate in harness code and halo exchanges, and can carry the staggering
  in the type (§10); the newer-language options depend on toolchain support
  (§4). It needs an answer before many kernels are written against the current
  one-MultiFab-per-field form.
- **2-D fields:** a 2-D field is a single-level MultiFab (`nk = 1`) indexed
  with a literal `k = 0`, because `Array4` requires the third index. So a
  field's rank is only visible where it is created, and nothing stops a 2-D
  field from being read in a truly 3-D loop. How to make the rank explicit
  (e.g., a 2-D view type, or a slab box in the loop) is open, together with the
  layer vs. interface (`nk` vs. `nk+1`) distinction. It needs an answer with
  the first type that mixes 2-D and 3-D fields, e.g., the forcing types or the
  barotropic fields of the dynamics.
- **Construction styles:** converging the `Grid` and `VerticalGrid` styles
  (§14), once the forcing types show which one generalizes.
- **Answer-change mechanism:** designed with the first protoMOMxx change that
  alters answers, e.g., a bug fix (§5).
