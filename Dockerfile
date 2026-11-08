# Dockerfile — Compila el Pedagogical Compressor en un contenedor Ubuntu
# sin instalar nada en tu sistema host.
#
# Uso:
#   docker build -t pedagogical-comp-builder .
#   docker run --rm -v "$(pwd)/out:/out" pedagogical-comp-builder
#
# Los plugins terminan en ./out/{VST3,LV2,CLAP}/

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV BUILD_TYPE=Release

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake git build-essential pkg-config ca-certificates \
    libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev \
    libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
    libglu1-mesa-dev mesa-common-dev \
    libasound2-dev \
    webkit2gtk-4.0-dev libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copia solo el código (cacheable si no cambia)
COPY CMakeLists.txt   /src/
COPY Source           /src/Source/
COPY README.md        /src/

# Build
RUN cmake -B build -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    && cmake --build build -j"$(nproc)"

# Carpeta donde se copiarán los binarios al host
RUN mkdir -p /out

CMD cp -r build/PedagogicalCompressor_artefacts/${BUILD_TYPE}/VST3       /out/VST3 && \
    cp -r build/PedagogicalCompressor_artefacts/${BUILD_TYPE}/LV2        /out/LV2  && \
    cp -r build/PedagogicalCompressor_artefacts/${BUILD_TYPE}/CLAP       /out/CLAP && \
    cp -r build/PedagogicalCompressor_artefacts/${BUILD_TYPE}/Standalone /out/Standalone && \
    echo "✔ Listos en /out" && \
    ls -laR /out
