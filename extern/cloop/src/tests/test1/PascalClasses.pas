{
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes.
 *
 *  Copyright (c) 2014 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
}

unit PascalClasses;

interface

uses CalcPascalApi;

type
	MyStatusImpl = class(StatusImpl)
		constructor create;

		procedure dispose(); override;
		function getCode(): Integer; override;
		procedure setCode(n: Integer); override;

	private
		code: Integer;
	end;

	MyCalculatorImpl = class(CalculatorImpl)
		constructor create;

		procedure dispose(); override;
		function sum(status: Status; n1: Integer; n2: Integer): Integer; override;
		function getMemory(): Integer; override;
		procedure setMemory(n: Integer); override;
		procedure sumAndStore(status: Status; n1: Integer; n2: Integer); override;

	private
		memory: Integer;
	end;

	MyCalculator2Impl = class(Calculator2Impl)
		constructor create;

		procedure dispose(); override;
		function sum(status: Status; n1: Integer; n2: Integer): Integer; override;
		function getMemory(): Integer; override;
		procedure setMemory(n: Integer); override;
		procedure sumAndStore(status: Status; n1: Integer; n2: Integer); override;
		function multiply(status: Status; n1: Integer; n2: Integer): Integer; override;
		procedure copyMemory(calculator: Calculator); override;
		procedure copyMemory2(address: IntegerPtr); override;

	private
		memory: Integer;
	end;

	MyBrokenCalculatorImpl = class(MyCalculatorImpl)
		function sum(status: Status; n1: Integer; n2: Integer): Integer; override;
	end;

	MyFactoryImpl = class(FactoryImpl)
		procedure dispose(); override;
		function createStatus(): Status; override;
		function createCalculator(status: Status): Calculator; override;
		function createCalculator2(status: Status): Calculator2; override;
		function createBrokenCalculator(status: Status): Calculator; override;
		procedure setStatusFactory(statusFactory: StatusFactory); override;

	private
		statusFactory: StatusFactory;
	end;

implementation

//--------------------------------------

// MyStatusImpl


constructor MyStatusImpl.create;
begin
	inherited;
	code := 0;
end;

procedure MyStatusImpl.dispose();
begin
	self.destroy();
end;

function MyStatusImpl.getCode(): Integer;
begin
	Result := code;
end;

procedure MyStatusImpl.setCode(n: Integer);
begin
	code := n;
end;


//--------------------------------------

// MyCalculatorImpl


constructor MyCalculatorImpl.create;
begin
	inherited;
	memory := 0;
end;

procedure MyCalculatorImpl.dispose();
begin
	self.destroy();
end;

function MyCalculatorImpl.sum(status: Status; n1: Integer; n2: Integer): Integer;
begin
	if (n1 + n2 > 1000) then
		raise CalcException.create(Status.ERROR_1)
	else
		Result := n1 + n2;
end;

function MyCalculatorImpl.getMemory(): Integer;
begin
	Result := memory;
end;

procedure MyCalculatorImpl.setMemory(n: Integer);
begin
	memory := n;
end;

procedure MyCalculatorImpl.sumAndStore(status: Status; n1: Integer; n2: Integer);
begin
	setMemory(sum(status, n1, n2));
end;


//--------------------------------------

// MyCalculator2Impl


constructor MyCalculator2Impl.create;
begin
	inherited;
	memory := 0;
end;

procedure MyCalculator2Impl.dispose();
begin
	self.destroy();
end;

function MyCalculator2Impl.sum(status: Status; n1: Integer; n2: Integer): Integer;
begin
	if (n1 + n2 > 1000) then
		raise CalcException.create(Status.ERROR_1)
	else
		Result := n1 + n2;
end;

function MyCalculator2Impl.getMemory(): Integer;
begin
	Result := memory;
end;

procedure MyCalculator2Impl.setMemory(n: Integer);
begin
	memory := n;
end;

procedure MyCalculator2Impl.sumAndStore(status: Status; n1: Integer; n2: Integer);
begin
	setMemory(sum(status, n1, n2));
end;

function MyCalculator2Impl.multiply(status: Status; n1: Integer; n2: Integer): Integer;
begin
	Result := n1 * n2;
end;

procedure MyCalculator2Impl.copyMemory(calculator: Calculator);
begin
	setMemory(calculator.getMemory());
end;

procedure MyCalculator2Impl.copyMemory2(address: IntegerPtr);
begin
	setMemory(address^);
end;


//--------------------------------------

// MyBrokenCalculatorImpl


function MyBrokenCalculatorImpl.sum(status: Status; n1: Integer; n2: Integer): Integer;
begin
	Result := inherited sum(status, n1, n2) + 1;
end;


//--------------------------------------

// MyFactoryImpl


procedure MyFactoryImpl.dispose();
begin
	self.destroy();
end;

function MyFactoryImpl.createStatus(): Status;
begin
	if (statusFactory = nil) then
		Result := MyStatusImpl.create
	else
		Result := statusFactory.createStatus();
end;

function MyFactoryImpl.createCalculator(status: Status): Calculator;
begin
	Result := MyCalculatorImpl.create;
end;

function MyFactoryImpl.createCalculator2(status: Status): Calculator2;
begin
	Result := MyCalculator2Impl.create;
end;

function MyFactoryImpl.createBrokenCalculator(status: Status): Calculator;
begin
	Result := MyBrokenCalculatorImpl.create;
end;

procedure MyFactoryImpl.setStatusFactory(statusFactory: StatusFactory);
begin
	self.statusFactory := statusFactory;
end;


end.
