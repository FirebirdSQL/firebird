#!/bin/sh
set -e

: "${CTNG_UID:=1000}"
: "${CTNG_GID:=1000}"

current_uid="$(id -u ctng)"
current_gid="$(id -g ctng)"

if [ "$CTNG_GID" != "$current_gid" ]; then
	# If the requested GID exists, use it as the primary group.
	if getent group "$CTNG_GID" >/dev/null 2>&1; then
		usermod -g "$CTNG_GID" ctng
	else
		groupmod -g "$CTNG_GID" ctng
	fi
fi

if [ "$CTNG_UID" != "$current_uid" ]; then
	# If the requested UID is already taken by another user, keep the existing UID.
	if getent passwd "$CTNG_UID" >/dev/null 2>&1 && [ "$(getent passwd "$CTNG_UID" | cut -d: -f1)" != "ctng" ]; then
		echo "warning: CTNG_UID=$CTNG_UID already exists, keeping ctng uid=$current_uid" >&2
	else
		usermod -u "$CTNG_UID" ctng
	fi
fi

chown ctng:ctng /home/ctng /home/ctng/firebird-build/gen

export HOME=/home/ctng

exec gosu ctng "$@"
