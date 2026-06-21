#!/bin/bash

# Script para limpiar y recompilar desde cero

echo "=== Recompilación Completa ==="
echo ""

echo "1. Limpiando..."
./clean.sh

echo ""
echo "2. Compilando..."
./build.sh

echo ""
echo "✓ Recompilación completada"

