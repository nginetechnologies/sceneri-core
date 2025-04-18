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

COPY ./PackagedEditor /app

EXPOSE 30159/udp

RUN chmod +x /app/bin/Linux/ProfileDevelopment/Editor
RUN chmod +x /app/bin/Linux/ProfileDevelopment/crashpad_handler
RUN chmod +x /app/bin/Linux/ProjectSystem
RUN chmod +x /app/bin/Linux/AssetCompiler

ENTRYPOINT ["./bin/Linux/ProfileDevelopment/Editor"]