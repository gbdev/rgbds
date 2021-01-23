case `echo $1 | cut -d '-' -f 1` in
	ubuntu)
		sudo apt-get -qq update
		sudo apt-get install -yq bison libpng-dev pkg-config
		;;
	macos)
		brew install bison libpng pkg-config md5sha1sum
		echo 'export PATH="/usr/local/opt/bison/bin:$PATH"' >> /Users/runner/.bash_profile
		export LDFLAGS="-L/usr/local/opt/bison/lib"
		;;
	*)
		echo "WARNING: Cannot install deps for OS '$1'"
		;;
esac

bison --version
make --version
cmake --version
