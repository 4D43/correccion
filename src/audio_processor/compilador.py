import re
import os
import string
from datetime import datetime
import collections
from difflib import get_close_matches

# --- Definición del Nodo Trie ---
class TrieNode:
    def __init__(self):
        self.children = collections.defaultdict(TrieNode)
        self.is_end_of_word = False

# --- Estructura de Datos Trie ---
class Trie:
    def __init__(self):
        self.root = TrieNode()

    def insert(self, word):
        node = self.root
        for char in word:
            node = node.children[char]
        node.is_end_of_word = True

    def search(self, word):
        node = self.root
        for char in word:
            if char not in node.children:
                return False
            node = node.children[char]
        return node.is_end_of_word

    def starts_with(self, prefix):
        node = self.root
        for char in prefix:
            if char not in node.children:
                return False
            node = node.children[char]
        return True

    # Método para obtener todas las palabras en el Trie
    def get_all_words(self, node=None, prefix="", words=None):
        if node is None:
            node = self.root
        if words is None:
            words = []

        if node.is_end_of_word:
            words.append(prefix)

        for char, child_node in node.children.items():
            self.get_all_words(child_node, prefix + char, words)
        return words

    def print_all_words(self):
            
            print("\n--- Palabras en el Trie ---")
            words = self.get_all_words() # Reutilizamos el método existente
            if not words:
                print("El Trie está vacío.")
                return

            for word in sorted(words): # Opcional: ordenar alfabéticamente
                print(word)
            print("---------------------------")

# --- Función para Generar el Conjunto de Datos y Construir el Trie ---
def generate_dataset_and_trie(file_path):
    """
    Genera un conjunto de palabras únicas a partir de un archivo de texto,
    excluyendo palabras clave comunes de tipos de datos, y las almacena en un Trie.
    Ahora también separa las palabras que contienen '#'.

    Args:
        file_path (str): La ruta al archivo de texto de entrada.

    Returns:
        Trie: Un Trie que contiene las palabras únicas del archivo.
    """
    data_type_keywords = {
        "int", "integer", "float", "double", "string", "char", "boolean",
        "bool", "void", "long", "short", "byte", "decimal", "date", "time",
        "datetime", "array", "list", "dict", "dictionary", "set", "tuple",
        "object", "class"
    }

    unique_words_trie = Trie()
    processed_words = set()

    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                # Modificación: Usamos re.split para dividir por espacios y por '#'
                # Esto manejará casos como "palabra#otra_palabra" o "palabra # otra"
                words_in_line = re.split(r'\s+|#', line.lower())
                
                for word in words_in_line:
                    cleaned_word = word.strip(string.punctuation).strip() # Limpiar puntuación y espacios extra
                    
                    if cleaned_word and cleaned_word not in data_type_keywords and cleaned_word not in processed_words:
                        # Si la palabra todavía contiene '#', se asegura de que se haya manejado o se inserte
                        # Si usamos re.split(r'\s+|#', ...), ya deberían estar separadas.
                        # Este bloque asegura que solo palabras válidas y únicas se inserten.
                        unique_words_trie.insert(cleaned_word)
                        processed_words.add(cleaned_word)
    except FileNotFoundError:
        print(f"Error: El archivo '{file_path}' no fue encontrado.")
        return None
    except Exception as e:
        print(f"Ocurrió un error: {e}")
        return None

    return unique_words_trie
    
# === Formatos de fecha compatibles ===
FORMATOS_FECHA = [ #
    r"\d{4}-\d{2}-\d{2}",              # 2025-07-21
    r"\d{2}/\d{2}/\d{4}",              # 21/07/2025
    r"\d{2}-\d{2}-\d{4}",              # 21-07-2025
    r"\d{1,2} de [a-z]+ de \d{4}"      # 21 de julio de 2025
]

def es_fecha(texto): #
    texto = texto.lower() #
    for formato in FORMATOS_FECHA: #
        if re.fullmatch(formato, texto): #
            return True #
    return False #

