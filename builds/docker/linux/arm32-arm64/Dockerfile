ARG ARG_BASE=arm64v8/debian:bookworm
ARG ARG_TARGET_ARCH=aarch64-pc-linux-gnu
ARG ARG_CTNF_CONFIG=crosstool-ng-config-arm64

ARG ARG_CTNG_VERSION=1.26.0
ARG ARG_LIBTOOL_VERSION=2.4.6
ARG ARG_NCURSES_VERSION=6.3
ARG ARG_LTM_VERSION=1.2.0
ARG ARG_ZLIB_VERSION=1.3.1
ARG ARG_ICU_VERSION=70-1

ARG ARG_CPUCOUNT=24

ARG ARG_CTNG_UID=1000
ARG ARG_CTNG_GID=1000


FROM debian:bookworm as builder

ARG ARG_TARGET_ARCH
ARG ARG_CTNF_CONFIG

ARG ARG_CTNG_VERSION
ARG ARG_LIBTOOL_VERSION
ARG ARG_NCURSES_VERSION
ARG ARG_LTM_VERSION
ARG ARG_ZLIB_VERSION
ARG ARG_ICU_VERSION

ARG ARG_CPUCOUNT

ARG ARG_CTNG_UID
ARG ARG_CTNG_GID

RUN groupadd -g $ARG_CTNG_GID ctng && \
	useradd -d /home/ctng -m -g $ARG_CTNG_GID -u $ARG_CTNG_UID -s /bin/bash ctng && \
	\
	{ echo 'tzdata tzdata/Areas select Etc'; echo 'tzdata tzdata/Zones/Etc select UTC'; } | debconf-set-selections && \
	\
	apt-get update && \
	apt-get -y install \
		sudo \
		automake \
		bison \
		bzip2 \
		chrpath \
		flex \
		gcc \
		g++ \
		git \
		gperf \
		gawk \
		help2man \
		libexpat1-dev \
		libncurses5-dev \
		libsdl1.2-dev \
		libtool \
		libtool-bin \
		libtool-doc \
		python3-dev \
		texinfo \
		make \
		curl \
		wget \
		unzip \
		xz-utils \
		nano && \
	\
	adduser ctng sudo && \
	echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER ctng

COPY --from=asfernandes/firebird-builder:fb6-x64-ng-v1 --chown=ctng:ctng /home/ctng/x-tools /home/ctng/x-tools

RUN mkdir ~/build && \
	curl -SL --output ~/build/icu4c-${ARG_ICU_VERSION}-src.tgz https://github.com/unicode-org/icu/releases/download/release-${ARG_ICU_VERSION}/icu4c-`echo ${ARG_ICU_VERSION} | tr - _`-src.tgz && \
	curl -SL --output ~/build/ncurses-${ARG_NCURSES_VERSION}.tar.gz https://ftp.gnu.org/pub/gnu/ncurses/ncurses-${ARG_NCURSES_VERSION}.tar.gz && \
	curl -SL --output ~/build/libtool-${ARG_LIBTOOL_VERSION}.tar.gz https://ftpmirror.gnu.org/libtool/libtool-${ARG_LIBTOOL_VERSION}.tar.gz && \
	curl -SL --output ~/build/ltm-${ARG_LTM_VERSION}.tar.xz https://github.com/libtom/libtommath/releases/download/v${ARG_LTM_VERSION}/ltm-${ARG_LTM_VERSION}.tar.xz && \
	curl -SL --output ~/build/zlib-${ARG_ZLIB_VERSION}.tar.gz https://zlib.net/zlib-${ARG_ZLIB_VERSION}.tar.gz

ADD --chown=ctng:ctng $ARG_CTNF_CONFIG /tmp/crosstool-ng-config

