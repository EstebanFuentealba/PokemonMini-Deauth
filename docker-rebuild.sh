#!/bin/bash

# Script para limpiar y reconstruir la imagen Docker desde cero
# Útil cuando hay problemas con la compilación

echo "=== Reconstrucción Completa de Docker ==="
echo ""

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}⚠️  Esto eliminará la imagen Docker existente y la reconstruirá desde cero.${NC}"
echo -e "${YELLOW}⚠️  La primera vez tomará 5-10 minutos en Mac Apple Silicon.${NC}"
echo ""
read -p "¿Continuar? (s/n): " -n 1 -r
echo

if [[ ! $REPLY =~ ^[SsYy]$ ]]; then
    echo "Cancelado."
    exit 0
fi

echo ""
echo -e "${BLUE}1. Limpiando archivos de compilación...${NC}"
rm -rf build/

echo ""
echo -e "${BLUE}2. Eliminando contenedores relacionados...${NC}"
docker ps -a | grep pm-compiler | awk '{print $1}' | xargs -r docker rm -f 2>/dev/null || true

echo ""
echo -e "${BLUE}3. Eliminando imagen Docker anterior...${NC}"
docker rmi pm-compiler 2>/dev/null || echo "No había imagen previa"

echo ""
echo -e "${BLUE}4. Limpiando cache de Docker (opcional)...${NC}"
docker builder prune -f

echo ""
echo -e "${BLUE}5. Reconstruyendo imagen Docker...${NC}"
echo -e "${YELLOW}⏱️  Esto tomará varios minutos...${NC}"
docker build --platform linux/amd64 --no-cache -t pm-compiler .

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}✓ Imagen Docker reconstruida exitosamente!${NC}"
    echo ""
    echo "Ahora puedes compilar con:"
    echo "  ./build.sh"
else
    echo ""
    echo -e "${RED}✗ Error al reconstruir la imagen Docker${NC}"
    exit 1
fi

