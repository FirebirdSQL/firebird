# Building Firebird under CMake+Clang 

## Windows:

install CMake and Clang (inside of MS Visual Studio or separately)

replace to Your paths:
```
   SET SOURCE="path/to/firebirdSourceFolder" 
    SET CMAKE="c:/TOOLSET/CMake/bin/cmake"
 SET LLVM_BIN="c:/TOOLSET/LLVM/bin"
       SET CC="%LLVM_BIN%/clang.exe"  
      SET CXX="%LLVM_BIN%/clang++.exe"
      SET  RC="%LLVM_BIN%/llvm-rc.exe"
SET CPU_COUNT=8
```

build:
```
echo "build filder is %SOURCE%/build"
rmdir /S /Q  %SOURCE%\build 
%CMAKE% -DCMAKE_RC_COMPILER=%RC% -DCMAKE_C_COMPILER=%CC% -DCMAKE_CXX_COMPILER=%CXX% -G "Unix Makefiles" -S %SOURCE% -B %SOURCE%/build 
%CMAKE% --build %SOURCE%/build --config Debug --target all -j %CPU_COUNT% --
```

## Linux:

install CMake and Clang (from the standard repository or separately)

replace to Your paths:
```
   SOURCE="path/to/firebirdSourceFolder"
    CMAKE="/usr/bin/cmake"
       CC="/usr/bin/clang"  
      CXX="/usr/bin/clang++"
CPU_COUNT=8
```

build:
```
echo "build filder is ${SOURCE}/build"
rm -R ${SOURCE}/build 
${CMAKE} -DCMAKE_C_COMPILER=${CC} -DCMAKE_CXX_COMPILER=${CXX} -G "Unix Makefiles" -S ${SOURCE} -B ${SOURCE}/build 
${CMAKE} --build ${SOURCE}/build --config Debug --target all -j ${CPU_COUNT} --
```

##  MacOS:

install CMake (from brew)
```
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   brew install cmake
```

replace to Your paths:
```
   SOURCE="path/to/firebirdSourceFolder"
    CMAKE="cmake"
       CC="clang"  
      CXX="clang++"
CPU_COUNT=8
```

build:
```
echo "build filder is ${SOURCE}/build"
rm -R ${SOURCE}/build 
${CMAKE} -DCMAKE_C_COMPILER=${CC} -DCMAKE_CXX_COMPILER=${CXX} -G "Unix Makefiles" -S ${SOURCE} -B ${SOURCE}/build 
${CMAKE} --build ${SOURCE}/build --config Debug --target all -j ${CPU_COUNT} --
```

