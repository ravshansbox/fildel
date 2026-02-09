FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    build-essential \
    clang \
    libncurses-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN make clean && make

CMD ["./fildel"]
