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

runAndCheckExit "Build messages file (firebird.msg)" "bin/build_file -f firebird.msg"
runAndCheckExit "Creating security database" "echo create database 'security5.fdb' | bin/isql -q"
runAndCheckExit "Creating security database metadata" "bin/isql -q security5.fdb -i security.sql"
runAndCheckExit "Restore examples database (employee)" "bin/gbak -rep examples/empbuild/employee.gbak examples/empbuild/employee.fdb"

rm -f security.sql bin/build_file examples/empbuild/employee.gbak AfterUntar.sh
