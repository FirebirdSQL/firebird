CalcException = class(Exception)
public
	constructor create(code: Integer);

	function getCode: Integer;

	class procedure checkException(status: Status);
	class procedure catchException(status: Status; e: Exception);
	class procedure setVersionError(status: Status; interfaceName: string;
		currentVersion, expectedVersion: NativeInt);

private
	code: Integer;
end;

