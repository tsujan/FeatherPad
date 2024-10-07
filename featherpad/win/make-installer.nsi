; example2.nsi
;
; This script is based on example1.nsi, but it remember the directory,
; has uninstall support and (optionally) installs start menu shortcuts.
;
; It will install example2.nsi into a directory that the user selects.
;
; See install-shared.nsi for a more robust way of checking for administrator rights.
; See install-per-user.nsi for a file association example.
;--------------------------------
;Include Modern UI
;--------------------------------

!include "MUI2.nsh"

!include "FileFunc.nsh"   ; for ${GetSize} for EstimatedSize registry entry

!define UNINST_LIST uninstall_list.nsh

;--------------------------------
;General
;--------------------------------

; The name of the installer
Name "$%APP_NAME%"

; The file to write
OutFile "dist\$%APP_NAME%-x64-setup.exe"

; Build Unicode installer
Unicode True

;Default installation folder
InstallDir $LOCALAPPDATA\Programs\$%APP_NAME%

;Get installation folder from registry if available
InstallDirRegKey HKCU "Software\$%APP_NAME%" "Install_Dir"

;Request application privileges for Windows Vista
RequestExecutionLevel user

Setcompressor LZMA

;--------------------------------
;Interface Settings
;--------------------------------

!define MUI_ABORTWARNING

;--------------------------------
;Pages
;--------------------------------

;!insertmacro MUI_PAGE_LICENSE "${NSISDIR}\Docs\Modern UI\License.txt"

;Page components
!insertmacro MUI_PAGE_COMPONENTS

;Page directory
!insertmacro MUI_PAGE_DIRECTORY

;Page instfiles
!insertmacro MUI_PAGE_INSTFILES

;UninstPage uninstConfirm
!insertmacro MUI_UNPAGE_CONFIRM

;UninstPage instfiles
!insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
;Languages
;--------------------------------

!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; The stuff to install
;--------------------------------
Section "$%APP_NAME% (required)"

  SectionIn RO

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  ; Put file there
  File /r "dist\$%APP_NAME%\*.*"

  ; Write the installation path into the registry
  WriteRegStr HKCU SOFTWARE\$%APP_NAME% "Install_Dir" "$INSTDIR"

  ; Write the uninstall keys for Windows
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$%APP_NAME%" "DisplayName" "$%APP_NAME%"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$%APP_NAME%" "DisplayIcon" "$INSTDIR\$%APP_NAME%.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$%APP_NAME%" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$%APP_NAME%" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$%APP_NAME%" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$%APP_NAME%" "EstimatedSize" "$0"

SectionEnd

;--------------------------------
; Optional section (can be disabled by the user)
;--------------------------------
Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\$%APP_NAME%"
  CreateShortcut "$SMPROGRAMS\$%APP_NAME%\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$SMPROGRAMS\$%APP_NAME%\$%APP_NAME%.lnk" "$INSTDIR\$%APP_NAME%.exe"

SectionEnd

;--------------------------------
; Uninstaller Section
;--------------------------------
Section "Uninstall"

  ; Remove registry keys
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$%APP_NAME%"
  DeleteRegKey HKCU SOFTWARE\$%APP_NAME%

  ; Remove files and uninstaller
  ;RMDir /r "$INSTDIR"
  !include ${UNINST_LIST}
  Delete $INSTDIR\uninstall.exe
  RMDir "$INSTDIR"

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\$%APP_NAME%\*.lnk"

  ; Remove directories
  RMDir "$SMPROGRAMS\$%APP_NAME%"

SectionEnd
