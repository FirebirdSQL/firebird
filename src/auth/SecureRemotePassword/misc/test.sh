#!/bin/sh
compile()
{
	file=test_${1}
	g++ -I../../../include -DLINUX -DAMD64 -DDEV_BUILD ${file}.cpp \
		../../../../temp/Debug/auth/SecureRemotePassword/srp.o \
		../../../../temp/Debug/common.a -ltommath ../../../../gen/Debug/firebird/lib/libfbclient.so \
		 -lpthread -Wl,-rpath,../../../../gen/Debug/firebird/lib -lrt -o ${file} && ./${file}
}

compile srp
read x
