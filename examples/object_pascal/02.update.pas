Program update;

{*
 *  PROGRAM:  Object oriented API samples.
 *  MODULE:    02.update.cpp
 *  DESCRIPTION:  Run once prepared statement with parameters
 *          a few times, committing transaction after each run.
 *          Learns how to prepare statement, manually define parameters
 *          for it, execute that statement with different parameters
 *          and perform non-default error processing.
 *
 *          Example for the following interfaces:
 *          IAttachment - database attachment
 *          ITransaction - transaction
 *          IStatement - SQL statement execution
 *          IMessageMetadata - describe input and output data format
 *          IMetadataBuilder - tool to modify/create metadata
 *          IStatus - return state holder
 *
 *          Note that all updates are rolled back in this version. (see *** later)
 *
 *    Run something like this to build the program :
 *
 *    fpc -Fu./common -Fu/opt/firebird/include/firebird -FUlib -oupdate 02.update.pas
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  https://www.ibphoenix.com/about/firebird/idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Paul Reeves
 *  for the Firebird Open Source RDBMS project.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 *}

{$mode Delphi}{$H+}

Uses
  SysUtils,
  Firebird;

Type

  Buffer = String [255];


Var

  // master and status are required for all access to the API.

  // This is main interface of firebird, and the only one
  // for getting which there is special function in our API
  master: IMaster;

  // Status is used to return error description to user
  status: IStatus;

  // Provider is needed to start to work with database (or service)
  prov: IProvider;

  // declare pointers to required interfaces
  att: IAttachment;
  tra: ITransaction;

  // Interface executes prepared SQL statement
  stmt: IStatement;

  // Interfaces provides access to format of data in messages
  meta: IMessageMetadata;

  // Interface makes it possible to change format of data or define it yourself
  builder: IMetadataBuilder;

  Dept_Data: Array[0..4] Of String = ( '622', '100', '116', '900', '0' );
  Percent_data: Array[0..4] Of Double = ( 0.05, 1.00, 0.075, 0.10, 0 );

  i: Integer;
  InputBuffer: Buffer;
  len: Integer;

  PPercent_inc: PChar;
  PDept_no: PChar;


Const
  UpdateString = 'UPDATE department SET budget = ? * budget + budget WHERE dept_no = ?';

  SQL_DIALECT_V6 = 3;
  SQL_DIALECT_CURRENT = SQL_DIALECT_V6;
  SQL_TEXT = 452; // CHAR
  SQL_DOUBLE = 480; // DOUBLE PRECISION


  Procedure PrintError( AMaster: IMaster; AStatus: IStatus );
  Var
    maxMessage: Integer;
    outMessage: PAnsiChar;
  Begin
    maxMessage := 256;
    outMessage := StrAlloc( maxMessage );
    AMaster.getUtilInterface.formatStatus( outMessage, maxMessage, AStatus );
    writeln( outMessage );
    StrDispose( outMessage );
  End;


  // Get the department and percent parameters for an example to run.


Begin

  master := fb_get_master_interface;
  status := master.getStatus;
  Try

    // the main dispatcher is returned by a call to IMaster
    // no errors can occur - this function will always succeed
    prov := master.getDispatcher;

    // We assume that ISC_USER and ISC_PASSWORD env vars are set. Otherwise,
    // see code in 01.create for an example of setting the un/pw via the dpb.
    att := prov.attachDatabase( status, 'employee', 0, nil );
    writeln( 'Attached to database employee.fdb' );

    // start transaction
    tra := att.startTransaction( status, 0, nil );

    // prepare statement
    stmt := att.prepare( status, tra, 0, UpdateString, SQL_DIALECT_CURRENT, 0 );

    // build metadata
    // IMaster creates empty new metadata in builder
    builder := master.getMetadataBuilder( status, 2 );
    // set required info on fields
    builder.setType( status, 0, SQL_DOUBLE + 1 );
    builder.setType( status, 1, SQL_TEXT + 1 );
    builder.setLength( status, 1, 3 );
    // IMetadata should be ready
    meta := builder.getMetadata( status );
    // no need for builder any more
    builder.Release( );
    builder := nil;

    len := meta.getMessageLength( status );
    If ( len > sizeof( InputBuffer ) ) Then
      Raise Exception.Create( 'Input message length too big - cannot continue' )
    Else
      FillChar( InputBuffer, SizeOf( InputBuffer ), 0 );

    i := meta.getNullOffset( status, 0 );
    InputBuffer[i] := Char( 0 );
    i := meta.getNullOffset( status, 1 );
    InputBuffer[i] := Char( 0 );

    Try
      // locations of parameters in input message
      PPercent_inc := PChar( @InputBuffer [meta.getOffset( status, 0 )] );
      PDept_no := PChar( @InputBuffer [meta.getOffset( status, 1 )] );
      For i := 0 To length( Dept_Data ) - 1 Do Begin
        If ( Dept_Data [i] = '0' ) Or ( Percent_data [i] = 0 ) Then
          break;
        StrPCopy( PPercent_inc, Percent_data [i].ToString );
        StrPCopy( PDept_no, Dept_Data [i] );
        WriteLn( 'Increasing budget for department:  ' + PDept_no + '  by ' + PPercent_inc + ' percent.' );
        Try
          stmt.Execute( status, tra, meta, @InputBuffer, nil, nil );

          // Save/Cancel each department's update independently.
          // *** Change to commitRetaining() to see changes
          // *** tra.commitRetaining(status);
          tra.rollbackRetaining( status );
        Except
          on E: FBException Do Begin
            PrintError( master, status );
            tra.rollbackRetaining( status );
          End;
        End;
      End;

      stmt.Free( status );
      stmt := nil;

      meta.Release;
      meta := nil;

      tra.commit( status );
      tra := nil;

      att.detach( status );
      att := nil;
    Except
      on E: FBException Do Begin
        PrintError( master, status );
        tra.rollbackRetaining( status );
      End;
      on E: Exception Do
        WriteLn( E.Message );
    End;

  Finally
    If assigned( meta ) Then
      meta.Release;
    If assigned( builder ) Then
      builder.Release;
    If assigned( stmt ) Then
      stmt.Release;
    If assigned( tra ) Then
      tra.Release;
    If assigned( att ) Then
      att.Release;

    prov.Release;
    status.dispose;
  End;
End.
