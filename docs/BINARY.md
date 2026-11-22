# SotC para Mac con Apple Silicon

Descomprime el ZIP y abre **Jugar.command**. Selecciona la carpeta de tu copia descifrada de CUSA08809, actualización 1.01, que contiene `eboot.bin`. Si el lanzador lo pide, selecciona también la carpeta de tus módulos originales `libSceSystemGesture.sprx` y `libSceNgs2.sprx`. La preparación local requiere Python 3. El paquete es para Apple Silicon con macOS 15 o posterior y lleva firma ad hoc.

Puedes pasar las carpetas directamente desde Terminal.

```sh
./Jugar.command "/ruta/a/CUSA08809" "/ruta/a/sys_modules"
```

Con una imagen enlazada previamente para tu copia, usa la siguiente variante. Puedes comprobar que el ejecutable carga la biblioteca gráfica con `./Jugar.command --check`.

```sh
SOTC_LINKED_BOOT="/ruta/a/boot-linked.bin" ./Jugar.command "/ruta/a/CUSA08809"
```

El lanzador imprime la ruta de los registros y, con Python 3, recoge los tiempos de fotograma y la carga de GPU disponible. Los polígonos y los bloqueos durante los giros de cámara siguen pendientes de validación en partida.

El código del proyecto, instrucciones de compilación y versiones de los componentes están en el repositorio. Las licencias y los créditos se encuentran en la carpeta `licenses`. [Código fuente](https://github.com/braito4/sotc-mac)
