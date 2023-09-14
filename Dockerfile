FROM debian:11-slim
LABEL org.opencontainers.image.source=https://github.com/gbdev/rgbds
ARG version=0.6.1
WORKDIR /rgbds

COPY . .

RUN apt-get update && \
    apt-get install sudo make cmake gcc build-essential -y

RUN ./.github/scripts/install_deps.sh ubuntu-20.04
RUN make -j WARNFLAGS="-Wall -Wextra -pedantic  -static" PKG_CONFIG="pkg-config --static" Q=

RUN tar caf rgbds-${version}-linux-x86_64.tar.xz --transform='s#.*/##' rgbasm rgblink rgbfix rgbgfx man/* .github/scripts/install.sh