def unir_fecha(tokens, inicio): #
    partes = [] #
    i = inicio #
    while i < len(tokens): #
        partes.append(tokens[i][1]) #
        posible_fecha = ' '.join(partes) #
        if es_fecha(posible_fecha): #
            return posible_fecha, i - inicio + 1 #
        i += 1 #
    return None, 0 #

# === Análisis Léxico ===
def analisis_lexico(texto): #
    texto = texto.lower() #
    palabras = texto.split() #
    palabras = [p.strip(string.punctuation) for p in palabras if p.strip(string.punctuation)] #

    acciones = { #
        "muéstrame", "muestrame", "mostrar", "muestra", "dame", "dámelos", "dámelas",
        "enséñame", "ensename", "quiero", "consultar", "consulta", "ver", "visualizar",
        "verifica", "explora", "lista", "listar", "recupera", "recuperar", "busca",
        "buscar", "obtén", "obtener", "extrae", "extraer", "filtra", "filtrar",
        "accede", "acceder", "selecciona", "seleccionar", "deseo", "necesito"
    }

    cuantificadores = { #
        "todos", "todas", "los", "las", "algunos", "algunas", "ninguno", "ninguna",
        "cada", "varios", "cualquier", "cualquiera", "muchos", "muchas", "pocos", "pocas",
        "uno", "una", "el", "la", "este", "esta", "estos", "estas"
    }

    conectores = { #
        "que", "donde", "cuyo", "cuyos", "cual", "cuales", "si", "cuando", "mientras",
        "aunque", "y", "o"
    }

    operadores = {"=", ">", "<", ">=", "<=", "!=", "<>"} #

    tokens = [] #
    for palabra in palabras: #
        if palabra in acciones: #
            tokens.append(("ACCION", palabra)) #
        elif palabra in cuantificadores: #
            tokens.append(("CUANTIFICADOR", palabra)) #
        elif palabra in conectores: #
            tokens.append(("CONECTOR", palabra)) #
        elif palabra in operadores: #
            tokens.append(("OPERADOR", palabra)) #
        elif palabra in {"tabla", "tablas", "base", "bases", "entidad", "entidades"}: #
            tokens.append(("INDICADOR_ENTIDAD", palabra)) #
        elif palabra in {"columna", "columnas", "campo", "campos", "atributo", "atributos"}: #
            tokens.append(("INDICADOR_ATRIBUTO", palabra)) #
        else:
            tokens.append(("PALABRA", palabra)) #
    return tokens #

def _reemplazar_operadores_compuestos(texto): #
    texto = texto.lower() #
    mapeo = { #
        "mayor o igual que": ">=",
        "menor o igual que": "<=",
        "mayor que": ">",
        "menor que": "<",
        "menor a": "<",
        "igual a": "=",
        "igual": "=",
        "no es": "!=",
        "diferente de": "!="
    }
    for frase, reemplazo in mapeo.items(): #
        texto = re.sub(r'\b' + re.escape(frase) + r'\b', reemplazo, texto) #
    return texto #

