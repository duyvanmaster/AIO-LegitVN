@echo off
echo Starting update process...
timeout /t 2 /nobreak >nul
if exist "C:\Users\domin\OneDrive\Máy tính\New folder\update_temp.exe" (
    echo Temporary update file exists.
    move /y "C:\Users\domin\OneDrive\Máy tính\New folder\update_temp.exe" "C:\Users\domin\OneDrive\Máy tính\New folder\AIOLegitVN.exe"
    if errorlevel 1 (
        echo Update failed. Please try again.
        pause
        exit /b 1
    ) else (
        echo Update successful.
        start "" "C:\Users\domin\OneDrive\Máy tính\New folder\AIOLegitVN.exe"
    )
) else (
    echo Temporary update file not found.
    pause
    exit /b 1
)
del "C:\Users\domin\OneDrive\Máy tính\New folder\update.bat"
