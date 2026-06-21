# Pokemon Mini - Hello World

![Build Status](https://github.com/TU_USUARIO/tu-repo/workflows/Build%20Pokemon%20Mini%20ROM/badge.svg)

Proyecto de ejemplo para la consola **Pokemon Mini** utilizando Docker y el toolchain c88-pokemini.

## 🎮 ¡Jugar Ahora!

**[▶️ Jugar en el navegador](https://TU_USUARIO.github.io/tu-repo/)**

## ✨ Características

- ✅ Pantalla con texto "HOLA MUNDO!"
- ✅ Contador interactivo
- ✅ Controles con botones A y B
- ✅ Emulador web integrado
- ✅ Compilación automática con GitHub Actions
- ✅ Releases automáticos
- ✅ Deploy automático a GitHub Pages

## 🚀 Inicio Rápido

### Opción 1: Descargar ROM Pre-compilado (Más Fácil)

1. Ve a la pestaña [Releases](../../releases)
2. Descarga `helloworld.min`
3. Abre con un emulador Pokemon Mini
4. ¡Juega!

### Opción 2: Compilar Localmente

#### Requisitos
- Docker instalado
- Git

#### Compilar

```bash
# Clonar el repositorio
git clone https://github.com/TU_USUARIO/tu-repo.git
cd tu-repo

# Dar permisos a los scripts
chmod +x *.sh

# Compilar
./build.sh

# El ROM estará en: helloworld.min
```

#### Scripts Disponibles

```bash
./build.sh      # Compilar el proyecto
./clean.sh      # Limpiar archivos de compilación
./rebuild.sh    # Limpiar y recompilar desde cero
```

## 📝 Desarrollo

### Estructura del Proyecto

```
pm_demo/
├── .github/workflows/
│   └── build.yml           # Configuración de GitHub Actions
├── src/
│   ├── main.c              # Código principal del juego
│   ├── isr.c               # Manejadores de interrupciones
│   ├── print.c             # Funciones de impresión
│   ├── font_6x8.c          # Fuente de caracteres
│   └── startup.asm         # Código de arranque
├── Dockerfile              # Entorno de compilación
├── Makefile                # Sistema de compilación
└── README.md               # Este archivo
```

### Modificar el Código

1. **Edita** `src/main.c` con tu código
2. **Compila** con `./rebuild.sh`
3. **Prueba** el ROM en un emulador

### Agregar Archivos Nuevos

Si creas nuevos archivos `.c`:

1. Agrégalos al `Makefile`:
```makefile
C_SOURCES := src/isr.c src/main.c src/print.c src/font_6x8.c src/tu_nuevo_archivo.c
```

2. Recompila:
```bash
./rebuild.sh
```

## 🔄 GitHub Actions

Este proyecto usa GitHub Actions para compilación automática:

- ✅ **Push a main/master**: Compila y sube artifact
- ✅ **Tags v\***: Crea release automático con el ROM
- ✅ **Pull Requests**: Verifica que compile

### Crear un Release

```bash
# Hacer commit de tus cambios
git add .
git commit -m "Nueva versión del juego"

# Crear tag
git tag v1.0.0

# Push con tags
git push origin main --tags
```

GitHub Actions automáticamente:
1. Compilará el ROM
2. Creará un Release
3. Subirá el archivo `.min`

## 🎯 Controles del Juego

- **Botón A**: Incrementar contador
- **Botón B**: Resetear contador a 0
- **D-pad**: (No implementado en este ejemplo)

## 🛠️ Tecnologías

- **c88-pokemini**: Toolchain para compilar ROMs de Pokemon Mini
- **Docker**: Entorno de compilación reproducible
- **GitHub Actions**: CI/CD automático
- **Makefile**: Sistema de compilación

## 📚 Recursos

- **c88-pokemini**: https://github.com/pokemon-mini/c88-pokemini
- **Pokemon Mini Wiki**: https://wiki.sublab.net/index.php/Pokemon_Mini
- **PokeMini Emulator**: https://sourceforge.net/projects/pokemini/
- **PokeMini Online**: https://asterick.github.io/pm/

## 🐛 Problemas Conocidos

### Compilación en Apple Silicon (M1/M2/M3)

La compilación local en Mac con Apple Silicon puede mostrar errores de Wine/QEMU, pero generalmente funciona. Los errores como:
- `wine client error: Bad file descriptor`
- `qemu: uncaught target signal 11`

Son normales y no afectan la generación del ROM. Si tienes problemas, usa GitHub Actions.

## 📄 Licencia

Este proyecto es un ejemplo educativo. El código es libre para usar y modificar.

## 🤝 Contribuciones

Las contribuciones son bienvenidas:

1. Fork el proyecto
2. Crea una rama (`git checkout -b feature/nueva-caracteristica`)
3. Commit tus cambios (`git commit -m 'Agregar nueva característica'`)
4. Push a la rama (`git push origin feature/nueva-caracteristica`)
5. Abre un Pull Request

## ⭐ Créditos

- **TASKING c88 Compiler**: EPSON/TASKING
- **c88-pokemini toolchain**: Comunidad Pokemon Mini
- **Ejemplo base**: Basado en ejemplos de c88-pokemini

---

**¡Disfruta programando para Pokemon Mini!** 🎮✨