# === Análisis Sintáctico ===
def analisis_sintactico(tokens): #
    estructura = { #
        "accion": None,
        "entidad": None,
        "condiciones": []
    }

    i = 0 #
    while i < len(tokens): #
        tipo, valor = tokens[i] #

        # Detectar fechas en una sola palabra
        if es_fecha(valor): #
            estructura["condiciones"].append({ #
                "atributo": "fecha",
                "operador": "=",
                "valor": valor
            })
            i += 1 #
            continue #

        # Unir tokens para frases como "21 de julio de 2025"
        if valor == "de" and i > 1 and tokens[i-1][1] == "en": #
            fecha, saltos = unir_fecha(tokens, i-1) #
            if fecha: #
                estructura["condiciones"].append({ #
                    "atributo": "fecha",
                    "operador": "=",
                    "valor": fecha
                })
                i += saltos #
                continue #

        # Acción principal
        if tipo == "ACCION" and estructura["accion"] is None: #
            estructura["accion"] = valor #

        elif tipo == "INDICADOR_ENTIDAD": #
            if i + 1 < len(tokens) and tokens[i+1][0] == "PALABRA": #
                estructura["entidad"] = tokens[i+1][1] #
                i += 1 #

        elif tipo == "PALABRA" and estructura["entidad"] is None: #
            if valor in {"clientes", "productos", "ventas"}:  # tablas válidas
                estructura["entidad"] = valor #
            elif valor in {"nombre", "edad", "id", "dept", "precio", "fecha"}:  # atributos válidos
                estructura.setdefault("atributos_mostrar", []).append(valor) #


        elif tipo == "CONECTOR" and valor in {"donde", "que"}: #
            j = i + 1 #
            while j < len(tokens) - 2: #
                if ( #
                    tokens[j][0] == "PALABRA" and #
                    tokens[j+1][0] == "PALABRA" and #
                    tokens[j+2][0] == "PALABRA"
                ):
                    atributo = tokens[j][1] #
                    operador_compuesto = f"{tokens[j+1][1]} {tokens[j+2][1]}" #
                    operador_normalizado = _reemplazar_operadores_compuestos(operador_compuesto) #
                    if operador_normalizado in {"=", ">", "<", ">=", "<=", "!=", "<>"}: #
                        if j + 3 < len(tokens): #
                            estructura["condiciones"].append({ #
                                "atributo": atributo,
                                "operador": operador_normalizado,
                                "valor": tokens[j+3][1]
                            })
                            j += 4 #
                            continue #
                j += 1 #
            i = j #
            continue #

        elif valor == "con" and i + 4 < len(tokens): #
            atributo = tokens[i+1][1] #
            posible_operador = f"{tokens[i+2][1]} {tokens[i+3][1]}" #
            operador = _reemplazar_operadores_compuestos(posible_operador) #
            valor_c = tokens[i+4][1] #

            if operador in {"=", ">", "<", ">=", "<=", "!=", "<>"}: #
                estructura["condiciones"].append({ #
                    "atributo": atributo,
                    "operador": operador,
                    "valor": valor_c
                })
                i += 4 #

        elif valor == "de" and i > 0 and tokens[i-1][0] == "PALABRA": #
            if not (i+1 < len(tokens) and es_fecha(tokens[i+1][1])): #
                if estructura["entidad"] is None: #
                    estructura["entidad"] = tokens[i-1][1] #
                if i + 1 < len(tokens) and tokens[i+1][0] == "PALABRA": #
                    estructura["condiciones"].append({ #
                        "atributo": "dept",
                        "operador": "=",
                        "valor": tokens[i+1][1].rstrip(".")
                    })
                    i += 1 #

        
            # Ej: "cliente llamado Lucia" o "cliente llamada Lucia"
        elif valor in {"llamado","llamados", "llamada"} and i > 0 and tokens[i-1][0] == "PALABRA": #
            if estructura["entidad"] is None: #
                estructura["entidad"] = tokens[i-1][1] #
            if i + 1 < len(tokens): #
                estructura["condiciones"].append({ #
                    "atributo": "nombre",
                    "operador": "=",
                    "valor": tokens[i+1][1]
                })
                i += 1  # Saltar el nombre

        # Ej: "cliente que se llama Pedro"
        elif (valor == "llama" or valor == "llaman") and i >= 2 and tokens[i-1][1] == "se": #
            if estructura["entidad"] is None and i >= 3: #
                estructura["entidad"] = tokens[i-3][1] #
            if i + 1 < len(tokens): #
                estructura["condiciones"].append({ #
                    "atributo": "nombre",
                    "operador": "=",
                    "valor": tokens[i+1][1]
                })
                i += 1 #
        i += 1 #

    return estructura #

