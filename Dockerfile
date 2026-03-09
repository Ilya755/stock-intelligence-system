FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV CC=gcc-14
ENV CXX=g++-14    

RUN apt-get update && apt-get install -y \
    build-essential \
    g++-14 \
    cmake \
    ninja-build \
    git \
    libpq-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    libboost-all-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt .

RUN mkdir src && echo "int main() { return 0; }" > src/main.cpp

RUN mkdir build && cd build \
    && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. \
    && ninja stock_lib || true 

COPY src/ ./src/

RUN cd build && cmake .. && ninja

FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y \
    libpq5 \
    libcurl4 \
    libssl3 \
    libboost-system1.83.0 \
    libboost-thread1.83.0 \
    ca-certificates \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -s /bin/bash stockuser

WORKDIR /app
RUN mkdir -p /app/configs /app/logs && chown -R stockuser:stockuser /app

USER stockuser

COPY --from=builder --chown=stockuser:stockuser /app/build/stock_app ./server

COPY --chown=stockuser:stockuser configs/app_config.json ./configs/app_config.json

ENV APP_CONFIG_PATH="/app/configs/app_config.json"

EXPOSE 8080

CMD ["./server"]