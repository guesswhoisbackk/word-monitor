param(
    [string]$Message = ""
)

$ErrorActionPreference = "Stop"
# Local clone of https://github.com/guesswhoisbackk/wordbook (created 2026-09-17).
$repo = "C:\antigravity\wordbook"

if (-not (Test-Path (Join-Path $repo ".git"))) {
    throw "Wordbook repo not found: $repo"
}

Push-Location $repo
try {
    git add -A
    if (-not $Message) {
        $Message = "wordbook update $(Get-Date -Format 'yyyy-MM-dd HH:mm')"
    }
    # Nothing staged means nothing changed; skip the empty commit.
    git diff --cached --quiet
    if ($LASTEXITCODE -ne 0) {
        git commit -m $Message
        if ($LASTEXITCODE -ne 0) { throw "commit failed" }
        git push
        if ($LASTEXITCODE -ne 0) { throw "push failed" }
        Write-Host "Pushed. Device picks it up on next sync (boot or every 12h; jsDelivr cache may add up to a few hours)."
    } else {
        Write-Host "No changes to push."
    }
} finally {
    Pop-Location
}
