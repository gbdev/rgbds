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

getlibrary 'https://www.zlib.net/zlib131.zip' 'zlib.zip' '72af66d44fcc14c22013b46b814d5d2514673dda3d115e64b690c1ad636e7b17' .
getlibrary 'https://github.com/glennrp/libpng/archive/refs/tags/v1.6.43.zip' 'libpng.zip'  '5e18474a26814ae479e02ca6432da32d19dc6e615551d140c954a68d63b3f192' .
getlibrary 'https://github.com/lexxmark/winflexbison/releases/download/v2.5.25/win_flex_bison-2.5.25.zip' 'winflexbison.zip'  '8d324b62be33604b2c45ad1dd34ab93d722534448f55a16ca7292de32b6ac135' install_dir

Move-Item zlib-1.3.1 zlib
Move-Item libpng-1.6.43 libpng
