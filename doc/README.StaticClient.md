# Building and using a static `fbclient`

Firebird's client library, `libfbclient`/`fbclient.dll`, is normally built and distributed only as a **shared** library.
An optional, non-default static archive is now also available for applications that need to link the client library
statically.

## Why this is not the default

Firebird overrides the global C++ operators `operator new`, `operator new[]`, `operator delete` and `operator delete[]`
(see `src/common/classes/alloc.cpp`) so that any bare, non-pool `new`/`delete` expression - inside Firebird itself, or
in foreign code linked into the *shared* client library - is routed through Firebird's own memory pool. This is safe in
the shared library: the symbols are not re-exported (POSIX version script / Windows `.def` file), so an application
linking against `libfbclient.so`/`fbclient.dll` keeps its own global operators untouched.

A *static* archive has no such boundary: its object files are merged directly into the host application, and the linker
would resolve the host's own bare `new`/`delete` to Firebird's definitions - silently hijacking all of the host
application's allocations. To avoid this, the static build isolates every internal global symbol — not just the global
operators, but also decNumber symbols, C++ mangled names, internal helpers, etc. On ELF, these symbols are renamed to
`__fbclient_*` names with `objcopy --redefine-syms`. On Darwin, native `ld` localizes every non-API symbol while
producing the relocatable object stored in the archive. This frees the standard global operator names for the host
application while keeping Firebird internally self-consistent (its own pool-routing logic in
`MemoryPool::globalFree` still works, since foreign/system allocations of Firebird's own instances still flow through
the isolated operators).

Regular pool-based allocation via `FB_NEW`/`FB_NEW_POOL` (the idiomatic path used throughout the codebase) is unaffected
- it never used the global operators to begin with.

## Building on POSIX

```sh
make -C temp/debug TARGET=Debug client_static
```

`TARGET` (`Debug` or `Release`) defaults the same way the top-level `firebird` target does if omitted, but pass it
explicitly - an empty/undefined `TARGET` elsewhere in a partial build (e.g. after only some sub-targets ran) can lead to
mismatched paths.

This produces `<firebird>/lib/libfbclient.a`, built from the shared client object set (yValve + remote client + common)
with the static client's built-in plugin registrations.

When TomCrypt support is enabled, ChaCha and ChaCha64 are included and registered as built-in wire crypt plugins in
`libfbclient.a`. The standalone `libChaCha.so` plugin is not needed by applications using the static client.

Link your application against it, along with the same system libraries the shared library depends on transitively
(pthread, dl, crypt, rt, etc. as applicable to your platform). Most third-party dependencies (tommath, tomcrypt) are
**not** bundled - link those directly. In particular, applications using the static client must link `-ltomcrypt`
when TomCrypt support is enabled.

**`decNumber`/`libdecFloat` is the one exception**: it is small, vendored in-tree, and has no external system-library
equivalent to conflict with, so its objects are merged directly into `libfbclient.a`/`fbclient_static.lib` - you do not
need to separately link `-ldecFloat`/`decnumber.lib`.

### Symbol collision prevention

Firebird overrides the global C++ operators `operator new`, `operator new[]`, `operator delete` and `operator delete[]`
(see `src/common/classes/alloc.cpp`). In a shared library, these symbols are hidden by the version script and don't
conflict with the host application. In a static archive, all of Firebird's internal symbols — not just the global
operators, but also decNumber symbols, C++ mangled names, internal helpers, etc. — would collide if they remain global.

The post-processing pipeline differs by object format.

On ELF:

1. **Archive slimming** uses `ld -r` with `-u` flags for every exported symbol from `builds/posix/firebird.vers`. The
   linker pulls in only archive members transitively reachable from those seeds; GNU `--cref` identifies the members
   that were pulled in, and only those members are repacked into the archive.
2. **Dynamic symbol renaming** renames every defined global symbol not in the exported API list with a `__fbclient_`
   prefix using `objcopy --redefine-syms`. ELF COMDAT group signatures are included as well; they are local symbols
   and are not reported by `nm -g`, but must be renamed with their sections. The rename map is generated
   automatically at build time:

   ```
   nm --defined-only -g libfbclient.a  |  extract symbol names  |  filter out API list  |  awk '{print $1, "__fbclient_" $1}'
   readelf --section-groups libfbclient.a  |  extract COMDAT group signatures  |  add to symbol list (ELF)
   ```

   This covers every internal symbol automatically (decNumber, global operators, C++ vtables, typeinfo, guard
   variables, COMDAT group signatures, etc.) without any hand-maintained lists. The multi-member archive structure is
   preserved, so COMDAT groups and cross-member references work correctly at final link time.

On Darwin:

1. **Archive slimming and symbol localization** use native `ld -r` with `-u` flags and `-exported_symbols_list`. The
   linker pulls in only archive members transitively reachable from the exported API seeds, keeps those API symbols
   global, and localizes every other definition. Apple `-map` output is written to the temporary map file for
   diagnostics. The resulting relocatable object is stored as the sole archive member.
2. No `objcopy` step is needed. Internal symbol names remain unchanged, but they are local to the relocatable object
   and therefore cannot collide with symbols in the host application. This also means Darwin archives do not contain
   `__fbclient_*` prefixes. Because this relocatable object is the sole archive member, applications that need to
   discard unused client code should link with the Darwin linker's `-dead_strip` option.

