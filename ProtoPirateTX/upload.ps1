$ErrorActionPreference = "Continue"
$TOKEN = "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"
$OWNER = "ouyangchuanwa11"
$REPO = "ProtoPirateTX"

$headers = @{
    "Authorization" = "Bearer $TOKEN"
    "Accept" = "application/vnd.github+json"
}

$filePath = "application.fam"
$localFile = "F:\U-Claw-Data\.openclaw\workspace\ProtoPirateTX\application.fam"
$contentBytes = [System.IO.File]::ReadAllBytes($localFile)
$encoded = [Convert]::ToBase64String($contentBytes)

$url = "https://api.github.com/repos/$OWNER/$REPO/contents/$filePath"

# Get SHA
try {
    $existing = Invoke-RestMethod -Uri $url -Headers $headers -Method Get
    $sha = $existing.sha
    Write-Host "SHA: $sha"
} catch {
    $sha = $null
    Write-Host "NEW FILE"
}

$body = @{
    message = "v3.1 rebuild all"
    content = $encoded
    branch = "main"
}
if ($sha) { $body.sha = $sha }

$jsonBody = $body | ConvertTo-Json -Compress
Write-Host "Uploading..."
$response = Invoke-RestMethod -Uri $url -Headers $headers -Method Put -Body $jsonBody -ContentType "application/json"
Write-Host "OK: $($response.content.name)"