# === Análisis Semántico ===
def analisis_semantico_mejorado(estructura): #
    condiciones_ln = [] #
    operador_map = { #
        "=": "igual",
        ">": "mayor",
        "<": "menor",
        ">=": "mayor o igual",
        "<=": "menor o igual",
        "!=": "diferente",
        "<>": "diferente"
    }
    for cond in estructura["condiciones"]: #
        atributo = cond["atributo"] #
        operador = operador_map.get(cond["operador"], cond["operador"]) #
        valor = cond["valor"] #
        condiciones_ln.append(f"{atributo} {operador} {valor}") #
    return " y ".join(condiciones_ln) #

# === Generar Consulta en Lenguaje Natural ===
def generar_lenguaje_natural_final(estructura, condiciones_ln): #
    if not estructura["entidad"]: #
        return "-- No se puede generar consulta en LN: entidad desconocida." #
    consulta = f"selecciona * de {estructura['entidad']}" #
    if condiciones_ln: #
        consulta += f" donde {condiciones_ln}" #
    return consulta + "." #

# === Generar SQL ===
def generar_sql(estructura): #
    if not estructura["entidad"]: #
        return "-- No se puede generar consulta SQL: entidad desconocida." #
    if "atributos_mostrar" in estructura and estructura["atributos_mostrar"]: #
        campos = ", ".join(estructura["atributos_mostrar"]) #
        sql = f"SELECT {campos} FROM {estructura['entidad']}" #
    else:
        sql = f"SELECT * FROM {estructura['entidad']}" #

    condiciones = [] #

    for cond in estructura["condiciones"]: #
        atributo = cond["atributo"] #
        operador = cond["operador"] #
        valor = cond["valor"] #

        # Conversión de fecha con formato largo
        if atributo == "fecha" and re.match(r"\d{1,2} de [a-z]+ de \d{4}", valor): #
            try:
                fecha_obj = datetime.strptime(valor, "%d de %B de %Y") #
                valor = fecha_obj.strftime("%Y-%m-%d") #
            except:
                pass #
            condiciones.append(f"{atributo} {operador} DATE('{valor}')") #
        else:
            if valor.replace(".", "", 1).isdigit(): #
                condiciones.append(f"{atributo} {operador} {valor}") #
            else:
                condiciones.append(f"{atributo} {operador} '{valor}'") #

    if condiciones: #
        sql += " WHERE " + " AND ".join(condiciones) #
    return sql  #