RUN cd && \
	mkdir src && \
	git clone https://github.com/crosstool-ng/crosstool-ng.git && \
	cd crosstool-ng && \
	git checkout -b branch crosstool-ng-${ARG_CTNG_VERSION} && \
	./bootstrap && \
	./configure --enable-local && \
	make && \
	mv /tmp/crosstool-ng-config .config && \
	cd ~/crosstool-ng && \
	./ct-ng build.${ARG_CPUCOUNT}

RUN sudo apt-get -y remove \
		autoconf \
		automake \
		binutils \
		cpp \
		gcc \
		g++ \
		binutils \
		libtool \
		git \
		libtommath1 \
		libtommath-dev \
		libtomcrypt1 \
		libtomcrypt-dev && \
	sudo rm -rf /var/lib/apt/lists/*

ENV PATH "/home/ctng/x-tools/x86_64-pc-linux-gnu/bin:/home/ctng/x-tools/x86_64-pc-linux-gnu/x86_64-pc-linux-gnu/sysroot/usr/local/bin:${PATH}"
ENV PATH "/home/ctng/x-tools/${ARG_TARGET_ARCH}/bin:/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot/usr/local/bin:${PATH}"

RUN cd ~/build && \
	mkdir -p libtool-${ARG_LIBTOOL_VERSION}-src && \
	tar xvf libtool-${ARG_LIBTOOL_VERSION}.tar.gz --strip 1 -C libtool-${ARG_LIBTOOL_VERSION}-src && \
	rm libtool-${ARG_LIBTOOL_VERSION}.tar.gz && \
	mkdir libtool-${ARG_LIBTOOL_VERSION}-build && \
	cd libtool-${ARG_LIBTOOL_VERSION}-build && \
	../libtool-${ARG_LIBTOOL_VERSION}-src/configure \
		--host=${ARG_TARGET_ARCH} \
		--prefix=/home/ctng/x-tools/${ARG_TARGET_ARCH} && \
	make -j${ARG_CPUCOUNT} && \
	make install

RUN cd ~/build && \
	mkdir -p ncurses-${ARG_NCURSES_VERSION}-src && \
	tar xvf ncurses-${ARG_NCURSES_VERSION}.tar.gz --strip 1 -C ncurses-${ARG_NCURSES_VERSION}-src && \
	rm ncurses-${ARG_NCURSES_VERSION}.tar.gz && \
	mkdir ncurses-${ARG_NCURSES_VERSION}-build && \
	cd ncurses-${ARG_NCURSES_VERSION}-build && \
	../ncurses-${ARG_NCURSES_VERSION}-src/configure \
		--host=${ARG_TARGET_ARCH} \
		--with-build-cc=x86_64-pc-linux-gnu-gcc \
		--prefix=/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot/usr \
		--bindir=/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot/usr/local/bin \
		--libdir=/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot/usr/local/lib \
		--enable-overwrite \
		--disable-db-install \
		--disable-termcap \
		--without-ada \
		--without-cxx \
		--without-cxx-binding \
		--without-develop \
		--without-tests \
		--without-progs \
		--with-default-terminfo-dir=/etc/terminfo \
		--with-terminfo-dirs="/etc/terminfo:/lib/terminfo:/usr/share/terminfo" && \
	make -j${ARG_CPUCOUNT} && \
	make install

RUN cd ~/build && \
	mkdir -p ltm-${ARG_LTM_VERSION}-src && \
	tar xvf ltm-${ARG_LTM_VERSION}.tar.xz --strip 1 -C ltm-${ARG_LTM_VERSION}-src && \
	rm ltm-${ARG_LTM_VERSION}.tar.xz && \
	cd ltm-${ARG_LTM_VERSION}-src && \
	CC=${ARG_TARGET_ARCH}-gcc AR=${ARG_TARGET_ARCH}-ar make \
		-C ../ltm-${ARG_LTM_VERSION}-src \
		-f makefile.shared \
		-j${ARG_CPUCOUNT} && \
	make install \
		-f makefile.shared \
		DESTDIR=/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot \
		INCPATH=/usr/include \
		AR=${ARG_TARGET_ARCH}-ar \
		RANLIB=${ARG_TARGET_ARCH}-ranlib

RUN cd ~/build && \
	mkdir zlib-${ARG_ZLIB_VERSION}-src && \
	tar xvf zlib-${ARG_ZLIB_VERSION}.tar.gz --strip 1 -C zlib-${ARG_ZLIB_VERSION}-src && \
	rm zlib-${ARG_ZLIB_VERSION}.tar.gz && \
	mkdir zlib-${ARG_ZLIB_VERSION}-build && \
	cd zlib-${ARG_ZLIB_VERSION}-build && \
	CROSS_PREFIX=${ARG_TARGET_ARCH}- ../zlib-${ARG_ZLIB_VERSION}-src/configure \
		--prefix=/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot/usr \
		--libdir=/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot/usr/local/lib \
		--enable-shared && \
	make -j${ARG_CPUCOUNT} && \
	make install

RUN cd ~/build && \
	mkdir icu4c-${ARG_ICU_VERSION}-src && \
	tar xvf icu4c-${ARG_ICU_VERSION}-src.tgz --strip 1 -C icu4c-${ARG_ICU_VERSION}-src && \
	mkdir icu4c-${ARG_ICU_VERSION}-build-x86_64 && \
	cd icu4c-${ARG_ICU_VERSION}-build-x86_64 && \
	CXXFLAGS='-std=c++17 -static-libstdc++' ../icu4c-${ARG_ICU_VERSION}-src/source/runConfigureICU \
		Linux \
		--host=x86_64-pc-linux-gnu && \
	make -j${ARG_CPUCOUNT}

RUN cd ~/build && \
	mkdir icu4c-${ARG_ICU_VERSION}-build && \
	cd icu4c-${ARG_ICU_VERSION}-build && \
	CXXFLAGS='-std=c++17 -static-libstdc++' ../icu4c-${ARG_ICU_VERSION}-src/source/runConfigureICU \
		Linux \
		--host=${ARG_TARGET_ARCH} \
		--with-cross-build=/home/ctng/build/icu4c-${ARG_ICU_VERSION}-build-x86_64 \
		--prefix=/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot/usr/local \
		--includedir=/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot/usr/include && \
	make -j${ARG_CPUCOUNT} && \
	make install


FROM $ARG_BASE

ARG ARG_TARGET_ARCH
ARG ARG_CTNG_UID
ARG ARG_CTNG_GID

RUN apt-get update && \
	apt-get -y install \
		cmake \
		libc6-amd64-cross \
		libfile-copy-recursive-perl \
		unzip && \
	rm -rf /var/lib/apt/lists/*

RUN ln -s /usr/x86_64-linux-gnu/lib64 /lib64 && \
	groupadd -g $ARG_CTNG_GID ctng && \
	useradd -d /home/ctng -m -g $ARG_CTNG_GID -u $ARG_CTNG_UID -s /bin/bash ctng

USER ctng

RUN chmod o=u /home/ctng

COPY --from=builder --chown=ctng:ctng /home/ctng/x-tools /home/ctng/x-tools
COPY --chown=ctng:ctng scripts/* /

# Let non-emulated x86_64-linux-gnu make run.
RUN rm /home/ctng/x-tools/${ARG_TARGET_ARCH}/bin/make

ENV PATH "/home/ctng/x-tools/${ARG_TARGET_ARCH}/bin:/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot/usr/local/bin:/home/ctng/x-tools/x86_64-pc-linux-gnu/bin:${PATH}"
ENV LIBRARY_PATH "/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot/usr/local/lib"
ENV LD_LIBRARY_PATH "/usr/x86_64-linux-gnu/lib:/home/ctng/x-tools/${ARG_TARGET_ARCH}/${ARG_TARGET_ARCH}/sysroot/usr/local/lib"

ENV BUILD_ARCH=$ARG_TARGET_ARCH

WORKDIR /firebird

ENTRYPOINT ["/entry.sh"]
