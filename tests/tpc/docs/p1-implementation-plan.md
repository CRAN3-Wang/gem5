# Member C P1 implementation plan

1. Audit the original proposal/assessment, paper IV-B/IV-D and Tables I-II,
   README contracts and existing C1 framework. Preserve the untracked originals.
2. Implement retirement-time X86/O3 register provenance using successful actual
   load values, dynamic sequence identities and explicit address operands. This
   deliberately trains later than the paper's decode taint circuit. Reject
   unsupported load forms; never infer dependence from numerical equality.
3. Add bounded/versioned P1 relations (four equal signed-offset observations),
   stream handoff and one-time distance multiplier feedback to T2. Controller
   creates P1-owned capabilities explicitly; T2 retains producer ownership.
4. Add exact-byte producer-return subscriptions for resident/pending/missing
   lines, with modeled readiness and context/security. Dependent VAs go through
   the existing translating queue to L1D, source 2. No functional memory reads,
   algorithm packet queue or private public-lifecycle accounting.
5. Add shuffled array-of-pointer guests and negative controls; directed tests
   cover confidence, dependencies, generation, return timing and translation.
   T2 fixtures remain outside production profiles; real T2 remains absent.
6. Run all 20 existing host tests; establish Python 3.11 and attempt real X86
   gem5 build and focused simulations. Record actual evidence and blockers.
7. Publish only a gem5 draft PR against `tpc-project`; never merge or update
   the parent gitlink. All new guests, focused tests and docs live in gem5.

Scope steering: the user superseded parent-project deliverables. All authored
parent source edits were restored; existing parent runners are reused unchanged.

Finite resources, split access policy, retirement/recovery policy and all
approximations will be documented with the final implementation. Full TP/TPC
performance evaluation is deferred until Member B supplies real T2.
