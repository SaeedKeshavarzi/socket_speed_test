IF EXIST ".\.vs" (
    rmdir ".\.vs" /s /q
)

IF EXIST ".\bin" (
    rmdir ".\bin" /s /q
)

IF EXIST ".\obj" (
    rmdir ".\obj" /s /q
)

IF EXIST ".\ipch" (
    rmdir ".\ipch" /s /q
)

IF EXIST ".\x64" (
    rmdir ".\x64" /s /q
)

del *.db

IF EXIST ".\release" (
    rmdir ".\release" /s /q
)

IF EXIST ".\debug" (
    rmdir ".\debug" /s /q
)

IF EXIST ".\Makefile" (
    del ".\Makefile" /s /q
)

del *.Debug

del *.Release

del *.stash

del *.user

IF EXIST ".\socket_speed_test\release" (
    rmdir ".\socket_speed_test\release" /s /q
)

IF EXIST ".\socket_speed_test\debug" (
    rmdir ".\socket_speed_test\debug" /s /q
)

del socket_speed_test\*.Release

del socket_speed_test\*.Debug
