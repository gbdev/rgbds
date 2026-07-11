FROM debian:13-slim
LABEL org.opencontainers.image.source=https://github.com/gbdev/rgbds
ARG version=1.0.1
WORKDIR /rgbds

COPY . .

# Install dependencies
RUN apt-get update && \
    apt-get install sudo make cmake gcc build-essential -y
RUN ./.github/scripts/install-deps.sh debian

# Build RGBDS
RUN make -j "$(getconf _NPROCESSORS_ONLN)" CXXFLAGS="-O3 -flto -DNDEBUG -static" PKG_CONFIG="pkg-config --static" Q=

# Create the install script
RUN make install.sh Q=

# Create an archive with the compiled executables, man pages, and install script,
# so it can be copied outside of the container and installed/used in another system
RUN tar caf rgbds-linux-x86_64.tar.xz --transform='s#.*/##' rgbasm rgblink rgbfix rgbgfx man/* install.sh

# Install RGBDS on the container so all the executables will be available in the PATH
RUN cp man/* .
RUN make install Q=
