; Script to package Bioimages Collection Manager

; HM NIS Edit Wizard helper defines
!define APPLICATION_NAME "Bioimages Collection Manager"
!define APPLICATION_VERSION "1.2.0"
!define APPLICATION_PUBLISHER "Ken Polzin"
!define APPLICATION_WEB_SITE "http://bioimages.vanderbilt.edu"
!define APPLICATION_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\${APPLICATION_EXECUTABLE}"
!define APPLICATION_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION_NAME}"
!define APPLICATION_UNINST_ROOT_KEY "HKLM"
!define APPLICATION_EXECUTABLE "${APPLICATION_NAME}.exe"
;!define OPTION_LICENSE_AGREEMENT

SetCompressor /FINAL /SOLID lzma
SetCompressorDictSize 64

!include nsDialogs.nsh ;Used by APPDATA uninstaller.

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "installer.ico"
!define MUI_UNICON "installer.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "page_header.bmp"
!define MUI_TEXT_WELCOME_INFO_TEXT "Setup will guide you through the installation of Bioimages Collection Manager.$\n$\nIt is recommended that you close any instances of Bioimages Collection Manager before running this installer.$\n$\nClick Next to continue.$\n"
;!define MUI_COMPONENTSPAGE_SMALLDESC

; Welcome page

!insertmacro MUI_PAGE_WELCOME
; License page
!ifdef OPTION_LICENSE_AGREEMENT
   !insertmacro MUI_PAGE_LICENSE "license.txt"
!endif

; Components page
;!insertmacro MUI_PAGE_COMPONENTS
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APPLICATION_EXECUTABLE}"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages

UninstPage custom un.UnPageUserAppData un.UnPageUserAppDataLeave
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------

Name "${APPLICATION_NAME}"
BrandingText "${APPLICATION_NAME} ${APPLICATION_VERSION}"
OutFile "bioimages-collection-manager-${APPLICATION_VERSION}-setup.exe"
InstallDir "$PROGRAMFILES\${APPLICATION_NAME}"
InstallDirRegKey HKLM "${APPLICATION_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show
RequestExecutionLevel highest

##############################################################################
#                                                                            #
#   RE-INSTALLER FUNCTIONS                                                   #
#                                                                            #
##############################################################################

#Function .onInit

#  ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION_NAME}" "UninstallString"
#  StrCmp $R0 "" done

#  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "${APPLICATION_NAME} is already installed. $\n$\nClick `OK` to remove the previous version or `Cancel` to cancel this upgrade." IDOK uninst
#  Abort

  ;Run the uninstaller
#  uninst:
#    ClearErrors
#    Exec $INSTDIR\uninst.exe

#  done:
#FunctionEnd

Section "MainSection" SEC01
  SetOutPath "$INSTDIR"
  SetOverwrite try
  CreateDirectory "$SMPROGRAMS\${APPLICATION_NAME}"
  CreateShortCut "$SMPROGRAMS\${APPLICATION_NAME}\${APPLICATION_NAME}.lnk" "$INSTDIR\${APPLICATION_EXECUTABLE}"
  CreateShortCut "$DESKTOP\${APPLICATION_NAME}.lnk" "$INSTDIR\${APPLICATION_EXECUTABLE}"
  File "${APPLICATION_NAME}\${APPLICATION_EXECUTABLE}"
  File "${APPLICATION_NAME}\icudt54.dll"
  File "${APPLICATION_NAME}\icuin54.dll"
  File "${APPLICATION_NAME}\icuuc54.dll"
  File "${APPLICATION_NAME}\libeay32.dll"
  File "${APPLICATION_NAME}\libgcc_s_dw2-1.dll"
  File "${APPLICATION_NAME}\libssl32.dll"
  File "${APPLICATION_NAME}\libstdc++-6.dll"
  File "${APPLICATION_NAME}\libwinpthread-1.dll"
  File "${APPLICATION_NAME}\msvcr120.dll"
  File "${APPLICATION_NAME}\Qt5Core.dll"
  File "${APPLICATION_NAME}\Qt5Gui.dll"
  File "${APPLICATION_NAME}\Qt5Network.dll"
  File "${APPLICATION_NAME}\Qt5Sql.dll"
  File "${APPLICATION_NAME}\Qt5Widgets.dll"
  File "${APPLICATION_NAME}\Qt5Xml.dll"
  File "${APPLICATION_NAME}\ssleay32.dll"
  SetOutPath "$INSTDIR\exiftool"
  File "${APPLICATION_NAME}\exiftool\exiftool.exe"
  SetOutPath "$INSTDIR\imageformats"
  File "${APPLICATION_NAME}\imageformats\qjpeg.dll"
  SetOutPath "$INSTDIR\platforms"
  File "${APPLICATION_NAME}\platforms\qwindows.dll"
  SetOutPath "$INSTDIR\sqldrivers"
  File "${APPLICATION_NAME}\sqldrivers\qsqlite.dll"
