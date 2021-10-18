# Building Firebird on MacOSX using brew

## Preparing

- Install XCode


```bash
brew install automake libtool icu4c
export CFLAGS=-I/usr/local/opt/icu4c/include
export LDFLAGS=-L/usr/local/opt/icu4c/lib
export CXXFLAGS=-I/usr/local/opt/icu4c/include
export LD_LIBRARY_PATH="/usr/local/opt/icu4c/lib:$LD_LIBRARY_PATH"
export LIBTOOLIZE=glibtoolize
export LIBTOOL=glibtool
```

## Configuring

```bash
./autogen.sh --with-builtin-tommath --with-builtin-tomcrypt
```

## Building

In order to get Release build

```bash
make -j4
```

In order to get Debug build

```bash
make -j4 Debug
```