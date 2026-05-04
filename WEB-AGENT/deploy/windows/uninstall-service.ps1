param(
    [string]$ServiceName = "web-agent"
)

& sc.exe stop $ServiceName
& sc.exe delete $ServiceName
exit $LASTEXITCODE
