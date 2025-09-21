FROM debian:bookworm AS builder

ENV NDK=/opt/android-ndk
ENV NDK_TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
ENV PATH=$NDK_TOOLCHAIN/bin:$PATH

ADD https://dl.google.com/android/repository/android-ndk-r28c-linux.zip /tmp/android-ndk.zip
ADD https://github.com/unicode-org/icu/releases/download/release-63-1/icu4c-63_1-src.tgz /tmp/icu4c-63_1-src.tgz

RUN apt-get update && \
	apt-get install -y \
		cmake \
		g++ \
		unzip \
		xz-utils && \
	rm -rf /var/cache/apt/archives /var/lib/apt/lists/* && \
	\
	unzip /tmp/android-ndk.zip -d $NDK && for d in "$NDK"/*; do mv "$d"/* "$NDK"/; rmdir "$d"; done && \
	mkdir /src && \
	tar -xzf /tmp/icu4c-63_1-src.tgz -C /src --one-top-level=linux && \
	rm /tmp/android-ndk.zip /tmp/icu4c-63_1-src.tgz && \
	\
	cd /src/linux/icu/source && \
	./runConfigureICU Linux --prefix=$PWD/prebuilt \
		CFLAGS="-Os" \
		CXXFLAGS="--std=c++17" \
		--enable-static \
		--enable-shared=no \
		--enable-extras=no \
		--enable-strict=no \
		--enable-icuio=no \
		--enable-layout=no \
		--enable-layoutex=no \
		--enable-tools \
		--enable-tests=no \
		--enable-samples=no \
		--enable-dyload \
		--with-data-packaging=archive && \
	make -j15 && \
	make install && \
	\
	cp -a /src/linux /src/x86 && \
	cd /src/x86/icu/source && \
	./configure --prefix=/build/x86 \
		--host=i686-android-linux \
		--enable-static=no \
		--enable-shared \
		--enable-extras=no \
		--enable-strict=no \
		--enable-icuio=no \
		--enable-layout=no \
		--enable-layoutex=no \
		--enable-tools=no \
		--enable-tests=no \
		--enable-samples=no \
		--enable-renaming \
		--enable-dyload \
		--with-cross-build=/src/linux/icu/source \
		CFLAGS='-Os' \
		CXXFLAGS='--std=c++17' \
		LDFLAGS='-static-libstdc++ -Wl,-z,max-page-size=16384 -Wl,-rpath=\$$ORIGIN' \
		CC=i686-linux-android29-clang \
		CXX=i686-linux-android29-clang++ \
		AR=llvm-ar \
		RANLIB=llvm-ranlib \
		--with-data-packaging=archive && \
	make -j15 && \
	make install && \
	\
	cp -a /src/linux /src/x64 && \
	cd /src/x64/icu/source && \
	./configure --prefix=/build/x64 \
		--host=x86_64-android-linux \
		--enable-static=no \
		--enable-shared \
		--enable-extras=no \
		--enable-strict=no \
		--enable-icuio=no \
		--enable-layout=no \
		--enable-layoutex=no \
		--enable-tools=no \
		--enable-tests=no \
		--enable-samples=no \
		--enable-renaming \
		--enable-dyload \
		--with-cross-build=/src/linux/icu/source \
		CFLAGS='-Os' \
		CXXFLAGS='--std=c++17' \
		LDFLAGS='-static-libstdc++ -Wl,-z,max-page-size=16384 -Wl,-rpath=\$$ORIGIN' \
		CC=x86_64-linux-android29-clang \
		CXX=x86_64-linux-android29-clang++ \
		AR=llvm-ar \
		RANLIB=llvm-ranlib \
		--with-data-packaging=archive && \
	make -j15 && \
	make install && \
	\
	cp -a /src/linux /src/arm32 && \
	cd /src/arm32/icu/source && \
	./configure --prefix=/build/arm32 \
		--host=arm-linux-androideabi \
		--enable-static=no \
		--enable-shared \
		--enable-extras=no \
		--enable-strict=no \
		--enable-icuio=no \
		--enable-layout=no \
		--enable-layoutex=no \
		--enable-tools=no \
		--enable-tests=no \
		--enable-samples=no \
		--enable-renaming \
		--enable-dyload \
		--with-cross-build=/src/linux/icu/source \
		CFLAGS='-Os -march=armv7-a -mfloat-abi=softfp -mfpu=neon' \
		CXXFLAGS='--std=c++17 -march=armv7-a -mfloat-abi=softfp -mfpu=neon' \
		LDFLAGS='-static-libstdc++ -Wl,-z,max-page-size=16384 -march=armv7-a -Wl,--fix-cortex-a8 -Wl,-rpath=\$$ORIGIN' \
		CC=armv7a-linux-androideabi29-clang \
		CXX=armv7a-linux-androideabi29-clang++ \
		AR=llvm-ar \
		RANLIB=llvm-ranlib \
		--with-data-packaging=archive && \
	make -j15 && \
	make install && \
	\
	cp -a /src/linux /src/arm64 && \
	cd /src/arm64/icu/source && \
	./configure --prefix=/build/arm64 \
		--host=aarch64-android-linux \
		--enable-static=no \
		--enable-shared \
		--enable-extras=no \
		--enable-strict=no \
		--enable-icuio=no \
		--enable-layout=no \
		--enable-layoutex=no \
		--enable-tools=no \
		--enable-tests=no \
		--enable-samples=no \
		--enable-renaming \
		--enable-dyload \
		--with-cross-build=/src/linux/icu/source \
		CFLAGS='-Os' \
		CXXFLAGS='--std=c++17' \
		LDFLAGS='-static-libstdc++ -Wl,-z,max-page-size=16384 -Wl,-rpath=\$$ORIGIN' \
		CC=aarch64-linux-android29-clang \
		CXX=aarch64-linux-android29-clang++ \
		AR=llvm-ar \
		RANLIB=llvm-ranlib \
		--with-data-packaging=archive && \
	make -j15 && \
	make install && \
	\
	mkdir -p /out/x86 /out/x64 /out/arm32 /out/arm64 && \
	cd /out && \
	\
	cp /build/x86/lib/libicudata.so.63.1 x86/libicudata.63.1.so && \
	cp /build/x86/lib/libicui18n.so.63.1 x86/libicui18n.63.1.so && \
	cp /build/x86/lib/libicuuc.so.63.1 x86/libicuuc.63.1.so && \
	\
	cp /build/x64/lib/libicudata.so.63.1 x64/libicudata.63.1.so && \
	cp /build/x64/lib/libicui18n.so.63.1 x64/libicui18n.63.1.so && \
	cp /build/x64/lib/libicuuc.so.63.1 x64/libicuuc.63.1.so && \
	\
	cp /build/arm32/lib/libicudata.so.63.1 arm32/libicudata.63.1.so && \
	cp /build/arm32/lib/libicui18n.so.63.1 arm32/libicui18n.63.1.so && \
	cp /build/arm32/lib/libicuuc.so.63.1 arm32/libicuuc.63.1.so && \
	\
	cp /build/arm64/lib/libicudata.so.63.1 arm64/libicudata.63.1.so && \
	cp /build/arm64/lib/libicui18n.so.63.1 arm64/libicui18n.63.1.so && \
	cp /build/arm64/lib/libicuuc.so.63.1 arm64/libicuuc.63.1.so && \
	\
	tar cfJ /out/icu-android.tar.xz x86 x64 arm32 arm64 && \
	\
	rm -rf x86 x64 arm32 arm64 /src /build


FROM scratch

COPY --from=builder /out/icu-android.tar.xz /out/icu-android.tar.xz
