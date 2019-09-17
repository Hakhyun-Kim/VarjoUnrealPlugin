@rem param1 Should use the default Unreal Engine install directory
@rem param2 Should use Varjo modified Unreal Engine directory

if "%~1"=="" (set UE4=C:\Program Files\Epic Games\UE_4.23) else set UE4=%~f1
if "%~2"=="" (set unreal_dir=D:\Harry\UnrealEngine) else set unreal_dir=%~f2

@rem Copy over Varjo plugin source files
if exist "%~dp0Engine" rd /s /q "%~dp0Engine"
xcopy /S /Y /I "%unreal_dir%/Engine/Plugins/Runtime/Varjo" "%~dp0Engine/Plugins/Runtime/Varjo"

@rem Cleanup plugin build temp folder
if exist "%~dp0Engine/Plugins/Runtime/Varjo/Intermediate" rd /s /q "%~dp0Engine/Plugins/Runtime/Varjo/Intermediate"
if exist "%~dp0Engine/Plugins/Runtime/Varjo/Binaries" rd /s /q "%~dp0Engine/Plugins/Runtime/Varjo/Binaries"

@rem Copy Build configuration to disable IncrediBuild
rem mkdir "%~dp0Engine/Saved/UnrealBuildTool"
rem copy /Y "%~dp0BuildConfiguration.xml" "%~dp0Engine/Saved/UnrealBuildTool/"

@rem Cleanup plugin staging folder
if exist "%~dp0PluginStaging_ALL\Varjo" rd /s /q "%~dp0PluginStaging_ALL\Varjo"
call "%UE4%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%~dp0Engine\Plugins\Runtime\Varjo\Varjo.uplugin" -Package="%~dp0PluginStaging_ALL\Varjo" -NoXGE
if %errorlevel% NEQ 0 echo ERROR: failed to build VarjoPlugin, error code=%errorlevel% & exit /b 1

@rem Remove Intermediate folder
if exist "%~dp0PluginStaging_ALL\Varjo\Intermediate" rd /s /q "%~dp0PluginStaging_ALL\Varjo\Intermediate"

@rem Copy latest VarjoLib files
xcopy /S /Y /I "%~dp0Engine/Plugins/Runtime/Varjo/ThirdParty" "%~dp0PluginStaging_ALL\Varjo\ThirdParty"

@rem Remove folder copied during build
@rem if exist "%~dp0Engine" rd /s /q "%~dp0Engine"