### Post-archive processing

All of the post-archive processing (slimming, symbol isolation, and archive rebuild) is done by the unified helper
script `static_client.sh` (autoconf-generated from `builds/posix/static_client.sh.in`). A single code path handles
both ELF and Darwin, with `STATIC_CLIENT_SLIM_MODE` selecting the linker output format and platform-specific symbol
isolation behavior.

## Building on Windows

Build the static client via the usual Windows scripts with `CLIENT_ONLY=STATIC`:

```sh
builds\win32\make_all.bat CLIENT_ONLY=STATIC
```

TomCrypt support is enabled by default. ChaCha and ChaCha64 are then included and registered as built-in wire crypt
plugins in `fbclient_static.lib`; applications must link `tomcrypt.lib` in addition to the existing static-client
dependencies. To omit TomCrypt and ChaCha from a static client build, use:

```bat
builds\win32\run_all.bat CLIENT_ONLY=STATIC WITHOUT_TOMCRYPT
```

`WITHOUT_TOMCRYPT` is valid only with `CLIENT_ONLY=STATIC`.

That builds `yvalve` as a static library (`fbclient_static.lib`) under `temp\<platform>\ReleaseStatic` (or
`DebugStatic`), then runs `builds/win32/fix_fbclient_static.bat` to rename symbols with
`llvm-objcopy --redefine-syms`. The rename map is generated automatically — no hand-maintained lists needed:

1. Parse `builds/win32/defs/firebird.def` to extract the public API symbol names.
2. Run `llvm-nm --defined-only -g` on every extracted `.obj` to collect all defined global symbols.
3. For each symbol not in the API list, emit a rename pair mapping it to a `__fbclient_`-prefixed name (preserving
   x86 `__stdcall` `@n` decoration where present). C++ mangled (`?`-prefixed) symbols are always renamed.
4. Apply the single rename map across all objects with `llvm-objcopy --redefine-syms`.

This covers every internal symbol automatically (global operators, decNumber, C++ vtables, typeinfo, guard variables,
etc.) without any hand-maintained symbol lists or separate scripts. The script requires `llvm-objcopy` and `llvm-nm`
to be available on `PATH`. `decnumber.lib` is merged into the static archive via `<Lib><AdditionalDependencies>`
(MSVC's librarian merges a referenced `.lib`'s members into a new static-library archive when specified this way).

## Verifying symbol isolation

On ELF, the public API should remain global under original names:

```sh
nm libfbclient.a | grep ' T isc_'
```

should show all `isc_*` symbols as global (`T`).

Internal symbols should be renamed with the `__fbclient_` prefix:

```sh
nm libfbclient.a | grep ' T __fbclient_' | wc -l
```

should show a large number of renamed symbols (thousands).

ELF global operators should no longer exist under their original names:

```sh
nm libfbclient.a | grep -E ' _Znwm| _Znam| _ZdlPv| _ZdaPv' | grep -c ' T '
```

should show `0` — all have been renamed to `__fbclient_*` variants.

decNumber symbols should also be renamed:

```sh
nm libfbclient.a | grep ' T decNumberFromString'
```

should show nothing — it's now `__fbclient_decNumberFromString`.

On Darwin, the public API should remain externally visible:

```sh
nm -g libfbclient.a | grep ' _isc_'
```

Internal symbols are local rather than prefixed. They should not appear in the external-symbol listing:

```sh
nm -g libfbclient.a | grep -E '(_Znwm|_Znam|_ZdlPv|_ZdaPv|decNumberFromString)'
```

should show nothing. The same names may appear without `-g`, with local symbol types.

## Windows-specific behavior: no `DllMain`

On Windows, the shared `fbclient.dll` has a `DllMain` (`src/jrd/os/win32/ibinitdll.cpp`) that reacts to OS loader
notifications and sets a few globals in `src/common/dllinst.h`/`.cpp` (`Firebird::hDllInst`, `bDllProcessExiting`,
`dDllUnloadTID`). A statically linked `fbclient` has no separate module, so no `DllMain` ever runs for it.

This is safe: everywhere these globals are read, the code either already falls back correctly when they're at their
default `0`/`false` (e.g. the config-root lookup falls back to the host executable's own path; the loader-lock-hazard
checks simply never trigger, which is correct since a statically linked EXE never faces the DLL-unload-under-loader-lock
hazard they guard against), or is explicitly guarded to skip DLL-only work (`MasterImplementation.cpp`'s timer-thread
refcount bump on `fbclient.dll`).

The one real functional gap this closes: per-thread cleanup used to run *only* from `DllMain`'s `DLL_THREAD_DETACH`
case, which never fires without a real DLL. This is now handled uniformly for both the DLL and the static build using
Fiber-Local Storage (`FlsAlloc`/`FlsSetValue`/`FlsFree`) in `ThreadCleanup` (`src/yvalve/utl.cpp`) - the FLS callback
fires on thread exit for any thread that touched the slot, regardless of whether the code is in a DLL or linked directly
into the host application, mirroring what `pthread_key_create`'s destructor already gives the POSIX build for free.
