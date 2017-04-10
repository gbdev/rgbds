#!/bin/sh

if [ $TRAVIS_OS_NAME = "osx" ]; then
    brew update
    brew install libpng pkg-config
else # linux
    sudo apt-get -qq update
    sudo apt-get install -y -q bison flex libpng-dev pkg-config
fi

echo "Dependencies:"
yacc --version
flex --version
