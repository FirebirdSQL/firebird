(*  Initial Developer's Public License.
  The contents of this file are subject to the  Initial Developer's Public
  License Version 1.0 (the "License"). You may not use this file except
  in compliance with the License. You may obtain a copy of the License at
    http://www.ibphoenix.com?a=ibphoenix&page=ibp_idpl
  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
  for the specific language governing rights and limitations under the
  License.

  The Original Code is copyright 2007 Paul Reeves.

  The Initial Developer of the Original Code is Paul Reeves.

  All Rights Reserved.

  GUI related functions for the Firebird Installer.

  o ResizeWizardFormHeight
     - Change height of Wizard Form
     - Adjust the position of each control to work in new height.

  o ResizeWizardFormWidth
     - Make form wider

  o MoveOuterNoteBook
     - Move OuterNotebook and Bevel.
       Value moved should match value passed to ResizeWizardFormWidth.

  o EnumerateObjectList;
     - Simple demonstration of object enumeration.

  o Show Help
    - CreateHelpDlg;
    - ShowHelpDlg: Integer;
    - CloseHelpDlg;

  o Show some debug info for debugging inno setup script
    - CreateDebugDlg
    - CloseDebugDlg;
    - ShowDebugDlg

*)

var
  CompGroup1Array: array of string;
  CompGroup2Array: array of string;
  CompGroup3Array: array of string;
  CompGroupButtonsArray: array of string;
  CompGroup1, CompGroup2, CompGroup3, CompGroupButtons: TStringList;

// This is a non-exhaustive list of components that need to be repositioned
// and/or resized if the form size is changed. Use EnumerateObjectList to
// get a full list of components.
procedure SetupWizardFormComponentsArrays;
var
  i: integer;
begin

  SetArrayLength(CompGroup1Array,24);

  CompGroup1Array[0]:='WelcomePage';
  CompGroup1Array[1]:='WelcomeLabel2';
  CompGroup1Array[2]:='InnerPage';
  CompGroup1Array[3]:='InnerNotebook';
  CompGroup1Array[4]:='LicensePage';
  CompGroup1Array[5]:='LicenseMemo';
  CompGroup1Array[6]:='PasswordPage';
  CompGroup1Array[7]:='InfoBeforePage';
  CompGroup1Array[8]:='InfoBeforeMemo';
  CompGroup1Array[9]:='UserInfoPage';
  CompGroup1Array[10]:='SelectDirPage';
  CompGroup1Array[11]:='SelectComponentsPage';
  CompGroup1Array[12]:='ComponentsList';
  CompGroup1Array[13]:='SelectProgramGroupPage';
  CompGroup1Array[14]:='SelectTasksPage';
  CompGroup1Array[15]:='TasksList';
  CompGroup1Array[16]:='ReadyPage';
  CompGroup1Array[17]:='ReadyMemo';
  CompGroup1Array[18]:='PreparingPage';
  CompGroup1Array[19]:='InstallingPage';
  CompGroup1Array[20]:='InfoAfterPage';
  CompGroup1Array[21]:='InfoAfterMemo';
  CompGroup1Array[22]:='FinishedPage';
  CompGroup1Array[23]:='OuterNotebook';

  CompGroup1 := TStringList.create;

  for i := 0 to GetArrayLength( CompGroup1Array )-1 do begin
    CompGroup1.add( CompGroup1Array[i] );
    CompGroup1.objects[i] := (WizardForm.FindComponent(CompGroup1Array[i]));
  end;

  SetArrayLength(CompGroup2Array,8);
  CompGroup2Array[0] := 'Bevel';
  CompGroup2Array[1] := 'BeveledLabel';
  CompGroup2Array[2] := 'LicenseAcceptedRadio';
  CompGroup2Array[3] := 'LicenseNotAcceptedRadio';
  CompGroup2Array[4] := 'DiskSpaceLabel';
  CompGroup2Array[5] := 'ComponentsDiskSpaceLabel';
  CompGroup2Array[6] := 'NoIconsCheck';
  CompGroup2Array[7] := 'RunList';

  CompGroup2 := TStringList.create;
  for i := 0 to GetArrayLength( CompGroup2Array )-1 do begin
    CompGroup2.add(CompGroup2Array[i]);
    CompGroup2.objects[i] := (WizardForm.FindComponent(CompGroup2Array[i]));
  end;

  SetArrayLength(CompGroup3Array,1);
  CompGroup3Array[0] := 'WizardBitmapImage';
  CompGroup3 := TStringList.create;
  for i:=0 to GetArrayLength(CompGroup3Array)-1 do begin
    CompGroup3.add(CompGroup3Array[i]);
    CompGroup3.objects[i] := (WizardForm.FindComponent(CompGroup3Array[i]));
  end;

  SetArrayLength(CompGroupButtonsArray,3);
  CompGroupButtonsArray[0] := 'BackButton';
  CompGroupButtonsArray[1] := 'NextButton';
  CompGroupButtonsArray[2] := 'CancelButton';
  CompGroupButtons := TStringList.create;
  for i:=0 to GetArrayLength(CompGroupButtonsArray)-1 do begin
    CompGroupButtons.add(CompGroupButtonsArray[i]);
    CompGroupButtons.objects[i] := (WizardForm.FindComponent(CompGroupButtonsArray[i]));
  end;

