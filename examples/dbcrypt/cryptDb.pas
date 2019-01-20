{
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		cryptDb.pas
 *	DESCRIPTION:	Sample of how diskcrypt may be written using pascal.
 *					Does XOR 5 for all bytes in passed data.
 *
 *					Run something like this to build:
 *					fpc -Fu<path-to-Firebird.pas> -Mdelphi -fPIC cryptDb.pas
 *
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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2016 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________. }

library cryptDb;

uses
  SysUtils,
  Classes,
  Firebird;

Type
  TMyPluginModule = class(IPluginModuleImpl)
  private
    FRegistered: Boolean;

  public
    constructor Create;
    destructor Destroy; override;

    procedure registerMe;

    // TPluginModule implementation
    procedure doClean; override;
	procedure threadDetach; override;
  end;

  TMyCrypt = class(IDbCryptPluginImpl)
  private
    FCounter: Integer;
    FOwner: IReferenceCounted;
    FConfig: IPluginConfig;

  public
    constructor Create(config: IPluginConfig);
    destructor Destroy; override;

    // TRefCounted implementation
    procedure addRef; override;
    function release: Integer; override;

    // TPluginBase implementation
    procedure setOwner(ref: IReferenceCounted); override;
    function getOwner: IReferenceCounted; override;

    // TCryptPlugin implementation
    procedure setKey(status: IStatus; length: Cardinal; sources: IKeyHolderPluginPtr; keyName: PAnsiChar); override;
    procedure encrypt(status: IStatus; length: Cardinal; src, dst: Pointer); override;
    procedure decrypt(status: IStatus; length: Cardinal; src, dst: Pointer); override;
    procedure setInfo(status: IStatus; info: IDbCryptInfo); override;

  private
    procedure pxor(length: Cardinal; mem: Pointer);
  end;

  TMyFactory = class(IPluginFactoryImpl)
  public
    constructor Create(module: IPluginModule);
    destructor Destroy; override;

    // TPluginFactory implementation
    function createPlugin(status: IStatus; factoryParameter: IPluginConfig): IPluginBase; override;
  end;

/// implementation

Var
  Master: IMaster = nil;

  { TMyPluginModule }

constructor TMyPluginModule.Create;
begin
  inherited;
  FRegistered := false;
end;

destructor TMyPluginModule.Destroy;
begin
  if FRegistered then
  begin
    Master.getPluginManager.unregisterModule(Self);
    doClean();
  end;

  inherited;
end;

procedure TMyPluginModule.doClean;
begin
  FRegistered := False;
end;

procedure TMyPluginModule.threadDetach; 
begin
end;

procedure TMyPluginModule.registerMe;
begin
  if not FRegistered then
  begin
    Master.getPluginManager.registerModule(Self);
    FRegistered := True;
  end
end;

{ TMyFactory }

function TMyFactory.createPlugin(status: IStatus; factoryParameter: IPluginConfig): IPluginBase;
var
  plugin: IPluginBase;
begin
  plugin := TMyCrypt.Create(factoryParameter);
  plugin.addRef;
  Result := plugin;
end;

{ TMyCrypt }

constructor TMyCrypt.Create(config: IPluginConfig);
begin
  Inherited Create;

  FOwner := nil;
  FConfig := config;
  FConfig.addRef;
end;

destructor TMyCrypt.Destroy;
begin
  FConfig.release;
  FConfig := nil;

  inherited;
end;

procedure TMyCrypt.addRef;
begin
  InterlockedIncrement(FCounter);
end;

function TMyCrypt.release: Integer;
begin
  if InterlockedDecrement(FCounter) = 0 then
  begin
    Result := 0;
    Free;
  end
  else
    Result := 1;
end;

procedure TMyCrypt.setOwner(ref: IReferenceCounted);
begin
  FOwner := ref;
end;

function TMyCrypt.getOwner: IReferenceCounted;
begin
  Result := FOwner;
end;

procedure TMyCrypt.setInfo(status: IStatus; info: IDbCryptInfo);
begin
  status.init;

  // do nothing in this trivial sample
end;

procedure TMyCrypt.decrypt(status: IStatus; length: Cardinal; src, dst: Pointer);
begin
  status.init;

  // decrypt here
  Move(src^, dst^, length);
  pxor(length, dst);
end;

procedure TMyCrypt.encrypt(status: IStatus; length: Cardinal; src, dst: Pointer);
begin
  status.init;

  // encrypt here
  Move(src^, dst^, length);
  pxor(length, dst);
end;

procedure TMyCrypt.setKey(status: IStatus; length: Cardinal; sources: IKeyHolderPluginPtr; keyName: PAnsiChar);
begin
  status.init;
  // get encryption key "keyName" from "sources" if necessary
end;

procedure TMyCrypt.pxor(length: Cardinal; mem: Pointer);
var
  ptr: BytePtr;
  i: Integer;
begin
  ptr := BytePtr(mem);
  for i := 1 to length do
  begin
    ptr^ := (ptr^) xor 5;
    Inc(ptr);
  end;
end;

{ Factory }

constructor TMyFactory.Create(module: IPluginModule);
begin
  Inherited Create;
end;

destructor TMyFactory.Destroy;
begin
  inherited;
end;

{ entrypoint }

Procedure firebird_plugin(masterInterface: IMaster); cdecl; export;
var
  pluginManager: IPluginManager;
  module: TMyPluginModule;
  factory: IPluginFactory;
Begin
  Master := masterInterface;
  pluginManager := master.getPluginManager;

  module := TMyPluginModule.Create;
  module.registerMe;

  factory := TMyFactory.Create(module);
  pluginManager.registerPluginFactory(IPluginManager.TYPE_DB_CRYPT, 'cryptDb', factory);
End;

exports
  firebird_plugin;

begin

end.

