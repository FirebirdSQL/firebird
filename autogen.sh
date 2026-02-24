#!/bin/sh
#
# Run this to generate all the initial makefiles, etc.
#

PKG_NAME=Firebird6
SRCDIR=`dirname $0`
SRCDIR=`(cd "$SRCDIR" && pwd)`
OBJDIR=`pwd`

if [ "$OBJDIR" = "$SRCDIR" ]
then
  BUILD_MODE=in-tree
else
  BUILD_MODE=out-of-tree
fi

if [ -z "$AUTORECONF" ]
then
  AUTORECONF=autoreconf
fi

echo "AUTORECONF="$AUTORECONF

# This prevents calling automake in old autotools
AUTOMAKE=true
export AUTOMAKE

# This helps some old aclocal versions find local and source m4 macros
ACLOCAL="aclocal -I $OBJDIR -I $OBJDIR/m4 -I $SRCDIR"
if [ -d "$SRCDIR/m4" ]; then
	ACLOCAL="$ACLOCAL -I $SRCDIR/m4"
fi
export ACLOCAL

# Give a warning if no arguments to 'configure' have been supplied.
if test -z "$*" -a x$NOCONFIGURE = x; then
  echo "**Warning**: I am going to run \`configure' with no arguments."
  echo "If you wish to pass any to it, please specify them on the"
  echo \`$0\'" command line."
  echo
fi

# Some versions of autotools need it
if [ ! -d "$OBJDIR/m4" ]; then
	rm -rf "$OBJDIR/m4"
	mkdir -p "$OBJDIR/m4"
fi

# Ensure correct utilities are called by AUTORECONF
autopath=`dirname $AUTORECONF`
if [ "x$autopath" != "x" ]; then
	PATH=$autopath:$PATH
	export PATH
fi

if [ "$BUILD_MODE" = "out-of-tree" ]
then
	mkdir -p "$OBJDIR/builds/make.new"
	ln -sf "$SRCDIR/configure.ac" "$OBJDIR/configure.ac"
	ln -sf "$SRCDIR/acx_pthread.m4" "$OBJDIR/acx_pthread.m4"
	ln -sf "$SRCDIR/binreloc.m4" "$OBJDIR/binreloc.m4"
fi

echo "Running autoreconf in $OBJDIR ($BUILD_MODE) ..."
(cd "$OBJDIR" && $AUTORECONF --install --force --verbose) || exit 1

if [ "$BUILD_MODE" = "out-of-tree" ]
then
	# Prefer auxiliary files generated in the build directory when running configure
	sed -i 's#ac_aux_dir_candidates="${srcdir}/builds/make.new/config"#ac_aux_dir_candidates="$ac_pwd/builds/make.new/config${PATH_SEPARATOR}${srcdir}/builds/make.new/config"#' "$OBJDIR/configure"
fi

# Hack to bypass bug in autoreconf - --install switch not passed to libtoolize,
# therefore missing config.sub and confg.guess files
CONFIG_AUX_DIR="$OBJDIR/builds/make.new/config"
if [ ! -f "$CONFIG_AUX_DIR/config.sub" -o ! -f "$CONFIG_AUX_DIR/config.guess" ]; then
	# re-run libtoolize with --install switch, if it does not understand that switch
	# and there are no config.sub/guess files in CONFIG_AUX_DIR, we will anyway fail
	echo "Re-running libtoolize ..."
	if [ -z "$LIBTOOLIZE" ]; then
		LIBTOOLIZE=libtoolize
	fi
	(cd "$OBJDIR" && $LIBTOOLIZE --install --copy --force) || exit 1
fi

# If NOCONFIGURE is set, skip the call to configure
if test "x$NOCONFIGURE" = "x"; then
  if [ "$BUILD_MODE" = "out-of-tree" ]
  then
    CONFIGURE_CMD="$OBJDIR/configure --srcdir=$SRCDIR"
  else
    CONFIGURE_CMD="$OBJDIR/configure"
  fi

  echo Running $CONFIGURE_CMD $conf_flags "$@" ...
  rm -f config.cache config.log
  chmod a+x "$OBJDIR/configure"
  $CONFIGURE_CMD $conf_flags "$@" \
  && echo Now type \`make\' to compile $PKG_NAME
else
  echo Autogen skipping configure process.
fi

# EOF