end;

procedure ResizeWizardFormHeight(AValue: Integer);
var
  i: integer;
begin
    wizardform.height := wizardform.height+AValue;
    wizardform.top := wizardform.top - (AValue div 2);

    // Group 1 - reset height of components
    for i:=0 to GetArrayLength(CompGroup1Array)-1 do
      TControl(CompGroup1.objects[i]).height := TControl(CompGroup1.objects[i]).height + Avalue;

    // Group 2 - reset top of components
    for i:=0 to GetArrayLength(CompGroup2Array)-1 do
      TControl(CompGroup2.objects[i]).top := TControl(CompGroup2.objects[i]).top + AValue;

    // Group 3 - reset top of components but keep 'centered' vertically.
    for i:=0 to GetArrayLength(CompGroup3Array)-1 do
      TControl( CompGroup3.objects[i]).top := TControl(CompGroup3.objects[i]).top + (AValue div 2);

    // Group Buttons - reset top of components
    for i:=0 to GetArrayLength(CompGroupButtonsArray)-1 do
      TControl(CompGroupButtons.objects[i]).top := TControl(CompGroupButtons.objects[i]).top + AValue;

end;


// This needs more work. for now it is not used by the installer.
procedure ResizeWizardFormWidth(AValue: Integer);
var
  i: integer;
begin
    wizardform.width := wizardform.width + AValue;
    wizardform.left := wizardform.left - (AValue div 2);

    // Group 1 - reset width of components
    for i:=0 to GetArrayLength(CompGroup1Array)-1 do begin
      TControl(CompGroup1.objects[i]).width := TControl(CompGroup1.objects[i]).width + Avalue;
//      TControl(CompGroup1.objects[i]).left := TControl(CompGroup1.objects[i]).left + (Avalue div 2);
    end;

    // Group 2 - reset left of components
    for i:=0 to GetArrayLength(CompGroup2Array)-1 do begin
      TControl(CompGroup2.objects[i]).left := TControl(CompGroup2.objects[i]).left + AValue;
      TControl(CompGroup2.objects[i]).width := TControl(CompGroup2.objects[i]).width + (AValue div 2);
    end;

    // Group 3 - reset left of components but keep 'centered' horizontally.
//		for i:=0 to GetArrayLength(CompGroup3Array)-1 do
//			TControl( CompGroup3.objects[i]).left := TControl(CompGroup3.objects[i]).left - (AValue div 2);

		// Group 4 - reset left of buttons
		for i:=0 to GetArrayLength(CompGroupButtonsArray)-1 do begin
			TControl(CompGroupButtons.objects[i]).left := TControl(CompGroupButtons.objects[i]).left + AValue;
		end;

end;

procedure MoveOuterNoteBookRight(AValue: Integer);
begin
  with wizardform do begin
   OuterNotebook.left := wizardform.OuterNotebook.left + AValue;
   Bevel.left := Bevel.left + AValue;
  end;
end;

procedure MoveOuterNoteBookLeft(AValue: Integer);
begin
  with wizardform do begin
   OuterNotebook.left := wizardform.OuterNotebook.left - AValue;
   Bevel.left := Bevel.left - AValue;
  end;
