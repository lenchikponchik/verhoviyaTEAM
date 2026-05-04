param(
    [string]$ServiceName = "web-agent",
    [string]$AgentPath = "C:\web-agent\bin\web-agent.exe",
    [string]$ConfigPath = "C:\web-agent\config\agent_config.json"
)

$resolvedAgent = (Resolve-Path -LiteralPath $AgentPath).Path
$resolvedConfig = (Resolve-Path -LiteralPath $ConfigPath).Path
$binPath = "`"$resolvedAgent`" --config `"$resolvedConfig`""

& sc.exe create $ServiceName binPath= $binPath start= auto DisplayName= "WEB-AGENT"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& sc.exe description $ServiceName "WEB-AGENT background worker"
& sc.exe start $ServiceName
exit $LASTEXITCODE
