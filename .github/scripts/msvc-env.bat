@rem From https://github.com/microsoft/vswhere/wiki/Start-Developer-Command-Prompt#using-batch
for /f "delims=" %%p in ('vswhere.exe -prerelease -latest -property installationPath') do set vspath=%%p
@rem From https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170#developer_command_file_locations
call "%vspath%\VC\Auxiliary\Build\vcvars%1.bat"
