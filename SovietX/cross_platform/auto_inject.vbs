Option Explicit

Dim shell, fso, scriptDir, injector
Set fso = CreateObject("Scripting.FileSystemObject")
scriptDir = fso.GetParentFolderName(WScript.ScriptFullName)
injector = scriptDir & "\SovietInjector.exe"

If fso.FileExists(injector) Then
    Set shell = CreateObject("WScript.Shell")
    shell.Run Chr(34) & injector & Chr(34) & " --watch", 0, False
End If
