# Dot-source to activate ESP-IDF 5.5.3 in PowerShell when `python` is not on PATH
# (export.ps1 shells out to `python`). Usage: . tools/idf_env.ps1
param([string]$IdfPath = "$env:USERPROFILE\esp\esp-idf-v5.5.3")
$py = Get-ChildItem "$env:USERPROFILE\.espressif\python_env" -Recurse -Filter python.exe -Depth 3 |
    Where-Object { $_.FullName -match 'idf5\.5' } | Select-Object -First 1 -ExpandProperty FullName
if (-not $py) { throw "ESP-IDF 5.5 python env not found under ~/.espressif/python_env" }
$script = & $py "$IdfPath\tools\activate.py" --export 2>$null | Select-Object -Last 1
if (-not (Test-Path $script)) { throw "ESP-IDF activation failed" }
. $script
