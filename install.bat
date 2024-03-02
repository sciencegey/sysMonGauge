@echo off
goto check_Permissions

:check_Permissions
    echo Checking admin permissions...
    
    net session >nul 2>&1
    if %errorLevel% == 0 (
        goto install
    ) else (
        echo Error: Please run as administrator!
    )
    
    pause >nul

:install
    echo Installing packages...
    pip install -r requirements.txt
    echo Running post-install scripts
    
    python Scripts/pywin32_postinstall.py -install
    echo Installing service
    python sysMonSender.py stop
    python sysMonSender.py --startup auto install
    python sysMonSender.py start