#!/bin/sh

v50Filter="--after=05.05.2021"
v40Filter="--after=28.02.2016"
v30Offset=15471
v25Offset=13822

processBranch() {

Branch="$1"
Adjust="$2"
Filter="$3"

git checkout $Branch
git reset --hard origin/$Branch
git clean -d -x -f

TmpFile=temp.buildno
WriteBuildNumFile="src/misc/writeBuildNum.sh"
BuildNoFile="src/jrd/build_no.h"

OrgBuildNo=$(grep "FB_BUILD_NO" $BuildNoFile | cut -d'"' -f2)

Count=$(git rev-list $Filter --count $Branch)
Skip1=$(git rev-list $Filter --grep="increment build number" --count $Branch)
Skip2=$(git rev-list $Filter --grep="nightly update" --count $Branch)
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

git fetch --all

processBranch master 0 $v50Filter
processBranch v4.0-release 0 $v40Filter
processBranch B3_0_Release $v30Offset
processBranch B2_5_Release $v25Offset

export GIT_COMMITTER_NAME="firebirds"
export GIT_COMMITTER_EMAIL="<>"
export GIT_AUTHOR_NAME="firebirds"
export GIT_AUTHOR_EMAIL="<>"

git push --all

