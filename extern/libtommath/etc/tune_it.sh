#!/bin/sh

die() {
  echo "$1 failed"
  echo "Exiting"
  exit $2
}
# A linear congruential generator is sufficient for the purpose.
SEED=3735928559
LCG() {
  SEED=$(((1103515245 * $SEED + 12345) % 2147483648))
  echo $SEED
}
median() {
# read everything besides the header from file $1
#   | cut-out the required column $2
#     | sort all the entries numerically
#       | show only the first $3 entries
#         | show only the last entry
  tail -n +2 $1 | cut -d' ' -f$2 | sort -n | head -n $3 | tail -n 1
}

MPWD=$(dirname $(readlink -f "$0"))
FILE_NAME="tuning_list"
TOMMATH_CUTOFFS_H="$MPWD/../tommath_cutoffs.h"
BACKUP_SUFFIX=".orig"
RNUM=0

#############################################################################
# It would be a good idea to isolate these processes (with e.g.: cpuset)    #
#                                                                           #
# It is not a good idea to e.g: watch high resolution videos while this     #
# test are running if you do not have enough memory to avoid page faults.   #
#############################################################################

# Number of rounds overall.
LIMIT=100
# Number of loops for each input.
RLOOPS=10
# Offset ( > 0 ) . Runs tests with asymmetric input of the form 1:OFFSET
# Please use another destination for TOMMATH_CUTOFFS_H if you change OFFSET, because the numbers
# with an offset different from 1 (one) are not usable as the general cut-off values
# in "tommath_cutoffs.h".
OFFSET=1
# Number ( >= 3 ) of positive results (TC-is-faster) accumulated until it is accepted.
# Due to the algorithm used to compute the median in this Posix compliant shell script
# the value needs to be 3 (three), not less, to keep the variation small.
LAG=3
# Keep the temporary file $FILE_NAME. Set to 0 (zero) to remove it at the end.
# The file is in a format fit to feed into R directly. If you do it and find the median
# of this program to be off by more than a couple: please contact the authors and report
# the numbers from this program and R and the standard deviation. This program is known
# to get larger errors if the standard deviation is larger than ~50.
KEEP_TEMP=1

echo "You might like to watch the numbers go up to $LIMIT but it will take a long time!"

# Might not have sufficient rights or disc full.
echo "km ks tc3m tc3s" > $FILE_NAME || die "Writing header to $FILE_NAME" $?
i=1
while [ $i -le $LIMIT ]; do
   RNUM=$(LCG)
   printf "\r%d" $i
   "$MPWD"/tune -t -r $RLOOPS -L $LAG -S "$RNUM" -o $OFFSET >> $FILE_NAME || die "tune" $?
   i=$((i + 1))
done

if [ $KEEP_TEMP -eq 0 ]; then
   rm -v $FILE_NAME || die "Removing $KEEP_TEMP" $?
fi

echo "Writing cut-off values to \"$TOMMATH_CUTOFFS_H\"."
echo "In case of failure: a copy of \"$TOMMATH_CUTOFFS_H\" is in \"$TOMMATH_CUTOFFS_H$BACKUP_SUFFIX\""

cp -v $TOMMATH_CUTOFFS_H $TOMMATH_CUTOFFS_H$BACKUP_SUFFIX || die "Making backup copy of $TOMMATH_CUTOFFS_H" $?

cat << END_OF_INPUT > $TOMMATH_CUTOFFS_H || die "Writing header to $TOMMATH_CUTOFFS_H" $?
/* LibTomMath, multiple-precision integer library -- Tom St Denis */
/* SPDX-License-Identifier: Unlicense */
/*
   Current values evaluated on an AMD A8-6600K (64-bit).
   Type "make tune" to optimize them for your machine but
   be aware that it may take a long time. It took 2:30 minutes
   on the aforementioned machine for example.
 */
END_OF_INPUT

# The Posix shell does not offer an array data type so we create
# the median with 'standard tools'^TM

# read the file (without the first line) and count the lines
i=$(tail -n +2 $FILE_NAME | wc -l)
# our median point will be at $i entries
i=$(( (i / 2) + 1 ))
TMP=$(median $FILE_NAME 1 $i)
echo "#define MP_DEFAULT_KARATSUBA_MUL_CUTOFF $TMP"
echo "#define MP_DEFAULT_KARATSUBA_MUL_CUTOFF $TMP" >> $TOMMATH_CUTOFFS_H || die "(km) Appending to $TOMMATH_CUTOFFS_H" $?
TMP=$(median $FILE_NAME 2 $i)
echo "#define MP_DEFAULT_KARATSUBA_SQR_CUTOFF $TMP"
echo "#define MP_DEFAULT_KARATSUBA_SQR_CUTOFF $TMP" >> $TOMMATH_CUTOFFS_H || die "(ks) Appending to $TOMMATH_CUTOFFS_H" $?
TMP=$(median $FILE_NAME 3 $i)
echo "#define MP_DEFAULT_TOOM_MUL_CUTOFF      $TMP"
echo "#define MP_DEFAULT_TOOM_MUL_CUTOFF      $TMP" >> $TOMMATH_CUTOFFS_H || die "(tc3m) Appending to $TOMMATH_CUTOFFS_H" $?
TMP=$(median $FILE_NAME 4 $i)
echo "#define MP_DEFAULT_TOOM_SQR_CUTOFF      $TMP"
echo "#define MP_DEFAULT_TOOM_SQR_CUTOFF      $TMP" >> $TOMMATH_CUTOFFS_H || die "(tc3s) Appending to $TOMMATH_CUTOFFS_H" $?

