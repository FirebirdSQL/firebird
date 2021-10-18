echo "This script will build arch-specific files, useful for firebird functionality"
echo "Press ENTER to continue"
read x

runAndCheckExit() {
	Msg=$1
    Cmd=$2

	echo $Msg please wait...
    $Cmd
    ExitCode=$?

    if [ $ExitCode -ne 0 ]
    then
        echo "Aborted: The command $Cmd "
        echo "         failed with error code $ExitCode"
        exit $ExitCode
    fi
}

runAndCheckExit "Build messages file (firebird.msg)" bin/build_file
runAndCheckExit "Restore security database" "bin/gbak -rep security5.gbak security5.fdb"
runAndCheckExit "Restore examples database (employee)" "bin/gbak -rep examples/empbuild/employee.gbak examples/empbuild/employee.fdb"

rm -f security5.gbak examples/empbuild/employee.gbak AfterUntar.sh
