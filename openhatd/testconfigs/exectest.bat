@echo off

echo Starting exectest...

set exitcode=%1

:loop
if "%1" == "" goto end
echo exectest.bat parameter: %1
shift
goto loop

:end

sleep 2

echo Terminating exectest...
echo A message that goes to stderr... >&2

exit %exitcode%
