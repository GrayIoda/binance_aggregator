FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    make \
    cmake \
    ninja-build \
    g++ \
    gdb \
    gdbserver \
    rsync \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --break-system-packages "conan>=2.0"

RUN conan profile detect --force

WORKDIR /app

CMD ["/bin/bash"]
