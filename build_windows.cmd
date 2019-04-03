echo on

SET project_dir="%cd%"

echo Set up environment...
set PATH=%QT%\bin\;C:\Qt\Tools\QtCreator\bin\;C:\Qt\QtIFW2.0.1\bin\;%PATH%
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %PLATFORM%

echo Building CommerciumQT...
qmake -spec win32-msvc2015 CONFIG+=x86_64 CONFIG-=debug CONFIG+=release
nmake

echo Running tests...

echo Packaging...
cd %project_dir%\build\windows\msvc\x86_64\release\
windeployqt --qmldir ..\..\..\..\..\src\ CommerciumQT\CommerciumQT.exe

rd /s /q CommerciumQT\moc\
rd /s /q CommerciumQT\obj\
rd /s /q CommerciumQT\qrc\

echo Copying project files for archival...
copy "%project_dir%\README.md" "CommerciumQT\README.md"
copy "%project_dir%\LICENSE" "CommerciumQT\LICENSE.txt"
copy "%project_dir%\Qt License" "CommerciumQT\Qt License.txt"

echo Copying files for installer...
mkdir "%project_dir%\installer\windows\x86_64\packages\com.CommerciumQTproject.CommerciumQT\data\"
robocopy CommerciumQT\ "%project_dir%\installer\windows\x86_64\packages\com.CommerciumQTproject.CommerciumQT\data" /E

echo Packaging portable archive...
7z a CommerciumQT_%TAG_NAME%_windows_x86_64_portable.zip CommerciumQT

echo Creating installer...
cd %project_dir%\installer\windows\x86_64\
binarycreator.exe --offline-only -c config\config.xml -p packages CommerciumQT_%TAG_NAME%_windows_x86_64_installer.exe
