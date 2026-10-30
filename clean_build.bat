@echo off
setlocal
rem Deletes every folder named "build" found recursively under the current working directory.
for /f "delims=" %%D in ('dir /b /s /ad "%CD%\build" 2^>nul') do (
    if /i "%%~nxD"=="build" (
        if exist "%%D" (
            echo Deleting %%D
            rmdir /s /q "%%D"
        )
    )
)
echo Done.
endlocal
