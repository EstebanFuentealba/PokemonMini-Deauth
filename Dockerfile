FROM --platform=linux/amd64 ubuntu:20.04

# Evitar prompts interactivos durante la instalación
ENV DEBIAN_FRONTEND=noninteractive

# Habilitar arquitectura i386 para Wine (CAPA 1 - Casi nunca cambia)
RUN dpkg --add-architecture i386

# Instalar dependencias del sistema (CAPA 2 - Rara vez cambia)
# Usar cache mount para acelerar apt
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y \
    git \
    curl \
    wget \
    unzip \
    srecord \
    wine \
    wine32 \
    wine64 \
    make \
    && rm -rf /var/lib/apt/lists/*

# Crear directorio y clonar c88-pokemini (CAPA 3 - Puede cambiar ocasionalmente)
WORKDIR /opt
RUN git clone --depth 1 https://github.com/pokemon-mini/c88-pokemini.git

# Configurar Wine (CAPA 4 - Depende de c88-pokemini)
ENV WINEARCH=win32
ENV WINEPREFIX=/opt/c88-pokemini/wineprefix
RUN wineboot --init && while pgrep wineserver > /dev/null; do sleep 1; done

# Instalar toolchain (CAPA 5 - Depende de Wine)
WORKDIR /opt/c88-pokemini
RUN chmod +x install.sh installers/*.sh && bash install.sh

# Configurar PATH
ENV PATH="/opt/c88-pokemini/c88tools/bin:${PATH}"

# Directorio de trabajo para el proyecto (CAPA 6 - Siempre cambia)
WORKDIR /project

# Comando por defecto
CMD ["make"]
