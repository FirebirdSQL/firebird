# Building Firebird on MacOSX

## Preparing

MacOS build uses `vcpkg` installed as git submodule:

```bash
git submodule update --init
```

Install XCode and dependencies with `homebrew`:

```bash
brew install automake autoconf-archive cmake libtool ninja
```

## Configuring

Set necessary environment variables:

```bash
export LIBTOOLIZE=glibtoolize
export LIBTOOL=glibtool
```

In order to get Release build:

```bash
./autogen.sh --with-builtin-tommath --with-builtin-tomcrypt
```

In order to get Debug build:

```bash
./autogen.sh --with-builtin-tommath --with-builtin-tomcrypt --enable-developer
```

## Building

```bash
make -j4
```

## Testing

```bash
make tests -j4
make run_tests
```

## Packaging

```bash
make -C gen -B -f make.platform.postfix
make -C gen -B -f Makefile.install
```
