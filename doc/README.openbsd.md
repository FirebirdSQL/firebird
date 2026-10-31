# OpenBSD port (experimental)

This port targets OpenBSD amd64 and i386 using the base C/C++ compiler and
the GNU Make build. Native build and runtime validation is required before
deployment. The top-level CMake build and other OpenBSD architectures are not
covered here.

Do not test against production databases, a running installation, or IPC files
left by an unofficial port that defined `NETBSD` or disabled process-shared
pthread attributes. Successfully compiling those workarounds does not establish
correct interprocess synchronization.

## Build and initial checks

Use a fresh checkout on a local filesystem and an unprivileged account. The
dependencies include a C++17 compiler, GNU Make, CMake, unzip, GNU libtool,
Autoconf, Automake/aclocal, ICU headers/libraries, libiconv and gettext; libatomic
may also be needed when the compiler uses out-of-line atomic operations.
The bundled tommath/tomcrypt sources are used by the check script. Dependencies
must already be installed; the script does not install packages or change limits.

From the repository root:

```sh
AUTOCONF_VERSION=2.72 AUTOMAKE_VERSION=1.18 JOBS=1 sh builds/posix/check-openbsd.sh
```

Select Autoconf/Automake versions that are actually installed. Package revision
suffixes such as `p0` do not belong in these environment variables. Make sure
there is sufficient space for sources, objects, test binaries and the staging
copy; a source archive's size is not a build-space estimate.

The script performs configuration, Release build, common/engine unit tests,
staging and a C client SDK compile/link/run check. It writes logs beneath
`.openbsd-check` and stops at the first failing step. It does not delete earlier
results, create accounts, start a listener, change SYSDBA credentials, modify PF
or install into `/opt`. Its configured prefix is `/opt/firebird-openbsd-test`,
but files are copied only beneath `.openbsd-check/stage` using `DESTDIR`.
The staging directory is not an activated installation.

After a source fix, keep the logs and resume individual build/test commands in
the existing tree, or repeat the script in another clean checkout. Do not rerun
a clean-install script over an active database server.

## Implementation decisions

| Area | OpenBSD implementation |
| --- | --- |
| Platform identity | Separate `OPENBSD` configuration, `OB` build prefix and `OsOpenBsd` database implementation value; no NetBSD impersonation. |
| Wire data | `arch_generic` uses canonical encoding. Equal generic architecture values must not enable symmetric/native-layout encoding. |
| Shared mutex | A companion `.mutex` file uses the existing `SharedFileInfo` thread coordination and kernel `flock` locking. The mapping file's lifetime lock is separate. |
| Shared event | A lock-free 32-bit generation counter and shared `futex` wait/wake; not process-private pthread objects. |
| ICU loading | OpenBSD's shared-library ABI suffix is not treated as an ICU release number. The header ICU major selects renamed entry points; runtime version validation remains in place. |
| Module ownership | Compare loaded objects, including for version-resolved names; do not accept a dependency's symbol as the requested module's export. |
| Direct I/O | Keep the filesystem cache by default. Reject explicit unsupported direct-I/O requests in the engine, gbak and nbackup. Forced writes are unchanged. |
| Build dependencies | Use the generated GNU libtool and actual `dlname` from the tommath/tomcrypt `.la` files instead of Linux soname guesses. Use the platform C++ runtime. |
| SDK | Install each public compatibility header independently; header installation is part of both server and client-only builds. |

The OpenBSD shared-memory header version is 3, distinguishing this layout from
the pthread-based version 2. The companion mutex is opened under the existing
mapping initialization lock and is removed with the mapping. If removing the
mapping fails, the mutex is retained. Kernel locks are released when the owning
process exits; existing shared-data recovery is still responsible for repairing
an interrupted update. Individual thread abandonment is not a supported recovery
mechanism. Performance and crash recovery need native stress testing.

Event waits use a monotonic deadline and recheck the atomic generation after
interrupts and timeouts. The futex is shared, so the mapping need not have the
same virtual address in each process. Modular generation comparison handles
counter wrap assuming fewer than 2^31 unobserved posts.

Auxiliary connections use the port bytes shared by supported sockaddr layouts
instead of interpreting a peer's OS-specific family field. Server replies retain
layout conversion for older BSD/Darwin clients. IPv6 normalization includes
OpenBSD's family value 24. Cross-platform event delivery still needs testing.

CRC32C's SSE4.2 requirement is limited to that routine/translation unit; the
existing runtime CPU dispatch is retained. Monotonic timing falls back to
`CLOCK_MONOTONIC` when `CLOCK_MONOTONIC_RAW` is unavailable. No W^X exceptions,
fake pthread success returns, asynchronous-write defaults or ICU-name bypasses
are part of the port.

## Required validation before release

- OpenBSD 7.9 amd64 and i386: Release and Debug builds; default and client-only
  configurations. Record exact source revision, toolchain and package versions.
- Run `OpenBsdIpcSuite`: thread/process exclusion, contended try-lock, process
  exit while holding a mutex, event timeout/wrap, request/reply loops and wakeup
  through a second mapping. Extend with
  sustained contention, signal interruption and process termination during real
  lock-manager and transaction-cache updates. A unit counter test is not a
  database crash-recovery test.
- Run `OpenBsdModuleSuite`: version-resolved ICU names and dependency-symbol
  rejection. Exercise actual UTF8 collations/time-zone loading and explicit ICU
  version selection, including unavailable versions.
- Create disposable databases and verify transactions, rollback, concurrent
  clients, backup/restore, forced writes and recovery after server termination.
  Test SuperServer, SuperClassic and Classic before claiming support for them.
- Check IPv4/IPv6 SQL and event subscriptions between OpenBSD, Linux and Windows,
  including older Firebird 5 clients. Unit sockaddr tests do not open connections.
- Confirm that explicit `UseFileSystemCache = false`, `gbak -DIRECT_IO` and
  `nbackup -D ON` fail clearly without changing an existing database/backup.
- Verify an installed/staged client and C/C++ SDK consumers without access to
  the original build directory. Check `ldd` for both the client and engine plugin.
  Build-tree library symlinks must not escape into the final installation.
- Check SDK restoration when a single generated public header is absent and
  CMake libcds package export/import with shared and static targets.
- Run the affected tests on supported Linux/Windows configurations as regression
  checks. Test the software hash fallback with SSE4.2 unavailable.

Coordinate allocation of the new database OS identifier and message numbers
with maintainers before merging. Packaging, rc.d service management, account
creation, SYSDBA initialization and network access policy are separate from this
source port. Do not expose a server with default credentials.

## System references

- [OpenBSD futex(2)](https://man.openbsd.org/futex.2) and
  [flock(2)](https://man.openbsd.org/flock.2): event waits and file-lock lifetime.
- [OpenBSD dlopen(3)](https://man.openbsd.org/dlopen.3): versioned library lookup
  and dependency symbol lookup.
- [OpenBSD clock_gettime(2)](https://man.openbsd.org/clock_gettime.2): monotonic clocks.
- [CMake install](https://cmake.org/cmake/help/latest/command/install.html):
  `NAMELINK_ONLY` and export sets.
