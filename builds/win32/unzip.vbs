src = Wscript.Arguments(0)	' source zip archive name
dst = Wscript.Arguments(1)	' destination folder name

set objShell = CreateObject("Shell.Application")
set srcFiles = objShell.NameSpace(src)
set dstFolder = objShell.NameSpace(dst)

if (srcFiles is nothing) then
  set fso = CreateObject("Scripting.FileSystemObject")
  fso.GetStandardStream(2).WriteLine "Wrong source file name: " & src
  Wscript.Quit
end if


if (dstFolder is nothing) then
  set fso = CreateObject("Scripting.FileSystemObject")
  call fso.CreateFolder(dst)
  set dstFolder = objShell.NameSpace(dst)
end if

' Options
' 4
' Do not display a progress dialog box.
' 16
' Respond with "Yes to All" for any dialog box that is displayed.

call dstFolder.CopyHere(srcFiles.items, 16+4)

set srcFiles = Nothing
Set objShell = Nothing
