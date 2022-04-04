ARG CPUCOUNT=6

ARG GCC_VERSION=10.3.0
ARG LIBTOOL_VERSION=2.4.6
ARG MAKE_VERSION=4.3
ARG BINUTILS_VERSION=2.37
ARG NCURSES_VERSION=6.3
ARG ICU_VERSION=70_1

ARG BASE=ubuntu:raring
ARG BUILD_ARCH=x86_64-pc-linux-gnu
ARG LIBDIR=/usr/lib/x86_64-linux-gnu

FROM ubuntu:focal as downloader

ARG GCC_VERSION
ARG LIBTOOL_VERSION
ARG MAKE_VERSION
ARG BINUTILS_VERSION
ARG NCURSES_VERSION
ARG ICU_VERSION

RUN \
	apt-get -y update && \
	apt-get -y install curl xz-utils && \
	rm -rf /var/lib/apt/lists/* && \
	\
	mkdir /work && \
	cd /work && \
	\
	echo Downloading tools and libraries && \
	curl -SL --output gcc-${GCC_VERSION}.tar.xz https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz && \
	curl -SL --output libtool-${LIBTOOL_VERSION}.tar.gz https://ftpmirror.gnu.org/libtool/libtool-${LIBTOOL_VERSION}.tar.gz && \
	curl -SL --output make-${MAKE_VERSION}.tar.bz2 https://ftp.gnu.org/gnu/make/make-${MAKE_VERSION}.tar.gz && \
	curl -SL --output binutils-${BINUTILS_VERSION}.tar.bz2 https://mirror.nbtelecom.com.br/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.bz2 && \
	curl -SL --output ncurses-${NCURSES_VERSION}.tar.gz https://ftp.gnu.org/pub/gnu/ncurses/ncurses-${NCURSES_VERSION}.tar.gz && \
	curl -SL --output icu4c-${ICU_VERSION}-src.tgz https://github.com/unicode-org/icu/releases/download/release-`echo ${ICU_VERSION} | tr _ -`/icu4c-${ICU_VERSION}-src.tgz

FROM $BASE

ARG CPUCOUNT
ARG GCC_VERSION
ARG LIBTOOL_VERSION
ARG MAKE_VERSION
ARG BINUTILS_VERSION
ARG NCURSES_VERSION
ARG ICU_VERSION

ARG BUILD_ARCH
ARG LIBDIR

COPY --from=downloader /work /work

RUN \
	sed -i 's/archive/old-releases/g' /etc/apt/sources.list && \
	apt-get -y update && \
	apt-get -y install curl xz-utils lbzip2 bzip2 m4 gcc g++ make libtool autoconf libtommath-dev zlib1g-dev unzip && \
	rm -rf /var/lib/apt/lists/* && \
	\
	cd /work && \
	mkdir -p /work/gcc-${GCC_VERSION}-src && \
	tar -xvf gcc-${GCC_VERSION}.tar.xz --strip 1 -C gcc-${GCC_VERSION}-src && \
	rm gcc-${GCC_VERSION}.tar.xz && \
	cd gcc-${GCC_VERSION}-src && \
	./contrib/download_prerequisites && \
	\
	cd /work && \
	mkdir -p /work/libtool-${LIBTOOL_VERSION}-src && \
	tar -xzvf libtool-${LIBTOOL_VERSION}.tar.gz --strip 1 -C libtool-${LIBTOOL_VERSION}-src && \
	rm libtool-${LIBTOOL_VERSION}.tar.gz && \
	\
	cd /work && \
	mkdir -p /work/make-${MAKE_VERSION}-src && \
	tar -xzvf make-${MAKE_VERSION}.tar.bz2 --strip 1 -C make-${MAKE_VERSION}-src && \
	rm make-${MAKE_VERSION}.tar.bz2 && \
	\
	cd /work && \
	mkdir -p /work/binutils-${BINUTILS_VERSION}-src && \
	tar -xjvf binutils-${BINUTILS_VERSION}.tar.bz2 --strip 1 -C binutils-${BINUTILS_VERSION}-src && \
	rm binutils-${BINUTILS_VERSION}.tar.bz2 && \
	\
	cd /work && \
	mkdir -p /work/ncurses-${NCURSES_VERSION}-src && \
	tar -xzvf ncurses-${NCURSES_VERSION}.tar.gz --strip 1 -C ncurses-${NCURSES_VERSION}-src && \
	rm ncurses-${NCURSES_VERSION}.tar.gz && \
	\
	cd /work && \
	mkdir /work/icu-${ICU_VERSION}-src && \
	tar xzvf icu4c-${ICU_VERSION}-src.tgz --strip 1 -C icu-${ICU_VERSION}-src && \
	rm icu4c-${ICU_VERSION}-src.tgz && \
	\
	mkdir /work/gcc-${GCC_VERSION}-build && \
	cd /work/gcc-${GCC_VERSION}-build && \
	/work/gcc-${GCC_VERSION}-src/configure --build=${BUILD_ARCH} --prefix=/opt/gcc-${GCC_VERSION} --enable-languages=c,c++ --enable-bootstrap --enable-threads=posix --disable-multilib && \
	make -j${CPUCOUNT} && \
	make install && \
	\
	export LD_LIBRARY_PATH=/opt/gcc-${GCC_VERSION}/lib:/opt/gcc-${GCC_VERSION}/lib64 && \
	export LD_RUN_PATH=/opt/gcc-${GCC_VERSION}/lib:/opt/gcc-${GCC_VERSION}/lib64 && \
	export PATH=/opt/gcc-${GCC_VERSION}/bin:$PATH && \
	\
	mkdir /work/libtool-${LIBTOOL_VERSION}-build && \
	cd /work/libtool-${LIBTOOL_VERSION}-build && \
	/work/libtool-${LIBTOOL_VERSION}-src/configure --build=${BUILD_ARCH} --prefix=/opt/libtool-${LIBTOOL_VERSION} && \
	make -j${CPUCOUNT} && \
	make install && \
	\
	export PATH=/opt/libtool-${LIBTOOL_VERSION}/bin:$PATH && \
	\
	mkdir /work/make-${MAKE_VERSION}-build && \
	cd /work/make-${MAKE_VERSION}-build && \
	/work/make-${MAKE_VERSION}-src/configure --build=${BUILD_ARCH} --prefix=/opt/make-${MAKE_VERSION} && \
	make -j${CPUCOUNT} && \
	make install && \
	\
	export PATH=/opt/make-${MAKE_VERSION}/bin:$PATH && \
	\
	mkdir /work/binutils-${BINUTILS_VERSION}-build && \
	cd /work/binutils-${BINUTILS_VERSION}-build && \
	/work/binutils-${BINUTILS_VERSION}-src/configure --build=${BUILD_ARCH} --prefix=/opt/binutils-${BINUTILS_VERSION} && \
	make -j${CPUCOUNT} && \
	make install && \
	\
	export PATH=/opt/binutils-${BINUTILS_VERSION}/bin:$PATH && \
	\
	mkdir /work/ncurses-${NCURSES_VERSION}-build && \
	cd /work/ncurses-${NCURSES_VERSION}-build && \
	/work/ncurses-${NCURSES_VERSION}-src/configure \
		--build=${BUILD_ARCH} \
		--prefix=/usr \
		--libdir=${LIBDIR} \
		--disable-db-install \
		--disable-termcap \
		--without-ada \
		--without-cxx \
		--without-cxx-binding \
		--without-develop \
		--without-tests \
		--without-progs \
		--with-default-terminfo-dir=/etc/terminfo \
		--with-terminfo-dirs="/etc/terminfo:/lib/terminfo:/usr/share/terminfo" \
	&& \
	make -j${CPUCOUNT} && \
	make install && \
	\
	mkdir /work/icu-${ICU_VERSION}-build && \
	cd /work/icu-${ICU_VERSION}-build && \
	/work/icu-${ICU_VERSION}-src/source/runConfigureICU Linux --prefix=/opt/icu && \
	make -j${CPUCOUNT} && \
	make install && \
	\
	rm -rf /work

COPY scripts/* /

ENV LD_LIBRARY_PATH=/opt/gcc-${GCC_VERSION}/lib:/opt/gcc-${GCC_VERSION}/lib64
ENV LD_RUN_PATH=/opt/gcc-${GCC_VERSION}/lib:/opt/gcc-${GCC_VERSION}/lib64
ENV PATH=/opt/gcc-${GCC_VERSION}/bin:$PATH

ENV PATH=/opt/libtool-${LIBTOOL_VERSION}/bin:$PATH

ENV PATH=/opt/make-${MAKE_VERSION}/bin:$PATH

ENV PATH=/opt/binutils-${BINUTILS_VERSION}/bin:$PATH

ENV LIBRARY_PATH=/opt/icu/lib
ENV LD_LIBRARY_PATH=/opt/icu/lib:$LD_LIBRARY_PATH
ENV C_INCLUDE_PATH=/opt/icu/include
ENV CPLUS_INCLUDE_PATH=$C_INCLUDE_PATH

ENV BUILD_ARCH=$BUILD_ARCH

ENV CPUCOUNT=$CPUCOUNT

WORKDIR /firebird

ENTRYPOINT ["/entry.sh"]
