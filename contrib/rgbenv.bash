#!/usr/bin/env bash

# SPDX-License-Identifier: MIT
#
# Copyright (c) 2022 Zumi Daxuya
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

set -e
shopt -u nullglob

# --------------- Variables -------------------------------------------------

RGBDS_GIT="https://github.com/gbdev/rgbds"

# The version number is directly appended after these variables
RGBDS_DOWNLOAD_BASE="https://api.github.com/repos/gbdev/rgbds/tarball/v"
RGBDS_PREFIX="rgbds-"

# --------------- Var defines -------------------------------------------------

# Set up XDG standard directories (Linux, etc.)
RGBENV_DEFAULT=${XDG_DATA_HOME:-~/.local/share}/rgbenv/default
RGBENV_VERSIONS=${XDG_DATA_HOME:-~/.local/share}/rgbenv/versions

if [[ ! -d ${XDG_DATA_HOME:-~/.local/share} ]]; then
	RGBENV_DEFAULT=~/.rgbenv/default
	RGBENV_VERSIONS=~/.rgbenv/versions
fi

RGBDS_PREFIX_LEN=${#RGBDS_PREFIX}

# available offline
if [ -d $RGBENV_VERSIONS ]; then
	AVAILABLE_VERSIONS=$(find $RGBENV_VERSIONS -maxdepth 1 -type d -regex ".*$RGBDS_PREFIX[0-9]+.[0-9]+.[0-9]+\(-[a-zA-Z0-9_]+\)?" -printf "%f\n")
else
	AVAILABLE_VERSIONS=''
fi

# --------------- Helpers ----------------------------------------------------

check_online_versions () {
	# set the relevant variable
	ONLINE_VERSIONS=$(git ls-remote --tags $RGBDS_GIT | grep -Po "\d+\.\d+\.\d+(-[A-Za-z0-9_]+)?$")
}

help () {
	echo "rgbenv v0.1.0 - manage RGBDS versions"
	echo
	echo "Usage: rgbenv [use|exec|install|available] [options] [args]"
	echo
	echo "    use [version]?"
	echo "        Set or view the RGBDS version for"
	echo "        the currently-running shell."
	echo
	echo "    no-use"
	echo "        Clear the RGBDS version and let the"
	echo "        system PATH manage it."
	echo
	echo "    exec [-v/--version version]? [program, arg1, arg2, ...]"
	echo "        Run commands using a specific version"
	echo "        of RGBDS. If the file '.rgbds-version' is"
	echo "        present in the current directory, it will use"
	echo "        the version specified within, unless the -v"
	echo "        option is specified to override it."
	echo
	echo "    install [version]"
	echo "        Install a specific RGBDS version."
	echo
	echo "    uninstall [version]"
	echo "        Uninstall a RGBDS version."
	echo
	echo "    available [--online]"
	echo "        List installed RGBDS versions."
	echo
	echo "        Use --online to list versions available"
	echo "        for installation."
	echo
	echo "    remove"
	echo "        Removes the rgbenv directories."
}

checkver () {
	if [ -z $1 ]; then
		echo "No version number set!"
		point_to_available_command
		exit 1
	elif [[ ! $1 =~ [[:digit:]]+\.[[:digit:]]+\.[[:digit:]]+(-[A-Za-z0-9_]+)? ]]; then
		echo "Invalid version number."
		point_to_available_command
		exit 1
	fi
}

checkverinstall () {
	local version=$1
	local has_choice=0
	checkver $version;
	if [ -d $RGBENV_VERSIONS/$RGBDS_PREFIX$version ]; then
		while [ $has_choice -eq 0 ]; do
			read -p "Version $version is already installed. Do you want to reinstall? [y/n] " choice
			case $choice in
				[Yy]* ) has_choice=1; break;;
				[Nn]* ) exit; break;;
				* ) echo "Please answer y or n.";;
			esac
		done
	else
		echo "Version $version isn't installed yet."
	fi
}

_remove () {
	local has_choice=0
	while [ $has_choice -eq 0 ]; do
		read -p "Remove rgbenv directories? [y/n] " choice
		case $choice in
			[Yy]* ) rm -rv $RGBENV_DEFAULT; rm -rv $RGBENV_VERSIONS; has_choice=1; break;;
			[Nn]* ) has_choice=1; exit;;
			* ) echo "Please answer y or n.";;
		esac
	done
}