# === Función de Revisión y Sugerencia ===
def revisar_y_sugerir_traduccion(sql_query, estructura, diccionario_valido_trie, original_text):
    """
    Revisa la consulta SQL generada y la estructura sintáctica en busca de entidades
    o atributos que no coincidan exactamente con el diccionario válido (Trie).
    Ofrece sugerencias al usuario para corregir o confirmar.

    Args:
        sql_query (str): La consulta SQL generada.
        estructura (dict): La estructura sintáctica analizada.
        diccionario_valido_trie (Trie): Un Trie que contiene todas las tablas y columnas válidas.
        original_text (str): El texto original en lenguaje natural.

    Returns:
        str: La consulta SQL revisada y potencialmente modificada.
    """
    todas_palabras_validas = diccionario_valido_trie.get_all_words()
    
    # Lista para almacenar las revisiones necesarias
    revisiones = []

    # 1. Revisar la entidad (nombre de la tabla)
    entidad_sugerida = estructura.get("entidad")
    if entidad_sugerida and not diccionario_valido_trie.search(entidad_sugerida):
        revisiones.append({
            "tipo": "entidad",
            "original": entidad_sugerida,
            "campo_estructura": "entidad"
        })

    # 2. Revisar atributos a mostrar
    atributos_mostrar = estructura.get("atributos_mostrar", [])
    for i, attr in enumerate(atributos_mostrar):
        if not diccionario_valido_trie.search(attr):
            revisiones.append({
                "tipo": "atributo_mostrar",
                "original": attr,
                "campo_estructura": "atributos_mostrar",
                "indice": i
            })

    # 3. Revisar atributos en las condiciones
    for i, cond in enumerate(estructura.get("condiciones", [])):
        atributo_cond = cond.get("atributo")
        if atributo_cond and not diccionario_valido_trie.search(atributo_cond):
            revisiones.append({
                "tipo": "atributo_condicion",
                "original": atributo_cond,
                "campo_estructura": "condiciones",
                "indice": i,
                "sub_campo": "atributo"
            })
        
        # Opcional: Revisar también los valores de las condiciones si son cadenas y no números/fechas
        # Para este ejemplo, nos enfocamos en tablas/columnas

    # Proceso de revisión interactivo
    if not revisiones:
        print("\n✅ La consulta parece utilizar tablas y columnas válidas.")
        return sql_query

    print("\n--- Revisión de la Consulta ---")
    print(f"Texto original: '{original_text}'")
    print(f"Consulta SQL generada inicialmente: '{sql_query}'")
    print("\n⚠️ Se encontraron posibles inconsistencias con las tablas/columnas válidas.")

    nueva_sql_query = sql_query # Copia para modificar

    for rev in revisiones:
        palabra_a_revisar = rev["original"]
        print(f"\nLa palabra '{palabra_a_revisar}' no parece ser una tabla o columna válida.")
        
        # Obtener sugerencias de palabras cercanas
        sugerencias = get_close_matches(palabra_a_revisar, todas_palabras_validas, n=5, cutoff=0.6)
        
        opciones = [palabra_a_revisar] + sugerencias
        
        print("Posibles traducciones o correcciones:")
        for idx, opcion in enumerate(opciones):
            print(f"  {idx + 1}. {opcion}")
        
        seleccion = -1
        while seleccion < 1 or seleccion > len(opciones):
            try:
                seleccion_str = input(f"Por favor, elige la opción correcta para '{palabra_a_revisar}' (1-{len(opciones)}): ")
                seleccion = int(seleccion_str)
                if seleccion < 1 or seleccion > len(opciones):
                    print("Opción no válida. Por favor, ingresa un número dentro del rango.")
            except ValueError:
                print("Entrada inválida. Por favor, ingresa un número.")
        
        palabra_elegida = opciones[seleccion - 1]
        
        # Actualizar la estructura y la consulta SQL con la palabra elegida
        if rev["tipo"] == "entidad":
            # Si el usuario eligió una nueva entidad, reemplazamos en la estructura y en la SQL
            old_entity = estructura["entidad"]
            estructura["entidad"] = palabra_elegida
            # Esto es una simplificación, una regex más robusta sería ideal para la SQL
            nueva_sql_query = nueva_sql_query.replace(f"FROM {old_entity}", f"FROM {palabra_elegida}")
            print(f"'{old_entity}' se actualizó a '{palabra_elegida}' en la estructura y SQL.")
        
        elif rev["tipo"] == "atributo_mostrar":
            old_attr = estructura["atributos_mostrar"][rev["indice"]]
            estructura["atributos_mostrar"][rev["indice"]] = palabra_elegida
            # Reemplazar solo la primera ocurrencia en SELECT para evitar problemas
            # Esto asume que el atributo está en la cláusula SELECT directamente
            # Un parseo de SQL más profundo sería mejor para casos complejos
            nueva_sql_query = re.sub(r'\b' + re.escape(old_attr) + r'\b', palabra_elegida, nueva_sql_query, 1)
            print(f"'{old_attr}' se actualizó a '{palabra_elegida}' en la estructura y SQL.")

        elif rev["tipo"] == "atributo_condicion":
            old_attr = estructura["condiciones"][rev["indice"]][rev["sub_campo"]]
            estructura["condiciones"][rev["indice"]][rev["sub_campo"]] = palabra_elegida
            # Buscar y reemplazar en la cláusula WHERE
            # Esto es más complejo ya que el atributo está en una condición
            # Una forma simple es buscar la cadena completa de la condición
            # y recrearla, o hacer un reemplazo basado en la estructura
            # Para este ejemplo, intentaremos un reemplazo simple que podría fallar en casos complejos
            # La mejor forma sería regenerar la SQL después de las actualizaciones de la estructura.
            
            # Regenerar la SQL después de cada cambio en la estructura es lo más seguro
            # para mantener la coherencia.
            nueva_sql_query = generar_sql(estructura) 
            print(f"'{old_attr}' se actualizó a '{palabra_elegida}' en la estructura y se regeneró la SQL.")


    print("\n--- Revisión Completa ---")
    print(f"Consulta SQL final después de la revisión: '{nueva_sql_query}'")
    
    return nueva_sql_query

