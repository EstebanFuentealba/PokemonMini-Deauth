# 🌐 Configurar GitHub Pages

Para que tu juego sea accesible en línea, necesitas habilitar GitHub Pages en tu repositorio.

## Paso 1: Subir el Código a GitHub

Si aún no lo has hecho:

```bash
cd /Users/estebanfuentealba/pm_demo

git init
git add .
git commit -m "Initial commit con emulador web"

# Crea el repositorio en GitHub (https://github.com/new)
git remote add origin https://github.com/TU_USUARIO/TU_REPO.git
git branch -M main
git push -u origin main
```

## Paso 2: Habilitar GitHub Pages

### Opción A: Desde la Interfaz Web (Más Fácil)

1. Ve a tu repositorio en GitHub
2. Clic en **"Settings"** (⚙️)
3. En el menú lateral, clic en **"Pages"**
4. En **"Build and deployment"**:
   - **Source**: Selecciona **"GitHub Actions"**
5. ¡Listo! GitHub Pages está habilitado

### Opción B: Configuración Manual

Si ves opciones diferentes:

1. En **"Settings"** → **"Pages"**
2. **Source**: "Deploy from a branch"
3. **Branch**: Selecciona `gh-pages` (se creará automáticamente)
4. **Folder**: `/ (root)`
5. Clic en **"Save"**

## Paso 3: Activar GitHub Actions

1. Ve a **"Settings"** → **"Actions"** → **"General"**
2. En **"Workflow permissions"**:
   - Selecciona **"Read and write permissions"**
   - Marca ✅ **"Allow GitHub Actions to create and approve pull requests"**
3. Clic en **"Save"**

## Paso 4: Ejecutar el Workflow

### Primera Vez

```bash
# Hacer un cambio pequeño para trigger el workflow
git commit --allow-empty -m "Trigger GitHub Actions"
git push
```

O simplemente espera a tu próximo push normal.

### Ver el Progreso

1. Ve a la pestaña **"Actions"** en tu repositorio
2. Verás dos jobs:
   - ✅ **build** - Compila el ROM
   - ✅ **deploy-pages** - Despliega a GitHub Pages
3. Espera 5-10 minutos

## Paso 5: ¡Acceder a tu Juego!

Una vez completado, tu juego estará disponible en:

```
https://TU_USUARIO.github.io/TU_REPO/
```

Por ejemplo:
```
https://estebanfuentealba.github.io/pm-hello-world/
```

## Paso 6: Actualizar README

Actualiza el README con tu URL real:

```bash
nano README.md

# Busca y reemplaza:
# TU_USUARIO → tu_usuario_real
# tu-repo → nombre-de-tu-repo
```

## Verificar que Todo Funciona

### Check 1: GitHub Actions

1. Ve a **"Actions"**
2. Deberías ver ambos jobs completados con ✅
3. Si hay errores ❌, revisa los logs

### Check 2: GitHub Pages

1. Ve a **"Settings"** → **"Pages"**
2. Deberías ver un mensaje verde:
   ```
   Your site is live at https://TU_USUARIO.github.io/TU_REPO/
   ```
3. Haz clic en el link

### Check 3: Emulador

1. Abre la URL de tu juego
2. Deberías ver el emulador Pokemon Mini
3. El juego debería cargar automáticamente
4. Usa las teclas del teclado para jugar

## Controles del Emulador

- **Flechas**: D-pad
- **Z**: Botón A
- **X**: Botón B
- **C**: Botón C
- **Enter**: Power

## Troubleshooting

### "404 - Page not found"

**Solución:**
1. Verifica que GitHub Pages esté habilitado
2. Ve a **"Settings"** → **"Pages"**
3. Asegúrate de que **Source** sea "GitHub Actions"
4. Espera unos minutos y recarga la página

### "The game.min file is not loading"

**Solución:**
1. Ve a **"Actions"** y verifica que el job **build** se completó
2. Verifica que `helloworld.min` se generó correctamente
3. Revisa los logs del job **deploy-pages**

### "GitHub Actions failed"

**Posibles causas:**

1. **Permisos insuficientes:**
   - Ve a **"Settings"** → **"Actions"** → **"General"**
   - Habilita "Read and write permissions"

2. **GitHub Pages no habilitado:**
   - Ve a **"Settings"** → **"Pages"**
   - Selecciona **"GitHub Actions"** como source

3. **Archivos faltantes:**
   - Verifica que la carpeta `web/` tenga todos los archivos
   - `index.html`, `app.js`, `libminimon.wasm`, `favicon.png`

### "Blank page" o "White screen"

**Solución:**
1. Abre la consola del navegador (F12)
2. Revisa si hay errores de CORS o carga
3. Verifica que `game.min` esté en `web/` después del build

## Comandos Útiles

### Re-desplegar

```bash
git commit --allow-empty -m "Re-deploy to GitHub Pages"
git push
```

### Ver logs de GitHub Actions

1. **"Actions"** → Selecciona el workflow
2. Clic en el job que falló
3. Expande los steps para ver detalles

### Probar localmente

```bash
cd web
python3 -m http.server 8000

# Abre http://localhost:8000 en tu navegador
```

## Estructura Final

Después del deployment, GitHub Pages servirá:

```
https://TU_USUARIO.github.io/TU_REPO/
├── index.html              ← Página principal
├── app.js                  ← Emulador
├── libminimon.wasm         ← Core del emulador
├── game.min                ← Tu ROM (copiado automáticamente)
└── favicon.png             ← Icono
```

## Actualizar el Juego

Cada vez que hagas cambios:

```bash
# 1. Edita tu código
nano src/main.c

# 2. Commit y push
git add .
git commit -m "Nuevas características"
git push

# 3. GitHub Actions automáticamente:
#    - Compila el ROM
#    - Lo copia a web/game.min
#    - Despliega a GitHub Pages

# 4. En 5-10 minutos, tu juego estará actualizado en la web
```

## Custom Domain (Opcional)

Si tienes un dominio propio:

1. Ve a **"Settings"** → **"Pages"**
2. En **"Custom domain"** ingresa tu dominio
3. Sigue las instrucciones de configuración DNS
4. ¡Tu juego estará en tu dominio!

---

**¡Tu juego de Pokemon Mini ahora está disponible en línea para todo el mundo!** 🌐🎮




