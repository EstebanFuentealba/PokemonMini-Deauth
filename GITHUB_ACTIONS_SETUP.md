# ⚙️ Configurar GitHub Actions para Compilación Automática

Si no tienes acceso a una máquina x86_64 nativa, puedes usar **GitHub Actions** para compilar automáticamente tu ROM en la nube (gratis).

## Paso 1: Subir tu Proyecto a GitHub

```bash
cd /Users/estebanfuentealba/pm_demo

# Inicializar git (si no lo has hecho)
git init

# Agregar archivos
git add .

# Hacer commit
git commit -m "Initial Pokemon Mini project"

# Crear repositorio en GitHub y conectar
git remote add origin https://github.com/TU_USUARIO/tu-repo.git
git push -u origin main
```

## Paso 2: Crear Workflow de GitHub Actions

Crea el archivo `.github/workflows/build.yml`:

```yaml
name: Build Pokemon Mini ROM

on:
  push:
    branches: [ main, master ]
  pull_request:
    branches: [ main, master ]
  workflow_dispatch:  # Permite ejecutar manualmente

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v3
    
    - name: Set up Docker Buildx
      uses: docker/setup-buildx-action@v2
    
    - name: Build Docker image
      run: docker build --platform linux/amd64 -t pm-compiler .
    
    - name: Compile Pokemon Mini ROM
      run: docker run --rm -v $(pwd):/project pm-compiler make
    
    - name: Upload ROM artifact
      uses: actions/upload-artifact@v3
      with:
        name: pokemon-mini-rom
        path: |
          *.min
          *.sre
        if-no-files-found: warn
    
    - name: Create Release on Tag
      if: startsWith(github.ref, 'refs/tags/v')
      uses: softprops/action-gh-release@v1
      with:
        files: |
          *.min
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

## Paso 3: Crear el Archivo de Workflow

```bash
mkdir -p .github/workflows
cat > .github/workflows/build.yml <<'EOF'
# (pega el contenido YAML de arriba aquí)
EOF

git add .github/workflows/build.yml
git commit -m "Add GitHub Actions workflow"
git push
```

## Paso 4: Ver la Compilación

1. Ve a tu repositorio en GitHub
2. Clic en la pestaña **"Actions"**
3. Verás tu workflow ejecutándose
4. Cuando termine, descarga el artifact con el ROM

## Paso 5: Descargar tu ROM

1. En la página de Actions, clic en el workflow completado
2. Scroll down hasta "Artifacts"
3. Descarga "pokemon-mini-rom"
4. Descomprime y encontrarás tu `.min` file

## Funcionalidades Adicionales

### Auto-Release en Tags

Cuando crees un tag con formato `v*` (ej: `v1.0.0`), se creará automáticamente un release:

```bash
git tag v1.0.0
git push origin v1.0.0
```

### Ejecutar Manualmente

1. Ve a Actions
2. Selecciona "Build Pokemon Mini ROM"
3. Clic en "Run workflow"
4. Selecciona branch y clic en "Run"

### Badge en README

Agrega un badge a tu README.md:

```markdown
![Build Status](https://github.com/TU_USUARIO/tu-repo/workflows/Build%20Pokemon%20Mini%20ROM/badge.svg)
```

## Ventajas de GitHub Actions

- ✅ **Gratis** para repositorios públicos
- ✅ **x86_64 nativo** - sin problemas de emulación
- ✅ **Compilación automática** en cada push
- ✅ **Artifacts** guardados por 90 días
- ✅ **Releases automáticos** con tags

## Límites

- **2000 minutos/mes** para cuentas gratuitas
- Cada compilación toma **~5-10 minutos** la primera vez
- **~1-2 minutos** en compilaciones subsecuentes (cache)

## Troubleshooting

### Error: "Docker build failed"

Asegúrate de que tu `Dockerfile` esté en la raíz del repo.

### Error: "No artifacts found"

El `Makefile` probablemente falló. Revisa los logs de la Action.

### Compilación muy lenta

La primera vez toma tiempo. Las subsecuentes usan cache y son más rápidas.

## Workflow Alternativo Simplificado

Si el Dockerfile es muy pesado, puedes instalar las herramientas directamente:

```yaml
name: Build ROM (Simple)

on: [push]

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y git curl wget unzip srecord wine wine32
    
    - name: Setup Wine
      run: |
        export WINEARCH=win32
        export WINEPREFIX=$HOME/.wine
        wineboot --init
    
    - name: Install c88-pokemini
      run: |
        git clone https://github.com/pokemon-mini/c88-pokemini.git
        cd c88-pokemini
        chmod +x install.sh installers/*.sh
        ./install.sh
    
    - name: Build ROM
      run: |
        export WINEARCH=win32
        export WINEPREFIX=$HOME/.wine
        make
    
    - name: Upload ROM
      uses: actions/upload-artifact@v3
      with:
        name: rom
        path: "*.min"
```

---

**¡Con GitHub Actions puedes compilar tu ROM sin necesidad de una máquina x86_64 local!** 🚀

