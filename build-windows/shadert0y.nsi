; shadert0y.nsi — installs Shadert0y for Kdenlive.
;   DLL         -> <Kdenlive>\lib\frei0r-1\shadert0y.dll
;   XML         -> %APPDATA%\kdenlive\effects\shadert0y.xml
;   Uninstaller -> %ProgramFiles%\Shadert0y\  (dedicated folder, keeps frei0r-1 clean)
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

Name "Shadert0y for Kdenlive"
OutFile "shadert0y-1.0.0-windows-setup.exe"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

InstallDir "$PROGRAMFILES64\Kdenlive"
InstallDirRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Kdenlive" "InstallLocation"

Var KDIR        ; the frei0r-1 folder (where the DLL lives)
Var UNINSTDIR   ; dedicated folder for the uninstaller

Function .onInit
  ; On 64-bit Windows, use the 64-bit registry view + 64-bit Program Files
  ${If} ${RunningX64}
    SetRegView 64
    StrCpy $UNINSTDIR "$PROGRAMFILES64\Shadert0y"
  ${Else}
    StrCpy $UNINSTDIR "$PROGRAMFILES\Shadert0y"
  ${EndIf}

  ; Auto-detect Kdenlive if the default path isn't valid
  ${IfNot} ${FileExists} "$INSTDIR\*.*"
    ${If} ${FileExists} "$PROGRAMFILES64\Kdenlive\*.*"
      StrCpy $INSTDIR "$PROGRAMFILES64\Kdenlive"
    ${ElseIf} ${FileExists} "$PROGRAMFILES\Kdenlive\*.*"
      StrCpy $INSTDIR "$PROGRAMFILES\Kdenlive"
    ${EndIf}
  ${EndIf}
FunctionEnd

Function un.onInit
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
FunctionEnd

; --- Pages ---
!insertmacro MUI_PAGE_WELCOME
!define MUI_DIRECTORYPAGE_TEXT_TOP "Select your Kdenlive installation folder"
!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "Kdenlive folder"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Install"
  ; --- DLL goes into Kdenlive's lib\frei0r-1 (always frei0r-1) ---
  StrCpy $KDIR "$INSTDIR\lib\frei0r-1"
  SetOutPath $KDIR
  ${If} ${RunningX64}
    File "x64\shadert0y.dll"
  ${Else}
    File "x86\shadert0y.dll"
  ${EndIf}

  ; --- XML goes into %APPDATA%\kdenlive\effects (create if missing) ---
  CreateDirectory "$APPDATA\kdenlive\effects"
  SetOutPath "$APPDATA\kdenlive\effects"
  File "shadert0y.xml"

  ; --- Uninstaller goes into its OWN dedicated folder, not frei0r-1 ---
  CreateDirectory $UNINSTDIR
  WriteUninstaller "$UNINSTDIR\uninstall-shadert0y.exe"

  ; --- Registry bookkeeping so the uninstaller can find everything ---
  WriteRegStr HKLM "Software\Shadert0y" "PluginDir" "$KDIR"
  WriteRegStr HKLM "Software\Shadert0y" "UninstDir" "$UNINSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Shadert0y" "DisplayName" "Shadert0y (Kdenlive shader plugin)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Shadert0y" "Publisher" "HyperDev"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Shadert0y" "InstallLocation" "$UNINSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Shadert0y" "UninstallString" "$\"$UNINSTDIR\uninstall-shadert0y.exe$\""
SectionEnd

Section "Uninstall"
  ; Recover where we put things
  ReadRegStr $KDIR HKLM "Software\Shadert0y" "PluginDir"
  ReadRegStr $UNINSTDIR HKLM "Software\Shadert0y" "UninstDir"
  ${If} $KDIR == ""
    StrCpy $KDIR "$INSTDIR\lib\frei0r-1"
  ${EndIf}
  ${If} $UNINSTDIR == ""
    StrCpy $UNINSTDIR "$PROGRAMFILES\Shadert0y"
  ${EndIf}

  ; Remove the DLL from frei0r-1
  Delete "$KDIR\shadert0y.dll"

  ; Remove the XML
  Delete "$APPDATA\kdenlive\effects\shadert0y.xml"

  ; Remove the uninstaller + its dedicated folder
  Delete "$UNINSTDIR\uninstall-shadert0y.exe"
  RMDir "$UNINSTDIR"

  ; Clean up registry
  DeleteRegKey HKLM "Software\Shadert0y"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Shadert0y"
SectionEnd
