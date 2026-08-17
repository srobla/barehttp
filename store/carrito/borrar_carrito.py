#!/usr/bin/env python3
import os


try:
    # Abrir en modo 'w' trunca el archivo a 0 bytes
    with open("tienda/carrito/carrito.txt", "w") as f:
        pass 
    print("Carrito vaciado correctamente.")
except Exception as e:
    print(f"Error al vaciar: {e}")
