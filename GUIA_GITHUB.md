# 🚀 Guía Rápida de GitHub Actions

## Paso 1: Subir el Proyecto a GitHub

```bash
cd /Users/estebanfuentealba/pm_demo

# Inicializar git (si no lo has hecho)
git init

# Agregar todos los archivos
git add .

# Hacer commit
git commit -m "Initial commit: Pokemon Mini Hello World"

# Crear repositorio en GitHub y conectar
# Ve a https://github.com/new
# Crea un repositorio nuevo (público o privado)
# Luego ejecuta:

git remote add origin https://github.com/TU_USUARIO/TU_REPO.git
git branch -M main
git push -u origin main
```

## Paso 2: GitHub Actions se Ejecutará Automáticamente

Una vez que hagas push, GitHub Actions:

1. ✅ Construirá la imagen Docker
2. ✅ Compilará tu ROM
3. ✅ Subirá el ROM como **artifact**

### Ver el Progreso

1. Ve a tu repositorio en GitHub
2. Clic en la pestaña **"Actions"**
3. Verás el workflow ejecutándose
4. Cuando termine (5-10 minutos), verás una ✅

### Descargar el ROM Compilado

1. En la página de Actions
2. Clic en el workflow completado
3. Scroll down hasta "Artifacts"
4. Descarga **"pokemon-mini-rom"**
5. Descomprime y encontrarás `helloworld.min`

## Paso 3: Crear un Release

Para crear un release oficial:

```bash
# Asegúrate de que tus cambios estén commiteados
git add .
git commit -m "Versión 1.0.0 lista"

# Crear un tag con formato v*
git tag v1.0.0

# Push con el tag
git push origin main --tags
```

GitHub Actions automáticamente:
1. Compilará el ROM
2. Creará un **Release** en GitHub
3. Subirá el archivo `.min` al release
4. Agregará una descripción automática

### Ver el Release

1. Ve a tu repositorio
2. Clic en **"Releases"** (lado derecho)
3. Verás tu release **v1.0.0** con el ROM listo para descargar

## Paso 4: Actualizar README

No olvides actualizar el README con la URL correcta:

```bash
# Edita README.md
nano README.md

# Busca "TU_USUARIO/tu-repo" y reemplaza con tu usuario/repo real
# Ejemplo: estebanfuentealba/pm-hello-world
```

## Comandos Útiles

### Crear Nueva Versión

```bash
# Hacer cambios en tu código
nano src/main.c

# Compilar y probar localmente
./rebuild.sh

# Commit
git add .
git commit -m "Mejoras en el juego"

# Crear nuevo tag (incrementa el número)
git tag v1.0.1

# Push
git push origin main --tags
```

### Ver Todos los Releases

```bash
git tag -l
```

### Borrar un Tag (si te equivocaste)

```bash
# Local
git tag -d v1.0.0

# Remoto
git push origin :refs/tags/v1.0.0
```

## Triggers del Workflow

El workflow se ejecuta cuando:

- ✅ **Push a `main` o `master`**: Compila y sube artifact
- ✅ **Push de tag `v*`**: Compila y crea release
- ✅ **Pull Request**: Verifica que compile
- ✅ **Manual**: Botón "Run workflow" en GitHub Actions

## Estructura del Workflow

```yaml
name: Build Pokemon Mini ROM

on:
  push:
    branches: [ main, master, develop ]
    tags: [ 'v*' ]
  pull_request:
    branches: [ main, master ]
  workflow_dispatch:  # Permite ejecución manual

jobs:
  build:
    runs-on: ubuntu-latest  # Usa x86_64 nativo
    
    steps:
    - Checkout del código
    - Construir imagen Docker
    - Compilar ROM
    - Subir artifacts
    - Crear release (solo en tags)
```

## Badge en README

Agrega este badge al inicio de tu README:

```markdown
![Build Status](https://github.com/TU_USUARIO/TU_REPO/workflows/Build%20Pokemon%20Mini%20ROM/badge.svg)
```

Reemplaza `TU_USUARIO` y `TU_REPO` con los correctos.

## Troubleshooting

### "Docker build failed"

- Verifica que el `Dockerfile` esté en la raíz del repo
- Revisa los logs en la pestaña Actions

### "No artifacts found"

- El `Makefile` probablemente falló
- Revisa los logs de compilación
- Verifica que `helloworld.min` se genera localmente

### "Permission denied"

- Asegúrate de que los scripts tengan permisos de ejecución
- No es necesario en GitHub Actions (Ubuntu)

### El workflow no se ejecuta

- Verifica que el archivo esté en `.github/workflows/build.yml`
- Asegúrate de hacer push del workflow: `git add .github && git commit && git push`

## Ejemplo Completo

```bash
# 1. Inicializar repositorio
cd /Users/estebanfuentealba/pm_demo
git init
git add .
git commit -m "Initial commit"

# 2. Crear repositorio en GitHub
# (hazlo en la web)

# 3. Conectar y push
git remote add origin https://github.com/estebanfuentealba/pm-hello-world.git
git branch -M main
git push -u origin main

# 4. Esperar a que compile (5-10 min)
# Ir a: https://github.com/estebanfuentealba/pm-hello-world/actions

# 5. Crear primer release
git tag v1.0.0
git push origin v1.0.0

# 6. Ver el release
# Ir a: https://github.com/estebanfuentealba/pm-hello-world/releases
```

## ¡Listo!

Ahora tienes:
- ✅ Compilación automática en cada push
- ✅ Releases automáticos con tags
- ✅ ROM disponible para descargar
- ✅ CI/CD completamente configurado

---

**¡Felicidades! Tu proyecto está listo para compartir con el mundo!** 🎉




