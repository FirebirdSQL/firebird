#!/bin/sh

v60Filter="4f47587a72ac8bece190b529cd4c50417f772226"		# Sep 16 2023, increment build number
v50Filter="054431fce7650c0d58df3f9a4100ea98d215486e"		# May 5 2021, Implemented Batch::cancel() over the wire
v40Filter="bb46a0e8310de196104bcca78fda2e0c1ea77bb5"		# Feb 28 2016, Set up the new version
v30Offset=15471
v25Offset=13822

processBranch() {

Branch="$1"
Adjust="$2"
Filter="$3"

if [ "$Filter" ]
then
	Range="$Filter..$Branch" # this notation excludes commit $Filter, so it should not belong to our range
else
	Range="$Branch"
fi

git checkout $Branch
git reset --hard origin/$Branch
git clean -d -x -f

TmpFile=temp.buildno
WriteBuildNumFile="src/misc/writeBuildNum.sh"
BuildNoFile="src/jrd/build_no.h"

OrgBuildNo=$(grep "FB_BUILD_NO" $BuildNoFile | cut -d'"' -f2)

Count=$(git rev-list --count $Range)
Skip1=$(git rev-list --grep="increment build number" --count $Range)
Skip2=$(git rev-list --grep="nightly update" --count $Range)

git rev-list $Range >~/Count.$Branch
git rev-list --grep="increment build number" $Range >~/Skip1.$Branch
git rev-list --grep="nightly update" $Range >~/Skip2.$Branch

NewBuildNo=$(($Count-$Skip1-$Skip2+$Adjust))

if [ "$NewBuildNo" != "$OrgBuildNo" ]; then
  Starting="BuildNum="
  NewLine="BuildNum=$NewBuildNo"
  AwkProgram="(/^$Starting.*/ || \$1 == \"$Starting\") {\$0=\"$NewLine\"} {print \$0}"
  awk "$AwkProgram" <$WriteBuildNumFile >$TmpFile && mv $TmpFile $WriteBuildNumFile
  chmod +x $WriteBuildNumFile
  $WriteBuildNumFile rebuildHeader $BuildNoFile $TmpFile
  git commit -m "increment build number" $WriteBuildNumFile $BuildNoFile
  rm -f $TmpFile
fi

}

errFile=~/gitFsckErr.buildNo
git fsck --strict >$errFile 2>&1 || exit
rm -f $errFile

git fetch --all

processBranch master 0 $v60Filter
processBranch v5.0-release 0 $v50Filter
processBranch v4.0-release 0 $v40Filter
processBranch B3_0_Release $v30Offset
processBranch B2_5_Release $v25Offset

export GIT_COMMITTER_NAME="firebirds"
export GIT_COMMITTER_EMAIL="<>"
export GIT_AUTHOR_NAME="firebirds"
export GIT_AUTHOR_EMAIL="<>"

git push --all

