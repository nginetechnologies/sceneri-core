FROM registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest

RUN apt-get update && apt-get install -y \
    curl \
    libvulkan1 \
    mesa-vulkan-drivers \
    libopenal1 \
    libtiff5 \
    libassimp5 \
    zlib1g \
    libfreetype6 \
    libsdl2-2.0-0 \
    software-properties-common \
    && apt-get install -y --only-upgrade \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY ./PackagedProjectSystem /app

RUN chmod +x /app/ProjectSystem

ENTRYPOINT ["./ProjectSystem"]