Program select;

{
 *  PROGRAM:  Object oriented API samples.
 *  MODULE:    03.select.pas
 *  DESCRIPTION:
 *    A sample of running SELECT statement without parameters.
 *    Prints string fields in a table, coercing VARCHAR to CHAR.
 *    Learns how to coerce output data in prepared statement
 *    and execute it.
 *
 *    Example for the following interfaces:
 *
 *    IStatement - SQL statement execution
 *    IMessageMetadata - describe input and output data format
 *    IResultSet - fetch data returned by statement after execution
 *
 *    Run something like this to build the program :
 *
 *    fpc -Fu./common -Fu/opt/firebird/include/firebird -FUlib 03.select.pas
 *
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
 *  Most of the code for GetOutput was taken from Denis
 *  Simonov's UDR-Book project.
 *
 *  Copyright (c) 2020 Paul Reeves <preeves@ibphoenix.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________. }



{$mode Delphi}{$H+}

Uses {$IFDEF UNIX} {$IFDEF UseCThreads}
  cthreads
  , {$ENDIF} {$ENDIF}
  SysUtils
  , Firebird
  , strutils
  , FbCharsets
  ;

//  Record to store received metadata
Type
  TField = Record
    fieldname: String;
    fieldtype: Cardinal;
    fieldlength: Integer;
    offset: Integer;
    sqlnullind: Wordbool;
    charset: TFBCharSet;
    charLength: Integer;
    fieldvalue: String;
  End;


Var

  // master and status are required for all access to the API.

  // This is main interface of firebird, and the only one
  // for getting which there is special function in our API
  master: IMaster;

  // Status is used to return error descriptions to user
  status: IStatus;

  // Provides some miscellaneous utilities.
  util: IUtil;

  // Provider is needed to start to work with database (or service)
  prov: IProvider;

  // Attachment and Transaction contain methods to work with
  // database attachment and transaction
  att: IAttachment;
  tra: ITransaction;
  tpb: IXpbBuilder;

  // to prepare an sql statement
  stmt: IStatement;

  // We geain access to the result set with a cursor
  curs: IResultSet;

  // Retrieve info about metadata of a statement
  meta: IMessageMetadata;

  builder: IMetadataBuilder;

  // Store the meta data of each field in the result set
  fields: Array Of TField;

  // Store the titles of each field in the result set
  title: String = '';

  // msg is a pointer to each row in the result set.
  msg: Pointer;
  msgLen: Cardinal;


  counter: Integer;

Const
  // Firebird types
  SQL_VARYING = 448; // VARCHAR
  SQL_TEXT = 452; // CHAR


  Procedure PrintError(AMaster: IMaster; AStatus: IStatus);
  Var
    maxMessage: Integer;
    outMessage: PAnsiChar;
  Begin
    maxMessage := 256;
    outMessage := StrAlloc(maxMessage);
    AMaster.getUtilInterface.formatStatus(outMessage, maxMessage, AStatus);
    writeln(outMessage);
    StrDispose(outMessage);
  End;


  Function GetOutput(AStatus: IStatus; ABuffer: PByte; AMeta: IMessageMetadata; AUtil: IUtil;
    AFieldsArray: Array Of TField): UnicodeString;
  Var
    i: Integer;
    NullFlag: Wordbool;
    pData: PByte;
    CharBuffer: TBytes;
    StringValue: UnicodeString;
    current_field: TField;

  Begin
    Result := '';

    For i := 0 To length(AFieldsArray) - 1 Do Begin
      current_field := AfieldsArray[i];
      With current_field Do Begin

        NullFlag := PWordBool(ABuffer + AMeta.getNullOffset(AStatus, i))^;
        If NullFlag Then Begin
          StringValue := 'NULL';
          continue;
        End;

        // get a pointer to the field data
        pData := ABuffer + AMeta.getOffset(AStatus, i);
        pData := ABuffer + offset;

        Case fieldType Of

          SQL_VARYING:
          Begin
            SetLength(CharBuffer, fieldLength);
            // For VARCHAR, the first 2 bytes are the length
            charLength := PSmallint(pData)^;
            // For VARCHAR, the first 2 bytes are the length in bytes
            // so we copy it to the buffer starting at 3 bytes
            Move((pData + 2)^, CharBuffer[0], fieldLength);
            StringValue := charset.GetString(CharBuffer, 0, charLength);
          End;

          Else
            StringValue := ' Fieldtype not handled.';

        End; // case fieldType of

        If Result = '' Then
          Result := Result + UnicodeString(PadRight(UTF8Encode(StringValue), fieldLength))
        Else
          Result := Result + '  ' + UnicodeString(PadRight(UTF8Encode(StringValue), fieldLength));

      End; // end with current_field

    End; // for i := 0 to length(AFieldsArray) - 1 do begin

  End; // function GetOutput

