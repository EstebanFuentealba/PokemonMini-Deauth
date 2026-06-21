# ⚡ GitHub Actions Sin Docker

## Cambio Realizado

El workflow de GitHub Actions ahora **NO usa Docker** porque:

### ❌ Antes (Con Docker)

```yaml
steps:
  - Build Docker image (5-7 min)
  - Compile in Docker (2-3 min)
Total: 7-10 minutos
```

### ✅ Ahora (Sin Docker)

```yaml
steps:
  - Install dependencies (1-2 min)
  - Setup Wine (30 seg)
  - Install c88-pokemini con CACHE (30 seg - 2 min primera vez)
  - Compile (1-2 min)
Total: 3-5 minutos
```

## Ventajas

1. **⚡ Más Rápido**
   - Sin construcción de imagen Docker
   - Con cache de c88-pokemini
   - Primera compilación: ~5 min
   - Siguientes compilaciones: ~3 min

2. **💾 Usa Cache**
   - c88-pokemini se cachea entre builds
   - No necesita re-clonar ni re-instalar

3. **🔧 Más Simple**
   - Menos pasos
   - Más fácil de debugear
   - Logs más claros

4. **🌐 Mismo Resultado**
   - x86_64 nativo en ambos casos
   - Mismo toolchain
   - Mismo ROM generado

## ¿Cuándo Usar Docker?

### Usa Docker (Dockerfile) para:
- ✅ Desarrollo local en Mac Apple Silicon
- ✅ Desarrollo local en cualquier plataforma
- ✅ Entorno consistente local

### GitHub Actions usa instalación directa:
- ✅ Ya es x86_64 nativo (ubuntu-latest)
- ✅ No necesita emulación
- ✅ Más rápido sin Docker

## Makefile Inteligente

El `Makefile` ahora detecta automáticamente:

```makefile
# Detectar si estamos en GitHub Actions o Docker
ifeq ($(GITHUB_ACTIONS),true)
    PM_BASE := $(HOME)/c88-pokemini
else
    PM_BASE := /opt/c88-pokemini
endif
```

- **En GitHub Actions**: Usa `~/c88-pokemini`
- **En Docker local**: Usa `/opt/c88-pokemini`

## Compilación Local

### Sigue usando Docker localmente:

```bash
# Local (Mac, Windows, Linux)
./build.sh              # Usa Docker

# GitHub Actions (automático)
git push                # Usa instalación nativa
```

## Cache en GitHub Actions

El workflow cachea el toolchain:

```yaml
- name: Cache c88-pokemini
  uses: actions/cache@v3
  with:
    path: ~/c88-pokemini
    key: c88-pokemini-${{ hashFiles('**/Makefile') }}
```

**Resultado:**
- Primera vez: Clona e instala c88-pokemini (~2 min)
- Siguientes veces: Usa cache (~30 seg)

## Comparación de Tiempos

### Primera Compilación

| Método | Tiempo |
|--------|--------|
| Docker (antes) | 10-12 min |
| Sin Docker (ahora) | 5-7 min |

### Compilaciones Subsecuentes (con cache)

| Método | Tiempo |
|--------|--------|
| Docker (antes) | 3-5 min |
| Sin Docker (ahora) | 2-3 min |

## Estructura Actual

```
Desarrollo Local
├── Usa Dockerfile
├── Construye imagen (una vez)
└── Compila rápido

GitHub Actions
├── ubuntu-latest (x86_64)
├── Instala dependencias
├── Usa cache de c88-pokemini
└── Compila rápido
```

## ¿El Dockerfile Sigue Siendo Útil?

**¡SÍ!** El Dockerfile es esencial para:

1. **Desarrollo local en Mac Apple Silicon**
   - Proporciona emulación x86_64
   - Entorno consistente

2. **Desarrollo local en cualquier plataforma**
   - Windows, Linux, Mac Intel
   - Mismo entorno para todos

3. **Testing local**
   - Probar antes de hacer push
   - Mismo resultado que GitHub Actions

## Resumen

- ✅ **Dockerfile**: Para desarrollo local
- ✅ **GitHub Actions**: Instalación nativa (más rápido)
- ✅ **Makefile**: Detecta automáticamente el entorno
- ✅ **Cache**: Acelera compilaciones en CI

---

**¡Ahora tu proyecto compila más rápido en GitHub Actions!** ⚡




