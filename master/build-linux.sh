#!/bin/bash

# Script para compilar el master en Linux
echo "Compilando Master para Linux..."

# Compilar so-commons-library
echo "1. Compilando so-commons-library..."
cd ../so-commons-library/src
make clean
make all
if [ $? -ne 0 ]; then
    echo "Error compilando so-commons-library"
    exit 1
fi

# Volver al directorio del master
cd ../../master

# Compilar el master
echo "2. Compilando Master..."
make clean
make debug
if [ $? -ne 0 ]; then
    echo "Error compilando Master"
    exit 1
fi

echo "¡Compilación exitosa!"
echo "Para ejecutar: ./bin/master master.config"
