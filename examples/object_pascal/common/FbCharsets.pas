{
 *	PROGRAM:	UDR samples.
 *	MODULE:		FbCharsets.pas
 *	DESCRIPTION:	Charset helpers.
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
 *  The Original Code was created by Simonov Denis
 *  for the book Writing UDR Firebird in Pascal.
 *
 *  Copyright (c) 2018 Simonov Denis <sim-mail@list.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________. }

unit FbCharsets;

{$IFDEF MSWINDOWS}
{$DEFINE WINDOWS}
{$ENDIF}
{$IFDEF FPC}
{$mode delphi}
{$ENDIF}


interface

uses
  Classes, SysUtils {$IFDEF WINDOWS}, windows {$ENDIF};

type

// Firebird character sets
TFBCharSet = (
  CS_NONE = 0, // No Character Set
  CS_BINARY = 1, // BINARY BYTES
  CS_ASCII = 2, // ASCII
  CS_UNICODE_FSS = 3, // UNICODE in FSS format
  CS_UTF8 = 4, // UTF-8
  CS_SJIS = 5, // SJIS
  CS_EUCJ = 6, // EUC-J

  CS_JIS_0208 = 7 , // JIS 0208; 1990
  CS_UNICODE_UCS2 = 8 , // UNICODE v 1.10

  CS_DOS_737 = 9,
  CS_DOS_437 = 10 , // DOS CP 437
  CS_DOS_850 = 11 , // DOS CP 850
  CS_DOS_865 = 12 , // DOS CP 865
  CS_DOS_860 = 13 , // DOS CP 860
  CS_DOS_863 = 14 , // DOS CP 863

  CS_DOS_775 = 15,
  CS_DOS_858 = 16,
  CS_DOS_862 = 17,
  CS_DOS_864 = 18,

  CS_NEXT = 19, // NeXTSTEP OS native charset

  CS_ISO8859_1 = 21, // ISO-8859.1
  CS_ISO8859_2 = 22, // ISO-8859.2
  CS_ISO8859_3 = 23, // ISO-8859.3
  CS_ISO8859_4 = 34, // ISO-8859.4
  CS_ISO8859_5 = 35, // ISO-8859.5
  CS_ISO8859_6 = 36, // ISO-8859.6
  CS_ISO8859_7 = 37, // ISO-8859.7
  CS_ISO8859_8 = 38, // ISO-8859.8
  CS_ISO8859_9 = 39, // ISO-8859.9
  CS_ISO8859_13 = 40, // ISO-8859.13

  CS_KSC5601 = 44, // KOREAN STANDARD 5601

  CS_DOS_852 = 45 , // DOS CP 852
  CS_DOS_857 = 46 , // DOS CP 857
  CS_DOS_861 = 47 , // DOS CP 861

  CS_DOS_866 = 48,
  CS_DOS_869 = 49,

  CS_CYRL = 50 ,
  CS_WIN1250 = 51, // Windows cp 1250
  CS_WIN1251 = 52, // Windows cp 1251
  CS_WIN1252 = 53, // Windows cp 1252
  CS_WIN1253 = 54, // Windows cp 1253
  CS_WIN1254 = 55, // Windows cp 1254

  CS_BIG5 = 56, // Big Five unicode cs
  CS_GB2312 = 57, // GB 2312-80 cs

  CS_WIN1255 = 58, // Windows cp 1255
  CS_WIN1256 = 59, // Windows cp 1256
  CS_WIN1257 = 60, // Windows cp 1257

  CS_UTF16 = 61, // UTF-16
  CS_UTF32 = 62, // UTF-32

  CS_KOI8R = 63, // Russian KOI8R
  CS_KOI8U = 64, // Ukrainian KOI8U

  CS_WIN1258 = 65, // Windows cp 1258

  CS_TIS620 = 66 , // TIS620
  CS_GBK = 67, // GBK
  CS_CP943C = 68, // CP943C

  CS_GB18030 = 69 // GB18030
);

// Firebird character set mappig to code pages
TCharsetMap = record
  CharsetID: Integer;
  CharSetName: AnsiString;
  CharSetWidth: Word;
  CodePage: Integer;
