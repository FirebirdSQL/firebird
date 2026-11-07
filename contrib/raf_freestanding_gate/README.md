# Freestanding evidence gate V1 — Firebird #9251 experimental companion

> Scope: **experimental, isolated and opt-in**, not part of the upstream fix,
> not a Firebird server plugin, not a certified standards implementation.
> The producer fix stays in [PR #9274](https://github.com/FirebirdSQL/firebird/pull/9274).
> Firebird 4/5 backports remain separate.

## Boundary and portability

- Core language: C11, `stddef.h` + `stdint.h`; one .c and one .h.
- No allocation, file/network calls, syscalls, globals with mutable state,
  libc calls, exceptions, threading or host clock.
- All state changes and durable writes happen **outside** the core through
  explicit callbacks. The host test harness does use libc; the **core does not**.
- Source files are offered under the InterBase Public License 1.0, preserving
  the Firebird repository's existing licensing boundary. No external code
  or third-party cryptographic library is copied into the core.
- No change to `src/remote/protocol.cpp`, existing Firebird build rules, ABI,
  production logging or repository release branches in this experiment.

## Contract / typed states

```
request + nonempty evidence + parent digest + next sequence
    |
    v
preflight (read-only) ----fail----> BLOCKED receipt (if persisted)
    |
    v
atomic PREPARED receipt ----not definitely durable----> BLOCKED, no apply
    |
    v
apply -> verify ----------------success----> PASS terminal receipt
    |                                            |
    | failure                           terminal persist definitely rejected
    v                                            |
rollback -> verify_recovery <--------------------+
    | success
    v
ROLLED_BACK receipt
    | failure
    v
failback -> verify_recovery -> FAILBACK receipt or UNRECOVERABLE
```

Return codes from `persist`: **1** = confirmed durable, **0** = definitely
not appended, **-1** = indeterminate. An indeterminate terminal append
sets `UNRECOVERABLE` pending human/host reconciliation; it is NOT a
declaration of physical machine damage. Under an uncertain outcome, the
core does not guess which state was stored or silently perform a second
potentially conflicting action.

**Mandatory adapter duties:**

1. Authorize the change and prove an independent known-good rollback state
   before enabling `apply`; preflight MUST have no side effect.
2. `apply` MUST be reversible and idempotent. `rollback` and `failback`
   MUST be idempotent and implement distinct recovery strategies. Do not
   wrap irreversible production writes behind this API.
3. `verify_recovery` MUST check the **actual recovered/fallback state**,
   not just repeat the recovery callback's return code.
4. `persist` MUST atomically compare-and-swap (sequence, previous hash)
   at a durable append-only store; return 1 only after a readback-equivalent
   durability guarantee. A callback that always returns 1 is a TEST STUB,
   not a production adapter.
5. On crash after PREPARED, the host MUST reconcile in-flight effects and
   emit an append-only successor. The core does not provide process restart,
   power-loss recovery, concurrency control, or a watchdog.
6. The host MUST supply attributable, privacy-reviewed evidence bytes and
   retain them securely. A digest without archived original bytes cannot
   prove the meaning or authenticity of the evidence.
7. A SHA-256 chain detects accidental or unauthorized changes only when
   compared with a trusted anchor. It is **not a digital signature**, identity
   attestation, secure timestamp, or complete intrusion defense.
8. Never let a gate approve a deployment based solely on a host-level test.

## Receipt wire V1 (114 bytes, big-endian)

| Offset | Bytes | Field |
|---|---:|---|
| 0 | 4 | ASCII `RFG1` |
| 4 | 1 | format version = 1 |
| 5 | 8 | sequence |
| 13 | 1 | state |
| 14 | 32 | previous record SHA-256 |
| 46 | 4 | exact evidence byte count |
| 50 | 32 | SHA-256 of exact evidence bytes |
| 82 | 32 | SHA-256 of bytes [0,82) |

A successful transaction produces PREPARED + terminal records, linked by
the previous-digest field. `raf_receipt_valid` checks record integrity and
chain pointer; `raf_receipt_evidence_match` checks independently supplied
evidence against the stored digest. Internal format is a **custom versioned
binary schema**, not CBOR, JSON, RFC 8785 JCS or W3C PROV serialization.

## Standards mapping: target vs evidence

| Standard / framework | Applicable boundary | V1 result |
|---|---|---|
| RFC 6234 / SHA-256 | digest test vectors | Two known vectors + independent host tests; no FIPS certification |
| RFC 5424 | syslog header/message syntax | Bounded ASCII text formatter, using NILVALUE for missing clock/host; **NOT** complete syslog service |
| RFC 5425 (and updates) | secure syslog transport | **NOT IMPLEMENTED** — TLS is a host adapter responsibility |
| RFC 3339 | Internet timestamps | **NOT IMPLEMENTED** — host must supply verified time where available |
| RFC 8785 | canonical JSON | **NOT IMPLEMENTED** — receipts use binary schema |
| W3C PROV-O / PROV-DM | entities, activities, agent, derived evidence | **MAPPING PROPOSED**, exporter not implemented |
| IEEE 1012-2024 | V&V process traceability | Design intent only; formal V&V conformance not assessed |
| ISO/IEC/IEEE 29119-2:2021 | governed testing processes | Local adversarial cases, not a process conformance claim |
| 6 Sigma / DMAIC | improvement workflow | Method only; no DPMO measurement, capability index or certification |

Standards:
[RFC 5424](https://www.rfc-editor.org/rfc/rfc5424),
[RFC 5425](https://www.rfc-editor.org/rfc/rfc5425),
[RFC 6234](https://www.rfc-editor.org/rfc/rfc6234),
[RFC 3339](https://www.rfc-editor.org/rfc/rfc3339),
[RFC 8785](https://www.rfc-editor.org/rfc/rfc8785),
[W3C PROV-O](https://www.w3.org/TR/prov-o/),
[IEEE 1012](https://standards.ieee.org/ieee/1012/7324/),
[ISO/IEC/IEEE 29119-2](https://standards.ieee.org/ieee/2416/7498/).

A future PROV adapter can map record -> `prov:Entity`, verification
step -> `prov:Activity`, verified external authority -> `prov:Agent`,
and evidence lineage -> `prov:wasDerivedFrom`. No agent identity is
invented by the core.

## DMAIC and adversarial falsification

- **Define / ψ:** precise invariant and risk; for Firebird #9251,
  continuation with zero BLOB ID MUST preserve the previously decoded kind.
- **Measure / χ:** source commit, current HEAD, failed packet-size case,
  host capabilities and baseline.
- **Analyze / ρ:** model corruption, missing BPB, altered default flags,
  ambiguous callbacks, timeouts, lost receipts and crash windows.
- **Improve / Δ:** smallest reversible change; freestanding gate stays
  separate from the protocol hotfix.
- **Control / Σ → Ω → ψ':** run adverse cases, verify receipt digests, read
  provider CI, freeze on uncertainty, and append superseding evidence.

The 21 C host cases include SHA-256 known vectors, chain-link validation,
evidence substitution, syslog size bounds, invalid/preflight-blocked input,
missing/indeterminate persistence, failed apply, failed verify, rollback,
independently verified failback, failed recovery verification, missing
callbacks and sequence overflow.

### Reproduce on host (tests only; not a production Firebird build)

```sh
cd contrib/raf_freestanding_gate
cc -std=c11 -O2 -ffreestanding -fno-builtin -Wall -Wextra -Wpedantic \
   -Wconversion -Wsign-conversion -Werror -Iinclude -c src/raf_gate.c -o /tmp/raf_gate.o
nm -u /tmp/raf_gate.o  # expected: no undefined symbols
cc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion \
   -Wsign-conversion -Werror -fsanitize=address,undefined \
   -fno-omit-frame-pointer -Iinclude src/raf_gate.c tests/test_raf_gate.c \
   -o /tmp/test_raf_gate
/tmp/test_raf_gate
```

Locally scoped: GCC 14.2 and Clang 17 both compiled the isolated
freestanding object with zero undefined symbols and passed 21 host cases
under ASan/UBSan. This is **PASS_SCOPED_HOST**, not Firebird CI or E2E.

## Integration gate / rollback policy

This experiment MUST NOT be copied into Firebird's master PR as a broad
unreviewed refactor. A proposed adapter for `src/remote/protocol.cpp`
requires maintainer permission, a separately reviewed architecture,
a scope-specific ABI compatibility check, packet-boundary BLOB regression
tests on Firebird 4/5/master, actual CI receipts, and a documented
runtime rollback/failback procedure. Until then:

```text
SOURCE != ARTIFACT != EXECUTION != EVIDENCE != CLAIM
CLAIM_ALLOWED=false
UPSTREAM_INTEGRATION=TOKEN_VAZIO
RELEASE_ALLOWED=false
```
