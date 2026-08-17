#!/usr/bin/env python3
import os

if os.path.exists("tienda/carrito/carrito.txt"):
    with open("tienda/carrito/carrito.txt", "r") as f:
        carrito = f.read()
    if len(carrito) == 0:
        print("Tu carrito esta vacio. Agrega productos !!")
    else:
        print(carrito)
else:
    print("El carrito está vacío o el archivo no existe.")
