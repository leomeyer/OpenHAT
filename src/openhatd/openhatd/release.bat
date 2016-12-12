@echo off
rem Windows release packaging

if "%OPENHATD_TAG%" == "" set OPENHATD_TAG=current

set /p "OPENHATD_VERSION=" < VERSION

set POCOLIBPATH=..\..\..\opdi_core\code\c\libraries\POCO\bin
set ZIPFOLDER=openhatd-windows-%OPENHATD_VERSION%-%OPENHATD_TAG%

@echo Preparing zip folder...
mkdir %ZIPFOLDER%
mkdir %ZIPFOLDER%\bin
del %ZIPFOLDER%.zip

@echo Copying main binary...
copy openhatd.exe %ZIPFOLDER%\bin
copy hello-world.ini %ZIPFOLDER%\bin

@echo Preparing plugin folder...
mkdir %ZIPFOLDER%\plugins
xcopy /s ..\plugins\*.dll %ZIPFOLDER%\plugins
mkdir %ZIPFOLDER%\plugins\WebServerPlugin\webdocroot
xcopy /s ..\plugins\WebServerPlugin\webdocroot %ZIPFOLDER%\plugins\WebServerPlugin\webdocroot

@echo Copying testconfigs...
mkdir %ZIPFOLDER%\testconfigs
xcopy /s ..\testconfigs %ZIPFOLDER%\testconfigs

@echo Preparing documentation...
pushd ..
copy mkdocs.yml mkdocs.yml.orig
powershell -Command "(gc mkdocs.yml) -replace '__VERSION__', '%OPENHATD_VERSION%' | Out-File mkdocs.yml"
mkdocs build
copy mkdocs.yml.orig mkdocs.yml
del mkdocs.yml.orig
popd

@echo Preparing documentation folder...
mkdir %ZIPFOLDER%\doc
@echo "*.vsdx" > doc_exclude
xcopy /s /exclude:doc_exclude ..\openhatd-docs-%OPENHATD_VERSION%\* %ZIPFOLDER%\doc\
del doc_exclude

@echo Making zip...
7z a %ZIPFOLDER%.zip %ZIPFOLDER%
rmdir /s /q %ZIPFOLDER%
@echo zip complete.
