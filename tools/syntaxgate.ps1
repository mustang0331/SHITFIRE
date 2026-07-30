# Syntax gate for SHITFIRE.html.
#
# There is no node/deno/bun on this machine and JScript cannot parse ES2015+, so
# the only real JS parser available is the browser that ships the product.
#
# HOW IT WORKS, and why the obvious version does not:
#   `new Function(src)` was the first attempt. It reports PARSE-OK on source with
#   an unbalanced paren, because it compiles the body LAZILY - the function is
#   created and never called, so nothing is ever parsed. Measured against a
#   deliberately broken copy; it is a gate that cannot fail, which is worse than
#   no gate.
#   So instead the extracted source is wrapped in `if (false) { ... }` and loaded
#   as a real classic <script>. A syntax error is an EARLY error: the spec
#   requires it to be thrown before any statement runs, so Chrome reports it to
#   window.onerror at compile time. And because the only statement is a dead
#   branch, a clean parse executes nothing at all - no WebGL, no CDN fetch, no
#   DOM dependency, no side effects.
#
# Exit 0 = parsed clean. Exit 1 = SyntaxError (message and line printed). 2 = gate broke.
# Lives in tools/ per CLAUDE.md's dev-tooling policy: this tool only ever READS
# SHITFIRE.html. Run it on every code row (CLAUDE.md, Testing).
#   powershell -File tools\syntaxgate.ps1
param(
  [string]$Src = (Join-Path (Split-Path $PSScriptRoot -Parent) "SHITFIRE.html"),
  [string]$Work = (Join-Path $env:TEMP "shitfire-syntaxgate")
)
$chrome = "C:\Program Files\Google\Chrome\Application\chrome.exe"
if (-not (Test-Path $chrome)) { Write-Output "NO CHROME"; exit 2 }
if (-not (Test-Path $Work)) { New-Item -ItemType Directory -Force $Work | Out-Null }

$lines = Get-Content -LiteralPath $Src
$open = -1; $close = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
  if ($open -lt 0 -and $lines[$i] -match '<script\s+type="module"') { $open = $i; continue }
  if ($open -ge 0 -and $lines[$i] -match '^\s*</script>\s*$') { $close = $i; break }
}
if ($open -lt 0 -or $close -lt 0) { Write-Output "COULD NOT FIND MODULE SCRIPT"; exit 2 }
$firstJsLine = $open + 2      # 1-based line number in the HTML of the first JS line
Write-Output ("module script: HTML lines {0}-{1} ({2} lines of JS)" -f $firstJsLine, $close, ($close - $open - 1))

# Strip import statements - illegal outside a module. Each becomes a blank line so
# the reported line numbers still map onto the extracted file.
$stripped = 0
$out = New-Object System.Collections.Generic.List[string]
$out.Add('if (false) {')      # line 1 of the wrapper; JS line N is file line N+1
foreach ($l in $lines[($open + 1)..($close - 1)]) {
  if ($l -match '^\s*import\s' -and $l -match ';') { $out.Add(''); $stripped++ }
  else { $out.Add($l) }
}
$out.Add('}')
Write-Output ("stripped {0} import statement(s)" -f $stripped)

$jsPath = Join-Path $Work "body.js"
Set-Content -LiteralPath $jsPath -Value $out -Encoding utf8

$probe = @'
<!doctype html><meta charset="utf-8"><title>PENDING</title><body>
<script>
window.__err = null;
window.onerror = function (m, u, l, c) { window.__err = m + '  (probe line ' + l + ')'; return true; };
</script>
<script src="body.js"></script>
<script>
document.title = window.__err ? ('PARSE-FAIL: ' + window.__err) : 'PARSE-OK';
document.write('<pre>' + document.title + '</pre>');
</script>
'@
$probePath = Join-Path $Work "probe.html"
Set-Content -LiteralPath $probePath -Value $probe -Encoding utf8

# Fresh profile per run: a stale SingletonLock from an earlier headless run makes
# Chrome exit silently with an empty DOM, which reads as a gate failure and is not one.
$prof = Join-Path $Work ("prof_" + [Guid]::NewGuid().ToString("N").Substring(0, 8))
# Capture through a FILE. Assigning a native exe's output to a variable inside a
# script yielded an empty string every time here, while the identical pipeline
# typed at the prompt worked - so keep the object pipeline out of the path.
$domPath = Join-Path $Work "dom.txt"
$url = "file:///" + ($probePath -replace '\\', '/')
& $chrome --headless=new --disable-gpu --no-first-run --no-default-browser-check --allow-file-access-from-files --user-data-dir="$prof" --virtual-time-budget=15000 --dump-dom $url | Out-File -LiteralPath $domPath -Encoding utf8
$text = if (Test-Path $domPath) { Get-Content -LiteralPath $domPath -Raw } else { "" }
if ($null -eq $text) { $text = "" }

# Read the verdict out of <title> ONLY. Searching the whole DOM matches the
# probe's own inline script source, which necessarily contains the literal
# 'PARSE-FAIL: ' - that false positive made a clean file report as broken.
$verdict = ""
$tm = [regex]::Match($text, '(?s)<title>(.*?)</title>')
if ($tm.Success) { $verdict = $tm.Groups[1].Value.Trim() }
Write-Output "verdict: $verdict"
if ($verdict -match '^PARSE-FAIL:(.*)$') {
  $msg = $Matches[1].Trim()
  Write-Output "PARSE-FAIL: $msg"
  # translate the probe line number back to a line in the real HTML file
  $pl = [regex]::Match($msg, 'probe line (\d+)')
  if ($pl.Success) {
    $htmlLine = [int]$pl.Groups[1].Value - 1 + $firstJsLine - 1
    Write-Output ("  -> {0} line {1}" -f (Split-Path $Src -Leaf), $htmlLine)
  }
  exit 1
}
if ($verdict -eq 'PARSE-OK') { Write-Output "PARSE-OK - the module parses clean"; exit 0 }
Write-Output "GATE INCONCLUSIVE - no verdict in the DOM:"
Write-Output ($text.Substring(0, [Math]::Min(600, $text.Length)))
exit 2
