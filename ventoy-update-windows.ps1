# Get the releases from GitHub
$releases = Invoke-RestMethod -Uri "https://api.github.com/repos/ventoy/Ventoy/releases"

# Iterate through the releases
forEach ($release in $releases) {
    # Exclude pre-releases
    if (!$release.prerelease) {
        # Get the version of the release
        $version = $release.tag_name
        # Iterate through the release assets
        forEach ($asset in $release.assets) {
            # Include only Windows assets
            if ($asset.name -like "*-windows.zip") {
                # Download only if file does not exist on disk
                if (Test-Path $asset.name) {
                    Write-Host "You already have the latest version of Ventoy."
                    break
                }
                else {
                    Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $asset.name
                    Write-Host "Downloaded Ventoy for Windows $version."
                    break
                }
            }
        }
        break
    }
}