Begin

  master := fb_get_master_interface;
  status := master.getStatus;


  // Here we get access to the helper utility interfaces
  // no errors can occur - this function will always succeed
  util := master.getUtilInterface;

  // the main dispatcher is returned by a call to IMaster
  // no errors can occur - this function will always succeed
  prov := master.getDispatcher;

  Try
    Try
      // attach to employee db
      // We assume that ISC_USER and ISC_PASSWORD env vars are set. Otherwise,
      // see code in 01.create for an example of setting the un/pw via the dpb.
      att := prov.attachDatabase(status, 'employee', 0, nil);
      writeln('Attached to database employee.fdb');

      // start read only transaction
      tpb := util.getXpbBuilder(status, IXpbBuilder.TPB, nil, 0);
      tpb.insertTag(status, isc_tpb_read_committed);
      tpb.insertTag(status, isc_tpb_no_rec_version);
      tpb.insertTag(status, isc_tpb_wait);
      tpb.insertTag(status, isc_tpb_read);

      // start transaction
      tra := att.startTransaction(status, tpb.getBufferLength( status ), tpb.getBuffer( status ));

      // prepare statement
      stmt := att.prepare(status, tra, 0, 'Select last_name, first_name, phone_ext from phone_list ' +
        'where location = ''Monterey'' order by last_name, first_name', 3,
        IStatement.PREPARE_PREFETCH_METADATA);

      // get list of columns
      meta := stmt.getOutputMetadata(status);
      builder := meta.getBuilder(status);
      SetLength(fields, meta.getCount(status));

      // parse columns list & coerce datatype(s)
      For counter := 0 To length(fields) - 1 Do Begin
        If ((meta.getType(status, counter) = (SQL_VARYING Or SQL_TEXT))) Then
          builder.setType(status, counter, SQL_TEXT);
        fields[counter].fieldname := meta.getField(status, counter);
      End;
      // release automatically created metadata
      // metadata is not database object, therefore no specific call to close it
      meta.Release;

      // get metadata with coerced datatypes
      meta := builder.getMetadata(status);

      // builder is no longer needed
      builder.Release;
      builder := nil;

      // now get field info
      For counter := 0 To length(fields) - 1 Do Begin
        If fields[counter].fieldname <> '' Then Begin
          fields[counter].fieldlength := meta.getLength(status, counter);
          fields[counter].offset := meta.getOffset(status, counter);
          fields[counter].fieldType := meta.getType(status, counter) And Not 1;
          Case fields[counter].fieldType Of
            SQL_TEXT, SQL_VARYING:
              fields[counter].charset := TFBCharSet(meta.getCharSet(status, counter));
            Else
              ;
          End;
          // Set the title line for later use.
          If title = '' Then
            title := title + fields[counter].fieldname.PadRight(fields[counter].fieldlength)
          Else
            title := title + '  ' + fields[counter].fieldname.PadRight(fields[counter].fieldlength);
        End;
      End;

      // open cursor
      curs := stmt.openCursor(status, tra, nil, nil, meta, 0);

      // allocate output buffer
      msgLen := meta.getMessageLength(status);
      msg := AllocMem(msgLen);

      counter := 0;
      While curs.fetchNext(status, msg) = IStatus.RESULT_OK Do Begin
        If ((counter Mod 10) = 0) Then Begin
          writeln('');
          writeln(title);
        End;
        Inc(counter);
        WriteLn(GetOutput(status, msg, meta, util, fields));
      End;

      // What is correct way to close and release?
      // close interfaces
      curs.Close(status);
      stmt.Free(status);
      meta.Release();
      tra.commit(status);
      att.detach(status);

    Except
      on e: FbException Do
        PrintError(master, e.getStatus);
    End;
  Finally
    If assigned(meta) Then
      meta.Release;
    If assigned(builder) Then
      builder.Release;
    If assigned(curs) Then
      curs.Release;
    If assigned(stmt) Then
      stmt.Release;
    If assigned(tra) Then
      tra.Release;
    If assigned(att) Then
      att.Release;
    If assigned(tpb) Then
      tpb.dispose;

    prov.Release;
    status.dispose;
  End;

End.


