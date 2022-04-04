constructor CalcException.create(code: Integer);
begin
	self.code := code;
end;

function CalcException.getCode: Integer;
begin
	Result := code;
end;

class procedure CalcException.checkException(status: Status);
var
	code: Integer;
begin
	code := status.getCode();

	if (code <> 0) then
		raise CalcException.create(code);
end;

class procedure CalcException.catchException(status: Status; e: Exception);
begin
	if (e.inheritsFrom(CalcException)) then
		status.setCode(CalcException(e).code)
	else
		status.setCode(-1);
end;

class procedure CalcException.setVersionError(status: Status; interfaceName: string;
		currentVersion, expectedVersion: NativeInt);
begin
	status.setCode(Status.ERROR_1);
end;
