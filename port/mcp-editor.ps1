# mcp-editor.ps1 - open the Shitfire editor with Epic's Unreal MCP server up,
# so a Claude Code session in this repo can drive the editor over
# http://127.0.0.1:8000/mcp (the entry in .mcp.json at the repo root).
#
# UE 5.8's experimental "Unreal MCP" plugin (Engine/Plugins/Experimental/
# ModelContextProtocol) is enabled editor-only in Shitfire.uproject together
# with EditorToolset (actors, assets, properties) and AutomationTestToolset
# (discover + run the SHITFIRE.* suite). Loopback only, no auth, editor only,
# tools run serially on the game thread - do not expose the port.
#
# Usage:  powershell -ExecutionPolicy Bypass -File port\mcp-editor.ps1 [-Port 8000] [-Headless] [-Analytics]
#   -Headless runs with -nullrhi (no viewport) for a pure tool session.
#   -Analytics leaves the plugin's usage telemetry on (default in the engine:
#   SessionStart/SessionEnd/ToolCall events with hashed tool ids go to Epic's
#   engine analytics provider; this script turns it off unless asked).
# Then in Claude Code: /mcp shows "unreal-mcp"; list_toolsets is the entry tool.
# Stop: close the editor, or `ModelContextProtocol.StopServer` on its console.
param(
    [int]$Port = 8000,
    [switch]$Headless,
    [switch]$Analytics
)
$ErrorActionPreference = 'Stop'
$env:UE_SDKS_ROOT = 'C:\Users\conno\prj\SHITFIRE-port\port\unreal\AutoSDK-stub'
$editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$proj = Join-Path $PSScriptRoot 'unreal\Shitfire.uproject'
$cmds = "ModelContextProtocol.StartServer $Port"
if (-not $Analytics) { $cmds = "ModelContextProtocol.EnableAnalytics 0,$cmds" }
$args = @($proj, "-ExecCmds=$cmds", '-log')
if ($Headless) { $args += @('-nullrhi', '-nosplash', '-unattended') }
Write-Host "Unreal MCP: starting editor, server on http://127.0.0.1:$Port/mcp"
Start-Process -FilePath $editor -ArgumentList $args