setup () {
	local has_choice=0
	echo "rgbenv has not been set up yet."
	echo "The following operations will be done:"
	echo "    Create rgbenv defaults dir     -> $RGBENV_DEFAULT"
	echo "    Create source and binaries dir -> $RGBENV_VERSIONS"
	echo
	while [ $has_choice -eq 0 ]; do
		read -p "Would you like to do that now? [y/n] " choice
		case $choice in
			[Yy]* ) make_dirs; has_choice=1; break;;
			[Nn]* ) has_choice=1; exit;;
			* ) echo "Please answer y or n.";;
		esac
	done
}

make_dirs () {
	mkdir -p $RGBENV_DEFAULT
	mkdir -p $RGBENV_VERSIONS
}

show_install_instructions () {
	echo
	echo "You can try to install this version by:"
	echo "    rgbenv install $1"
}
point_to_available_command () {
		echo
		echo "To see available versions:"
		echo "    rgbenv available"
}

point_to_available_online () {
		echo
		echo "Check for installable versions by:"
		echo "    rgbenv available --online"
}

point_to_use () {
		echo
		echo "Select a version to use with:"
		echo "    rgbenv use <version>"
}

# --------------- Subcommands -----------------------------------------

_available () {
	if [ "$1" == "--online" ]; then
		check_online_versions
		echo "Installable versions:"
		for i in $ONLINE_VERSIONS; do
			echo "    $i"
		done
	
	else
	
		local have_versions=$AVAILABLE_VERSIONS
		
		if [ -z "$have_versions" ]; then
			echo "There are no versions currently installed."
			echo "Look at the available versions using:"
			echo "    rgbenv available --online"
			echo
			echo "Then, from that list, install the version you"
			echo "want using:"
			echo "    rgbenv install <version>"
		else
			echo "Currently installed versions:"
			for i in $have_versions; do
			# Versions prefixed with "rgbds-", cut them off
				i=$(echo "$i" | cut -c$(( $RGBDS_PREFIX_LEN + 1 ))-)
				echo "    $i"
			done
		fi
		
	fi
}

_use () {
	if [ -z $1 ]; then
		if [ -f $RGBENV_DEFAULT/version ]; then
			cat $RGBENV_DEFAULT/version
		else
			echo "Use which version?"
			point_to_available_command
		fi
	else
		checkver $1;
		# check if we have a working build of RGBASM (at the very least)
		if [ -x $RGBENV_VERSIONS/$RGBDS_PREFIX$1/rgbasm ]; then
			# create symlinks to the proper RGBDS version
			mkdir -p $RGBENV_DEFAULT/bin
			
			echo "Making symlinks..."
			for i in rgbasm rgblink rgbfix rgbgfx; do
				ln -sfv $RGBENV_VERSIONS/$RGBDS_PREFIX$1/$i $RGBENV_DEFAULT/bin/$i
			done
			echo
			
			echo $1 > $RGBENV_DEFAULT/version
			
			echo "The default RGBDS has been set to $1."
			if [[ ":$PATH:" != *":$RGBENV_DEFAULT/bin:"* ]]; then
				echo "To use it, ensure that the following is in your shell configuration:"
				echo
				echo "export PATH=\"$RGBENV_DEFAULT/bin:\$PATH\""
				echo
				echo "Once that's done, verify with rgbasm -V."
			fi
		else
			echo "Version $1 has not been installed yet."
			show_install_instructions $1
			point_to_available_command
		fi
	fi
}

_no_use () {
	if [ -f $RGBENV_DEFAULT/version ]; then
		rm $RGBENV_DEFAULT/version
	fi
	if [ -d $RGBENV_DEFAULT/bin ]; then
		rm -r $RGBENV_DEFAULT/bin
	fi
	echo "Symlinks and version configuration deleted."
	echo "RGBDS is now managed by the system."
	echo
	echo "You can set a version again by passing:"
	echo "    rgbenv use <version>"
	
}

