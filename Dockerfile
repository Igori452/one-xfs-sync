FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    gdb \
    libboost-thread-dev \
    libboost-system-dev \
    curl \
    ca-certificates \
    git \
    procps \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
CMD ["sleep", "infinity"]