end;

const
  group1 = ' group one';
  group2 = ' group two';
  group3 = ' group three';
  group4 = ' group not allocated.';

procedure EnumerateObjectList;
//Walk through the components on the InnoSetup install wizard
var
  Alist: TStringList;
  i: integer;
  aComponent: string;

begin
  Alist := TStringList.create;
  for i := 0 to WizardForm.ComponentCount -1 do begin
    aComponent := WizardForm.Components[i].name;
    if CompGroup1.IndexOf( aComponent ) > -1 then
      aComponent := aComponent + group1
    else if CompGroup2.IndexOf( aComponent ) > -1 then
      aComponent := aComponent + group2
    else if CompGroup3.IndexOf( aComponent ) > -1 then
      aComponent := aComponent + group3
    else
      aComponent := aComponent + group4;


    AList.add( aComponent );
  end;
  WizardForm.INFOBEFOREMEMO.lines.assign(Alist);
end;


var
  HelpDlg: TForm;

procedure CreateHelpDlg;
var
  HelpFileName: String;
  HelpMemo: TMemo;
  OKButton: TButton;
begin
  ExtractTemporaryFile ('installation_scripted.txt');
  HelpFileName := ExpandConstant ('{tmp}\installation_scripted.txt');
  HelpDlg := TForm.create(nil);
  with HelpDlg do begin
      BorderStyle := bsDialog;
      Position := poScreenCenter;
      ClientWidth := 450;
      ClientHeight := 550;
      Caption := 'Firebird Installation - command-line parameters.';
  end;
  HelpMemo := TMemo.Create(HelpDlg);
  with HelpMemo do begin
      Parent := HelpDlg;
      ScrollBars := ssVertical;
      Lines.LoadFromFile(HelpFileName);
      Align := alTop;
      Height := 500;
      ReadOnly := True;
    end;
  OKButton := TButton.create(HelpDlg);
  with OKButton do begin
      Parent := HelpDlg;
      Left := (Parent.width  div 2) - width div 2;
      top := Parent.ClientHeight - 40;
      Caption := 'OK';
      ModalResult := mrOK;
  end;
  HelpDlg.ActiveControl := OKButton;
end;


procedure CloseHelpDlg;
begin
  with HelpDlg do begin
     Close;
     Free;
  end;
end;


function ShowHelpDlg: Integer;
begin
  CreateHelpDlg;
  result := HelpDlg.ShowModal;
  CloseHelpDlg;
end;


// Add a simple debug dialogue
var
  DebugDlg: TForm;

procedure CreateDebugDlg(aString: String; ADescription: String);
var
  DebugText: TMemo;
//  DebugDescription: TLabel;
  OKButton: TButton;
begin
  DebugDlg := TForm.create(nil);
  with DebugDlg do begin
      BorderStyle := bsSizeable;
      Position := poScreenCenter;
      ClientWidth := 450;
      ClientHeight := 350;
      Caption := 'Firebird Installation - command-line parameters.';
  end;
  DebugText := TMemo.Create(DebugDlg);
  with DebugText do begin
      Parent := DebugDlg;
      ScrollBars := ssVertical;
      Lines.Text :=aString;
      Align := alTop;
      Height := 200;
      ReadOnly := True;
    end;
  OKButton := TButton.create(DebugDlg);
  with OKButton do begin
      Parent := DebugDlg;
      Left := (Parent.width  div 2) - width div 2;
      top := Parent.ClientHeight - 40;
      Caption := 'OK';
      ModalResult := mrOK;
  end;
  DebugDlg.ActiveControl := OKButton;

end;


procedure CloseDebugDlg;
begin
  with DebugDlg do begin
     Close;
     Free;
  end;
end;


function ShowDebugDlg(aString: String; aDescription: String): Integer;
begin
  CreateDebugDlg(aString,aDescription);
  result := DebugDlg.ShowModal;
  CloseDebugDlg;
end;


procedure CreateAdminUserPage;