SectionEnd

Section "Optional" SEC02
  SetOutPath "$APPDATA\${APPLICATION_NAME}\data"
  SetOverwrite try
  File "${APPLICATION_NAME}\data\bioimages.db"
SectionEnd

Section -AdditionalIcons
  SetOutPath $INSTDIR
  WriteIniStr "$INSTDIR\Bioimages website.url" "InternetShortcut" "URL" "${APPLICATION_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\${APPLICATION_NAME}\Bioimages website.lnk" "$INSTDIR\Bioimages website.url"
  CreateShortCut "$SMPROGRAMS\${APPLICATION_NAME}\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${APPLICATION_DIR_REGKEY}" "" "$INSTDIR\${APPLICATION_EXECUTABLE}"
  WriteRegStr ${APPLICATION_UNINST_ROOT_KEY} "${APPLICATION_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${APPLICATION_UNINST_ROOT_KEY} "${APPLICATION_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${APPLICATION_UNINST_ROOT_KEY} "${APPLICATION_UNINST_KEY}" "DisplayIcon" "$INSTDIR\${APPLICATION_EXECUTABLE}"
  WriteRegStr ${APPLICATION_UNINST_ROOT_KEY} "${APPLICATION_UNINST_KEY}" "DisplayVersion" "${APPLICATION_VERSION}"
  WriteRegStr ${APPLICATION_UNINST_ROOT_KEY} "${APPLICATION_UNINST_KEY}" "URLInfoAbout" "${APPLICATION_WEB_SITE}"
  WriteRegStr ${APPLICATION_UNINST_ROOT_KEY} "${APPLICATION_UNINST_KEY}" "Publisher" "${APPLICATION_PUBLISHER}"
SectionEnd

; Section descriptions
;!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
;  !insertmacro MUI_DESCRIPTION_TEXT ${SEC01} "Installs the main application"
;  !insertmacro MUI_DESCRIPTION_TEXT ${SEC02} "Installs the latest metadata"
;!insertmacro MUI_FUNCTION_DESCRIPTION_END

#################################
# Uninstaller                   #
#################################
Var UnPageUserAppDataDialog
Var UnPageUserAppDataCheckbox
Var UnPageUserAppDataCheckbox_State
Var UnPageUserAppDataEditBox

Function un.UnPageUserAppData
   !insertmacro MUI_HEADER_TEXT "Uninstall ${APPLICATION_NAME}" "Choose the uninstall option to perform."
   nsDialogs::Create /NOUNLOAD 1018
   Pop $UnPageUserAppDataDialog

   ${If} $UnPageUserAppDataDialog == error
      Abort
   ${EndIf}

   ${NSD_CreateLabel} 0 0 100% 24u "Do you want to delete ${APPLICATION_NAME}'s data folder? This will delete ALL changes you have made to records locally."
   Pop $0

   ${NSD_CreateText} 0 25u 100% 12u "$APPDATA\${APPLICATION_NAME}"
   Pop $UnPageUserAppDataEditBox
   SendMessage $UnPageUserAppDataEditBox ${EM_SETREADONLY} 1 0

   ${NSD_CreateLabel} 0 46u 100% 24u "Leave unchecked to keep the data folder for later use or check to delete the data folder."
   Pop $0

   ${NSD_CreateCheckbox} 0 71u 100% 8u "Yes, delete the data folder."
   Pop $UnPageUserAppDataCheckbox

   nsDialogs::Show
FunctionEnd

Function un.UnPageUserAppDataLeave
   ${NSD_GetState} $UnPageUserAppDataCheckbox $UnPageUserAppDataCheckbox_State
FunctionEnd

Function un.onUninstSuccess
  HideWindow
  IfSilent +2
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

;Function un.onInit
;  IfSilent +3
;  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
;  Abort
;FunctionEnd

Section Uninstall
  Delete "$INSTDIR\Bioimages website.url"
  Delete "$INSTDIR\uninst.exe"
  
  ;Uninstall User Data if option is checked, otherwise skip.
  ${If} $UnPageUserAppDataCheckbox_State == ${BST_CHECKED}
    Delete "$APPDATA\${APPLICATION_NAME}\data\local-bioimages.db"
  ${EndIf}
  
  Delete "$APPDATA\${APPLICATION_NAME}\data\bioimages.db"
  Delete "$APPDATA\${APPLICATION_NAME}\data\thumbnails.dat"
  Delete "$APPDATA\${APPLICATION_NAME}\data\CSVs\agents.csv"
  Delete "$APPDATA\${APPLICATION_NAME}\data\CSVs\determinations.csv"
  Delete "$APPDATA\${APPLICATION_NAME}\data\CSVs\images.csv"
  Delete "$APPDATA\${APPLICATION_NAME}\data\CSVs\last-downloaded.xml"
  Delete "$APPDATA\${APPLICATION_NAME}\data\CSVs\names.csv"
  Delete "$APPDATA\${APPLICATION_NAME}\data\CSVs\organisms.csv"
  Delete "$APPDATA\${APPLICATION_NAME}\data\CSVs\sensu.csv"
  Delete "$INSTDIR\${APPLICATION_EXECUTABLE}"
  Delete "$INSTDIR\icudt54.dll"
  Delete "$INSTDIR\icuin54.dll"
  Delete "$INSTDIR\icuuc54.dll"
  Delete "$INSTDIR\libeay32.dll"
  Delete "$INSTDIR\libgcc_s_dw2-1.dll"
  Delete "$INSTDIR\libssl32.dll"
  Delete "$INSTDIR\libstdc++-6.dll"
  Delete "$INSTDIR\libwinpthread-1.dll"
  Delete "$INSTDIR\msvcr120.dll"
  Delete "$INSTDIR\Qt5Core.dll"
  Delete "$INSTDIR\Qt5Gui.dll"
  Delete "$INSTDIR\Qt5Network.dll"
  Delete "$INSTDIR\Qt5Sql.dll"
  Delete "$INSTDIR\Qt5Widgets.dll"
  Delete "$INSTDIR\Qt5Xml.dll"
  Delete "$INSTDIR\ssleay32.dll"
  Delete "$INSTDIR\exiftool\exiftool.exe"
  Delete "$INSTDIR\imageformats\qjpeg.dll"
  Delete "$INSTDIR\platforms\qwindows.dll"
  Delete "$INSTDIR\sqldrivers\qsqlite.dll"

  Delete "$SMPROGRAMS\${APPLICATION_NAME}\Uninstall.lnk"
  Delete "$SMPROGRAMS\${APPLICATION_NAME}\Bioimages website.lnk"
  Delete "$DESKTOP\${APPLICATION_NAME}.lnk"
  Delete "$SMPROGRAMS\${APPLICATION_NAME}\${APPLICATION_NAME}.lnk"
  
  ;
  ; Also make sure to remove /data/CSVs folder and contents
  ;

  RMDir "$SMPROGRAMS\${APPLICATION_NAME}"
  RMDir "$INSTDIR\sqldrivers"
  RMDir "$INSTDIR\platforms"
  RMDir "$INSTDIR\imageformats"
  RMDir "$INSTDIR\exiftool"
  RMDir "$INSTDIR"
  RMDir "$APPDATA\${APPLICATION_NAME}\data\CSVs"
  RMDir "$APPDATA\${APPLICATION_NAME}\data"
  RmDir "$APPDATA\${APPLICATION_NAME}"

  DeleteRegKey ${APPLICATION_UNINST_ROOT_KEY} "${APPLICATION_UNINST_KEY}"
  DeleteRegKey HKLM "${APPLICATION_DIR_REGKEY}"
  SetAutoClose true
SectionEnd