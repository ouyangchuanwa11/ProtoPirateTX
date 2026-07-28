# ProtoPirateTX Full Upload via GitHub API
$ErrorActionPreference = "Stop"

$TOKEN = "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"
$OWNER = "ouyangchuanwa11"
$REPO = "ProtoPirateTX"
$BASE = "F:\U-Claw-Data\.openclaw\workspace\ProtoPirateTX"

$headers = @{
    "Authorization" = "Bearer $TOKEN"
    "Accept" = "application/vnd.github+json"
    "X-GitHub-Api-Version" = "2022-11-28"
}

# Files to upload with their repo paths
$files = @(
    @{Local = "$BASE\application.fam"; Remote = "application.fam"},
    @{Local = "$BASE\protopirate_rb.h"; Remote = "protopirate_rb.h"},
    @{Local = "$BASE\protopirate_rb.c"; Remote = "protopirate_rb.c"},
    @{Local = "$BASE\protopirate_decoder.c"; Remote = "protopirate_decoder.c"},
    @{Local = "$BASE\protopirate_tx.c"; Remote = "protopirate_tx.c"},
    @{Local = "$BASE\protopirate_rollback.c"; Remote = "protopirate_rollback.c"},
    @{Local = "$BASE\.github\workflows\build.yml"; Remote = ".github/workflows/build.yml"}
)

foreach ($file in $files) {
    $localPath = $file.Local
    $remotePath = $file.Remote
    
    if (-not (Test-Path $localPath)) {
        Write-Output "SKIP (not found): $localPath"
        continue
    }
    
    $content = [System.IO.File]::ReadAllBytes($localPath)
    $encoded = [Convert]::ToBase64String($content)
    
    # Get current SHA
    $url = "https://api.github.com/repos/$OWNER/$REPO/contents/$remotePath"
    try {
        $existing = Invoke-RestMethod -Uri $url -Headers $headers -Method Get
        $sha = $existing.sha
        Write-Output "UPDATING: $remotePath (SHA: $($sha.Substring(0,7)))"
    } catch {
        $sha = $null
        Write-Output "CREATING: $remotePath"
    }
    
    $body = @{
        message = "v3.1 - clean rebuild all files"
        content = $encoded
        branch = "main"
    }
    if ($sha) { $body.sha = $sha }
    
    $jsonBody = $body | ConvertTo-Json -Compress
    
    try {
        $response = Invoke-RestMethod -Uri $url -Headers $headers -Method Put -Body $jsonBody -ContentType "application/json"
        Write-Output "  OK: $($response.content.name)"
    } catch {
        Write-Output "  ERROR: $_"
    }
}

Write-Output "`nDONE. Check Actions: https://github.com/$OWNER/$REPO/actions"
