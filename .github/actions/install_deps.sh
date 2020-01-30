case `echo $1 | cut -d '-' -f 1` in
	ubuntu)
		sudo apt-get -qq update
		sudo apt-get install -yq bison libpng-dev pkg-config
		;;
	macos)
		brew install libpng pkg-config md5sha1sum
		;;
	*)
		echo "WARNING: Cannot install deps for OS '$1'"
		;;
esac

yacc --version