end;

{ TFbCharsetHelper }

TFbCharsetHelper = record helper for TFBCharSet
  function  GetCharset : TCharsetMap;
  function GetCodePage: Integer;
  function GetCharWidth: Word;
  function GetCharSetName: string;
  function  GetEncoding : TEncoding;
  function GetString(const Bytes: TBytes; ByteIndex, ByteCount: Integer): UnicodeString;
end;


implementation

const
  CharSetMap: array [0 .. 69] of TCharsetMap = (
    (CharsetID: 0; CharSetName: 'NONE'; CharSetWidth: 1; CodePage: CP_ACP),
    (CharsetID: 1; CharSetName: 'OCTETS'; CharSetWidth: 1; CodePage: CP_NONE),
    (CharsetID: 2; CharSetName: 'ASCII'; CharSetWidth: 1; CodePage:  {CP_ASCII}  CP_ACP),
    (CharsetID: 3; CharSetName: 'UNICODE_FSS'; CharSetWidth: 3; CodePage: CP_UTF8),
    (CharsetID: 4; CharSetName: 'UTF8'; CharSetWidth: 4; CodePage: CP_UTF8),
    (CharsetID: 5; CharSetName: 'SJIS_0208'; CharSetWidth: 2; CodePage: 20932),
    (CharsetID: 6; CharSetName: 'EUCJ_0208'; CharSetWidth: 2; CodePage: 20932),
    (CharsetID: 7; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 8; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 9; CharSetName: 'DOS737'; CharSetWidth: 1; CodePage: 737),
    (CharsetID: 10; CharSetName: 'DOS437'; CharSetWidth: 1; CodePage: 437),
    (CharsetID: 11; CharSetName: 'DOS850'; CharSetWidth: 1; CodePage: 850),
    (CharsetID: 12; CharSetName: 'DOS865'; CharSetWidth: 1; CodePage: 865),
    (CharsetID: 13; CharSetName: 'DOS860'; CharSetWidth: 1; CodePage: 860),
    (CharsetID: 14; CharSetName: 'DOS863'; CharSetWidth: 1; CodePage: 863),
    (CharsetID: 15; CharSetName: 'DOS775'; CharSetWidth: 1; CodePage: 775),
    (CharsetID: 16; CharSetName: 'DOS858'; CharSetWidth: 1; CodePage: 858),
    (CharsetID: 17; CharSetName: 'DOS862'; CharSetWidth: 1; CodePage: 862),
    (CharsetID: 18; CharSetName: 'DOS864'; CharSetWidth: 1; CodePage: 864),
    (CharsetID: 19; CharSetName: 'NEXT'; CharSetWidth: 1; CodePage: CP_NONE),
    (CharsetID: 20; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 21; CharSetName: 'ISO8859_1'; CharSetWidth: 1; CodePage: 28591),
    (CharsetID: 22; CharSetName: 'ISO8859_2'; CharSetWidth: 1; CodePage: 28592),
    (CharsetID: 23; CharSetName: 'ISO8859_3'; CharSetWidth: 1; CodePage: 28593),
    (CharsetID: 24; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 25; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 26; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 27; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 28; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 29; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 30; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 31; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 32; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 33; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 34; CharSetName: 'ISO8859_4'; CharSetWidth: 1; CodePage: 28594),
    (CharsetID: 35; CharSetName: 'ISO8859_5'; CharSetWidth: 1; CodePage: 28595),
    (CharsetID: 36; CharSetName: 'ISO8859_6'; CharSetWidth: 1; CodePage: 28596),
    (CharsetID: 37; CharSetName: 'ISO8859_7'; CharSetWidth: 1; CodePage: 28597),
    (CharsetID: 38; CharSetName: 'ISO8859_8'; CharSetWidth: 1; CodePage: 28598),
    (CharsetID: 39; CharSetName: 'ISO8859_9'; CharSetWidth: 1; CodePage: 28599),
    (CharsetID: 40; CharSetName: 'ISO8859_13'; CharSetWidth: 1; CodePage: 28603),
    (CharsetID: 41; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 42; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 43; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 44; CharSetName: 'KSC_5601'; CharSetWidth: 2; CodePage: 949),
    (CharsetID: 45; CharSetName: 'DOS852'; CharSetWidth: 1; CodePage: 852),
    (CharsetID: 46; CharSetName: 'DOS857'; CharSetWidth: 1; CodePage: 857),
    (CharsetID: 47; CharSetName: 'DOS861'; CharSetWidth: 1; CodePage: 861),
    (CharsetID: 48; CharSetName: 'DOS866'; CharSetWidth: 1; CodePage: 866),
    (CharsetID: 49; CharSetName: 'DOS869'; CharSetWidth: 1; CodePage: 869),
    (CharsetID: 50; CharSetName: 'CYRL'; CharSetWidth: 1; CodePage: 1251),
    (CharsetID: 51; CharSetName: 'WIN1250'; CharSetWidth: 1; CodePage: 1250),
    (CharsetID: 52; CharSetName: 'WIN1251'; CharSetWidth: 1; CodePage: 1251),
    (CharsetID: 53; CharSetName: 'WIN1252'; CharSetWidth: 1; CodePage: 1252),
    (CharsetID: 54; CharSetName: 'WIN1253'; CharSetWidth: 1; CodePage: 1253),
    (CharsetID: 55; CharSetName: 'WIN1254'; CharSetWidth: 1; CodePage: 1254),
    (CharsetID: 56; CharSetName: 'BIG_5'; CharSetWidth: 2; CodePage: 950),
    (CharsetID: 57; CharSetName: 'GB_2312'; CharSetWidth: 2; CodePage: 936),
    (CharsetID: 58; CharSetName: 'WIN1255'; CharSetWidth: 1; CodePage: 1255),
    (CharsetID: 59; CharSetName: 'WIN1256'; CharSetWidth: 1; CodePage: 1256),
    (CharsetID: 60; CharSetName: 'WIN1257'; CharSetWidth: 1; CodePage: 1257),
    (CharsetID: 61; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 62; CharSetName: 'Unknown'; CharSetWidth: 0; CodePage: CP_NONE),
    (CharsetID: 63; CharSetName: 'KOI8R'; CharSetWidth: 1; CodePage: 20866),
    (CharsetID: 64; CharSetName: 'KOI8U'; CharSetWidth: 1; CodePage: 21866),
    (CharsetID: 65; CharSetName: 'WIN1258'; CharSetWidth: 1; CodePage: 1258),
    (CharsetID: 66; CharSetName: 'TIS620'; CharSetWidth: 1; CodePage: 874),
    (CharsetID: 67; CharSetName: 'GBK'; CharSetWidth: 2; CodePage: 936),
    (CharsetID: 68; CharSetName: 'CP943C'; CharSetWidth: 2; CodePage: 943),
    (CharsetID: 69; CharSetName: 'GB18030'; CharSetWidth: 4; CodePage: 54936));

{ TFbCharset }

function TFbCharsetHelper.GetCharset(): TCharsetMap;
begin
  Result := CharSetMap[Integer(Self)];
end;

function TFbCharsetHelper.GetCodePage(): Integer;
begin
  Result := CharSetMap[Integer(Self)].CodePage;
end;

function TFbCharsetHelper.GetCharWidth(): Word;
begin
  Result := CharSetMap[Integer(Self)].CharSetWidth;
end;

function TFbCharsetHelper.GetCharSetName(): string;
begin
  Result := CharSetMap[Integer(Self)].CharSetName;
end;

function  TFbCharsetHelper.GetEncoding (): TEncoding;
begin
  Result := TEncoding.GetEncoding(CharSetMap[Integer(Self)].CodePage);
end;

function TFbCharsetHelper.GetString(
  const Bytes: TBytes; ByteIndex, ByteCount: Integer): UnicodeString;
var
  xEncoding: TEncoding;
begin
  xEncoding := GetEncoding();
  try
    Result := xEncoding.GetString(Bytes, ByteIndex, ByteCount);
  finally
    xEncoding.Free;
  end;
end;


end.


