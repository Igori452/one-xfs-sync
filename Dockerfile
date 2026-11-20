FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    gdb \
    liburing-dev \
    libacl1-dev \
    xfsprogs \
    lvm2 \
    fio \
    curl \
    ca-certificates \
    git \
    procps \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
CMD ["sleep", "infinity"]