begin

  AdminUserPage := CreateInputQueryPage(wpSelectTasks,
      ExpandConstant( '{cm:CreateSYSDBAPassword}' )
    , ExpandConstant( '{cm:ClickThroughPWCreation}' ) + #13#10 +
      ExpandConstant( '{cm:PasswordNote}' ) , '' );
  AdminUserPage.Add( ExpandConstant( '{cm:SYSDBAPassword}' ), True);
  AdminUserPage.Add( ExpandConstant( '{cm:RetypeSYSDBAPassword}' ), True);

  AdminUserPage.Values[0] := SYSDBAPassword;
  AdminUserPage.Values[1] := SYSDBAPassword;

end;


procedure DonateButtonOnClick(Sender: TObject);
var
  ErrorCode: Integer;
begin
  ShellExecAsOriginalUser('open', 'https://firebirdsql.org/en/donate/', '', '', SW_SHOWNORMAL, ewNoWait, ErrorCode);
end;


procedure CreateDonorPage;
var
  DonorText: AnsiString;

begin

// NOTE - The rtf must be pasted as a single line in the custom_messages.inc for the translated language. When removomg the EOLs pay attention that \par is always followed by a space.
(*
{\rtf1\ansi\ansicpg1252\deff0\nouicompat\deflang1033{\fonttbl{\f0\fnil\fcharset0 Calibri;}}{\colortbl ;\red0\green0\blue255;}{\*\generator Riched20 10.0.17763}\viewkind4\uc1\pard\sa200\sl276\slmult1\qj\f0\fs24\lang9 Firebird has empowered users worldwide with its reliable and powerful database capabilities, thanks to years of dedicated development and community collaboration.\par \pard\li568\ri568\sa200\sl240\slmult1\qj\par  \pard\sa200\sl276\slmult1\qj To ensure Firebird\rquote s continued growth and advancement, we invite you to {{\field{\*\fldinst{HYPERLINK "https://firebirdsql.org/en/donate"}}{\fldrslt{\ul\cf1 make a donation}}}}\f0\fs24   through the Firebird Foundation. Your generous contributions help fund ongoing development, maintain critical infrastructure, and expand the features you rely on. Additionally, our affordable subscriptions offer exclusive benefits designed to enhance your Firebird experience.\par \par Every contribution, big or small, plays a vital role in keeping Firebird innovative and robust for everyone.\par \par Visit our {{\field{\*\fldinst{HYPERLINK https://firebirdsql.org/en/firebird-foundation/ }}{\fldrslt{website\ul0\cf0}}}}\f0\fs24  to explore how you can contribute to this mission and help ensure Firebird\rquote s bright future. Thank you for being a valued part of the Firebird community!\par };
*)

  DonorPage :=  CreateCustomPage( wpInfoAfter, 'Support Firebird Development', '');
// For testing place it after the welcome page
//  DonorPage :=  CreateCustomPage( wpWelcome, 'Support Firebird Development', '');

  DonorText := ExpandConstant('{cm:DonorPage}');

  RichEditViewer := TRichEditViewer.Create(DonorPage);
  RichEditViewer.Width := DonorPage.SurfaceWidth;
  RichEditViewer.Height := DonorPage.SurfaceHeight;
  RichEditViewer.Anchors := [akLeft, akTop, akRight, akBottom];
  RichEditViewer.BevelKind := bkNone;
  RichEditViewer.BorderStyle := bsNone;
  RichEditViewer.Parent := DonorPage.Surface;
  RichEditViewer.ScrollBars := ssNone;
  RichEditViewer.UseRichEdit := True;
  RichEditViewer.RTFText := DonorText;
  RichEditViewer.ReadOnly := True;

  DonateButton := TNewButton.Create(WizardForm);
  DonateButton.Left := 10;
  DonateButton.Top := WizardForm.CancelButton.Top;
  // NOTE - If adding new translations be sure to check that this button is wide enough
  // for the translation of 'Donate Now'.
  DonateButton.Width := WizardForm.CancelButton.Width + 40;
  DonateButton.Height := WizardForm.CancelButton.Height;
  DonateButton.Anchors := [akLeft, akBottom];
  DonateButton.Caption := ExpandConstant('{cm:DonateNow}');
  DonateButton.OnClick := @DonateButtonOnClick;
  DonateButton.Parent := WizardForm;
  DonateButton.Visible := False;

end;


