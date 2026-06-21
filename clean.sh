#!/bin/bash

# Script para limpiar el proyecto

echo "Limpiando archivos de compilación..."
docker run --rm -v "$(pwd):/project" pm-compiler make clean
echo "✓ Limpieza completada"

