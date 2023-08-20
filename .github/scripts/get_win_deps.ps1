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

getlibrary 'https://www.zlib.net/zlib13.zip' 'zlib.zip' 'c561d09347f674f0d72692e7c75d9898919326c532aab7f8c07bb43b07efeb38' .
getlibrary 'https://download.sourceforge.net/libpng/lpng1637.zip' 'libpng.zip'  '3b4b1cbd0bae6822f749d39b1ccadd6297f05e2b85a83dd2ce6ecd7d09eabdf2' .
getlibrary 'https://github.com/lexxmark/winflexbison/releases/download/v2.5.24/win_flex_bison-2.5.24.zip' 'winflexbison.zip'  '39c6086ce211d5415500acc5ed2d8939861ca1696aee48909c7f6daf5122b505' install_dir

Move-Item zlib-1.2.13 zlib
Move-Item lpng1637 libpng