_install () {
	local version=$1
	local got_version=0
	local download_link="$RGBDS_DOWNLOAD_BASE$version"
	if [ -z $version ]; then
		echo "Which version to install?"
		point_to_available_online
		exit 1
	fi
	checkverinstall $version
	
	check_online_versions
	local available=$ONLINE_VERSIONS
	
	# check if the version we want is in the git version tags list
	for i in $available; do
		if [ $version = $i ]; then
			got_version=1
			continue
		fi
	done
	if [ $got_version -eq 0 ]; then
		echo "Version $version not found in the releases page."
		point_to_available_online
		exit 1
	fi
	
	# if so, download the tarball and extract it
	echo "Downloading from $download_link..."
	tempfile=$(mktemp)
	curl -L $download_link > $tempfile
	dirname=$(tar -tzf $tempfile | head -1 | cut -f1 -d'/')
	tar -xzf $tempfile --directory $RGBENV_VERSIONS
	mv $RGBENV_VERSIONS/$dirname $RGBENV_VERSIONS/$RGBDS_PREFIX$version
	rm $tempfile
	
	# then build it
	echo "Building RGBDS $version..."
	cd $RGBENV_VERSIONS/$RGBDS_PREFIX$version
	if make; then
		echo "Build successful."
		echo
		echo "You can now use this version by:"
		echo "    rgbenv use $version"
	else
		echo "One or more build components failed! You may still use this"
		echo "version, just with missing tools."
		exit 1
	fi
}

_uninstall () {
	local version=$1
	if [ -z $version ]; then
		echo "Which version to uninstall?"
		point_to_available_command
	else
		checkver $version
		if [ -d $RGBENV_VERSIONS/$RGBDS_PREFIX$version ]; then
			rm -r $RGBENV_VERSIONS/$RGBDS_PREFIX$version
			
			if [ -d $RGBENV_DEFAULT/version ]; then
				if [ $(cat $RGBENV_DEFAULT/version) = $version ]; then
					rm $RGBENV_DEFAULT/version
					rm $RGBENV_DEFAULT/bin/*
					echo "Removed current RGBDS version, RGBDS is now system-managed"
				fi
			fi
			echo "Version $1 uninstalled."
			
		fi
	fi
}

_exec () {
	if [ "$1" == "--version" ] || [ "$1" == "-v" ] ; then
		shift
		local version=$1
		checkver $version
		if [ -x $RGBENV_VERSIONS/$RGBDS_PREFIX$version/rgbasm ]; then
			shift
			if [ ${#@} -gt 0 ]; then
				echo "Executing with RGBDS version $version."
				env PATH="$RGBENV_VERSIONS/$RGBDS_PREFIX$version:$PATH" $@
			fi
		else
			echo "Version $version is not installed."
			show_install_instructions $version
			point_to_available_command
			exit 1
		fi
	elif [ -f .rgbds-version ]; then
		local version=$(cat .rgbds-version | tr -d " \t\n\r")
		checkver $version
		if [ -x $RGBENV_VERSIONS/$RGBDS_PREFIX$version/rgbasm ]; then
			if [ ${#@} -gt 0 ]; then
				echo "Executing with RGBDS version $version."
				env PATH="$RGBENV_VERSIONS/$RGBDS_PREFIX$version:$PATH" $@
			fi
		else
			echo "Version $version is not installed."
			show_install_instructions $version
			point_to_available_command
			exit 1
		fi
	else
		# when PATH is set properly, this would pretty much be useless
		# This *does* have a use if it isn't though
		if [ -f $RGBENV_DEFAULT/version ]; then
			version=$(cat $RGBENV_DEFAULT/version)
			if [ ! -x $RGBENV_VERSIONS/$RGBDS_PREFIX$version/rgbasm ]; then
				echo "Version not set."
				point_to_use
				exit 1
			else
				if [ ${#@} -gt 0 ]; then
					env PATH="$RGBENV_VERSIONS/$RGBDS_PREFIX$version:$PATH" $@
				fi
			fi
		else
			echo "Version not set."
			point_to_use
			exit 1
		fi
	fi
}

# --------------- Entry point -------------------------------------------------

# Perform setup
if [ ! -d $RGBENV_DEFAULT ]; then
	setup
fi

# Main program
case $1 in
	use* ) shift; _use $1;;
	no-use* ) shift; _no_use $1;;
	install* ) shift; _install $1;;
	uninstall* ) shift; _uninstall $1;;
	exec* ) shift; _exec $@;;
	available* ) shift; _available $1;;
	remove*) shift; _remove;;
	*) if [ ! -z "$1" ]; then echo -e "Unknown command $1.\n"; fi; help;;
esac
