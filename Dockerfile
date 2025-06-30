FROM debian:12-slim
LABEL org.opencontainers.image.source=https://github.com/gbdev/rgbds
ARG version=0.9.3
WORKDIR /rgbds

COPY . .

RUN apt-get update && \
    apt-get install sudo make cmake gcc build-essential -y

# Install dependencies and compile RGBDS
RUN ./.github/scripts/install_deps.sh ubuntu-22.04
RUN make -j CXXFLAGS="-O3 -flto -DNDEBUG -static" PKG_CONFIG="pkg-config --static" Q=

# Create an archive with the compiled executables and all the necessary to install it, 
# so it can be copied outside of the container and installed/used in another system
RUN tar caf rgbds-linux-x86_64.tar.xz --transform='s#.*/##' rgbasm rgblink rgbfix rgbgfx man/* .github/scripts/install.sh

# Install RGBDS on the container so all the executables will be available in the PATH
RUN cp man/* .
RUN ./.github/scripts/install.sh
