#!/bin/bash

echo "🔨 Compilando SGBD..."
echo "================================="

# Cambiar al directorio src
cd src

# Compilar el proyecto
g++ -std=c++17 -I. -o sgbd main.cpp data_storage/*.cpp record_manager/*.cpp Catalog_Manager/*.cpp replacement_policies/*.cpp query_processor/*.cpp

# Verificar si la compilación fue exitosa
if [ $? -eq 0 ]; then
    echo "✅ Compilación exitosa!"
    echo ""
    echo "Para ejecutar el SGBD:"
    echo "  cd src"
    echo "  ./sgbd"
    echo ""
    echo "O ejecutar directamente:"
    echo "  src/sgbd"
else
    echo "❌ Error en la compilación"
    exit 1
fi