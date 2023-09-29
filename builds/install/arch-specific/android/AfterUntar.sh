#!/bin/sh

echo "This script builds arch-specific files, useful for firebird functionality"

runAndCheckExit() {
	Msg=$1
    Cmd=$2

	echo $Msg please wait...
    eval $Cmd
    ExitCode=$?

    if [ $ExitCode -ne 0 ]
    then
        echo "Aborted: The command $Cmd "
        echo "         failed with error code $ExitCode"
        exit $ExitCode
    fi
}

runAndCheckExit "Build messages file (firebird.msg)" "./build_file -f firebird.msg"
runAndCheckExit "Creating security database" "echo create database \'security6.fdb\'^ | ./isql -q -term ^"
runAndCheckExit "Creating security database metadata" "./isql -q security6.fdb -i security.sql"
#runAndCheckExit "Restore examples database (employee)" "(cd examples/empbuild ; ../.././isql -q -i ../../employe2.sql)"

rm -f security.sql employe2.sql ./build_file AfterUntar.sh
