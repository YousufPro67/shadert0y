; shadert0y.nsi — installs Shadert0y for Kdenlive.
; VERSION is supplied by the release workflow; keep a useful local default.
!ifndef VERSION
  !define VERSION "dev"
!endif

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

Name "Shadert0y for Kdenlive"
OutFile "shadert0y-${VERSION}-windows-setup.exe"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

InstallDir "$PROGRAMFILES64\Kdenlive"
InstallDirRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Kdenlive" "InstallLocation"

Var KDIR
Var UNINSTDIR

Function .onInit
  ${If} ${RunningX64}
    SetRegView 64
    StrCpy $UNINSTDIR "$PROGRAMFILES64\Shadert0y"
  ${Else}
    StrCpy $UNINSTDIR "$PROGRAMFILES\Shadert0y"
  ${EndIf}

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
  StrCpy $KDIR "$INSTDIR\lib\frei0r-1"
  SetOutPath $KDIR
  ${If} ${RunningX64}
    File "x64\shadert0y.dll"
  ${Else}
    File "x86\shadert0y.dll"
  ${EndIf}

  CreateDirectory "$APPDATA\kdenlive\effects"
  SetOutPath "$APPDATA\kdenlive\effects"
  File "shadert0y.xml"

  CreateDirectory $UNINSTDIR
  WriteUninstaller "$UNINSTDIR\uninstall-shadert0y.exe"
  WriteRegStr HKLM "Software\Shadert0y" "PluginDir" "$KDIR"
  WriteRegStr HKLM "Software\Shadert0y" "UninstDir" "$UNINSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Shadert0y" "DisplayName" "Shadert0y (Kdenlive shader plugin)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Shadert0y" "Publisher" "HyperDev"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Shadert0y" "InstallLocation" "$UNINSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Shadert0y" "UninstallString" "$\"$UNINSTDIR\uninstall-shadert0y.exe$\""
SectionEnd

Section "Uninstall"
  ReadRegStr $KDIR HKLM "Software\Shadert0y" "PluginDir"
  ReadRegStr $UNINSTDIR HKLM "Software\Shadert0y" "UninstDir"
  ${If} $KDIR == ""
    StrCpy $KDIR "$INSTDIR\lib\frei0r-1"
  ${EndIf}
  ${If} $UNINSTDIR == ""
    StrCpy $UNINSTDIR "$PROGRAMFILES\Shadert0y"
  ${EndIf}
  Delete "$KDIR\shadert0y.dll"
  Delete "$APPDATA\kdenlive\effects\shadert0y.xml"
  Delete "$UNINSTDIR\uninstall-shadert0y.exe"
  RMDir "$UNINSTDIR"
  DeleteRegKey HKLM "Software\Shadert0y"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Shadert0y"
SectionEnd
