function getlibrary ([string] $URI, [string] $filename, [string] $hash, [string] $destdir) {
	$wc = New-Object Net.WebClient
	[string] $downloadhash = $null
	try {
		$wc.DownloadFile($URI, $filename)
		$downloadhash = $(Get-FileHash $filename -Algorithm SHA256).Hash
	} catch {
		Write-Host "${filename}: failed to download"
		exit 1
	}
	if ($hash -ne $downloadhash) {
		Write-Host "${filename}: SHA256 mismatch ($downloadhash)"
		exit 1
	}
	Expand-Archive -DestinationPath $destdir $filename
}

getlibrary 'https://www.zlib.net/zlib132.zip' 'zlib.zip' 'e8bf55f3017aa181690990cb58a994e77885da140609fc8f94abe9b65d2cae28' .
getlibrary 'https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.53.zip' 'libpng.zip' '9fb99118ec4523d9a9dab652ce7c2472ec76f6ccd69d1aba3ab873bb8cf84b98' .
getlibrary 'https://github.com/lexxmark/winflexbison/releases/download/v2.5.25/win_flex_bison-2.5.25.zip' 'winflexbison.zip' '8d324b62be33604b2c45ad1dd34ab93d722534448f55a16ca7292de32b6ac135' install_dir

Move-Item zlib-1.3.2 zlib
Move-Item libpng-1.6.53 libpng
