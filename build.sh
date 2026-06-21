#!/bin/bash

# Script para compilar el proyecto Pokemon Mini usando Docker

# Colores para output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Compilador Pokemon Mini con Docker ===${NC}"

# Verificar si Docker está instalado
if ! command -v docker &> /dev/null; then
    echo -e "${RED}Error: Docker no está instalado${NC}"
    exit 1
fi

# Construir la imagen Docker si no existe
if [[ "$(docker images -q pm-compiler 2> /dev/null)" == "" ]]; then
    echo -e "${BLUE}Construyendo imagen Docker (esto puede tomar varios minutos la primera vez)...${NC}"
    docker build --platform linux/amd64 -t pm-compiler .
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error al construir la imagen Docker${NC}"
        exit 1
    fi
fi

# Ejecutar la compilación
echo -e "${GREEN}Compilando proyecto...${NC}"
docker run --rm -v "$(pwd):/project" -e WINEARCH=win32 -e WINEPREFIX=/opt/wineprefix pm-compiler make

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Compilación exitosa!${NC}"
    echo -e "${GREEN}El archivo .min está en: build/helloworld.min${NC}"
else
    echo -e "${RED}✗ Error en la compilación${NC}"
    exit 1
fi

cp helloworld.min web/game.min