# === Compilador Principal ===
def compilador_nl2sql_texto(texto): #
    print("Texto de entrada:", texto) #
    tokens = analisis_lexico(texto) #
    print("\nTokens léxicos:", tokens) #

    estructura = analisis_sintactico(tokens) #
    print("\nEstructura sintáctica:", estructura) #

    condiciones_ln = analisis_semantico_mejorado(estructura) #
    print("\nCondición en lenguaje natural:", condiciones_ln) #

    consulta_ln = generar_lenguaje_natural_final(estructura, condiciones_ln) #
    print("\nConsulta NL:", consulta_ln) #

    consulta_sql = generar_sql(estructura) #
    print("\nConsulta SQL:", consulta_sql) #

    return consulta_sql, estructura # Ahora devuelve la SQL y la estructura

# === Main ===
if __name__ == "__main__":
    nombre_archivo_transcripcion = "transcripcion.txt" #
    
    input_db_schema_file = "relaciones_tablas.txt" 
    
    nombre_archivo_para_gestor = "consulta_para_gestor.txt" #

    # Crear un esquema_db.txt de ejemplo si no existe
    if not os.path.exists(input_db_schema_file):
        print(f"Creando '{input_db_schema_file}' de ejemplo con tablas y columnas válidas.")
        with open(input_db_schema_file, "w", encoding="utf-8") as f:
            f.write("clientes\n")
            f.write("productos\n")
            f.write("ventas\n")
            f.write("nombre\n")
            f.write("edad\n")
            f.write("id\n")
            f.write("dept\n")
            f.write("precio\n")
            f.write("fecha\n")
            f.write("cantidad\n") # Añadimos una columna de ejemplo para probar

    # Generar el Trie con las tablas y columnas válidas de la DB
    diccionario_valido_trie = generate_dataset_and_trie(input_db_schema_file)

    if diccionario_valido_trie:
        print(f"\nPalabras válidas de la DB almacenadas en el Trie:")
        # --- LLAMADA A LA NUEVA FUNCIÓN ---
        diccionario_valido_trie.print_all_words()
        # --- FIN DE LA LLAMADA ---

    if os.path.exists(nombre_archivo_transcripcion): #
        with open(nombre_archivo_transcripcion, "r", encoding="utf-8") as f: #
            contenido_original = f.read().strip()
    else:
        print("⚠ No se encontró el archivo. Usando consulta de ejemplo.") #
        contenido_original = "muéstrame las bontas que se realizaron en 21 de julio de 2025 donde la edat es mayor a 30"
        # Ejemplo con errores intencionales: "bontas" en lugar de "ventas", "edat" en lugar de "edad"
        # También puedes probar: "muéstrame el nombe de los cliontes"

    sql_generado, estructura_sintactica = compilador_nl2sql_texto(contenido_original) # Captura la estructura

    if diccionario_valido_trie:
        sql_final = revisar_y_sugerir_traduccion(sql_generado, estructura_sintactica, diccionario_valido_trie, contenido_original)
    else:
        sql_final = sql_generado
        print("\nNo se pudo cargar el diccionario de tablas/columnas válidas. No se realizó la revisión interactiva.")

    # Guardar la consulta SQL final (después de la revisión)
    with open(nombre_archivo_para_gestor, "w", encoding="utf-8") as f: #
        f.write(sql_final)
    print(f"\nConsulta SQL final (revisada) guardada en: {nombre_archivo_para_gestor}") #