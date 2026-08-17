#!/usr/bin/env python3
import sys
import urllib.parse
import os

MAX_LINEAS = 32
ARCHIVO_CARRITO = "tienda/carrito/carrito.txt"

try:
    datos_crudos = sys.stdin.read()
except EOFError:
    datos_crudos = ""

params = urllib.parse.parse_qs(datos_crudos)
nuevo_producto = params.get("producto", [None])[0]

if not nuevo_producto:
    print("Error: Parámetro 'producto' ausente.")
    sys.exit(0)

productos_en_carrito = []
if os.path.exists(ARCHIVO_CARRITO):
    with open(ARCHIVO_CARRITO, "r", encoding="utf-8") as f:
        productos_en_carrito = [line.strip() for line in f if line.strip()]

encontrado = False
carrito_actualizado = []

for item in productos_en_carrito:
    if item == nuevo_producto or item.startswith(nuevo_producto + " x"):
        if not encontrado:
            if " x" in item:
                nombre, cantidad = item.rsplit(" x", 1)
                nueva_cantidad = int(cantidad) + 1
                carrito_actualizado.append(f"{nombre} x{nueva_cantidad}")
            else:
                carrito_actualizado.append(f"{item} x2")
            encontrado = True
        else:
            carrito_actualizado.append(item)
    else:
        carrito_actualizado.append(item)

if not encontrado:
    carrito_actualizado.append(nuevo_producto)

if len(carrito_actualizado) > MAX_LINEAS:
    print(f"Error: Límite de {MAX_LINEAS} productos diferentes alcanzado.")
else:
    try:
        with open(ARCHIVO_CARRITO, "w", encoding="utf-8") as f:
            for item in carrito_actualizado:
                f.write(item + "\n")
        
        print(f"Carrito actualizado: {nuevo_producto}")
    except Exception as e:
        print(f"Error crítico de E/S: {e}")
