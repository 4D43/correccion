// gestor_indices.cpp - Implementaci√≥n del Gestor de √çndices
// Refactorizado completamente a espa√±ol con funcionalidad mejorada

#include "gestor_indices.h"
#include "../data_storage/gestor_buffer.h"
#include "../Catalog_Manager/gestor_catalogo.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>

// === IMPLEMENTACI√ìN DE INDICEBTREETENTERO ===

IndiceBTreeEntero::IndiceBTreeEntero() 
    : raiz_(nullptr), altura_(0), numero_entradas_(0) {
    // Crear nodo ra√≠z inicial como hoja
    raiz_ = new NodoHoja();
    altura_ = 1;
}

IndiceBTreeEntero::~IndiceBTreeEntero() {
    if (raiz_) {
        LiberarNodo(raiz_);
    }
}

Status IndiceBTreeEntero::Insertar(const std::string& clave_str, int clave_int, RecordId id_registro) {
    if (!raiz_) {
        return Status::ERROR;
    }
    
    // Buscar la hoja donde debe insertarse la clave
    NodoHoja* hoja = BuscarHoja(clave_int);
    if (!hoja) {
        return Status::ERROR;
    }
    
    // Insertar en la hoja
    Status status = InsertarEnHoja(hoja, clave_int, id_registro);
    if (status == Status::OK) {
        numero_entradas_++;
    }
    
    return status;
}

Status IndiceBTreeEntero::Eliminar(const std::string& clave_str, int clave_int, RecordId id_registro) {
    if (!raiz_) {
        return Status::NOT_FOUND;
    }
    
    // Buscar la hoja que contiene la clave
    NodoHoja* hoja = BuscarHoja(clave_int);
    if (!hoja) {
        return Status::NOT_FOUND;
    }
    
    // Eliminar de la hoja
    Status status = EliminarDeHoja(hoja, clave_int, id_registro);
    if (status == Status::OK) {
        numero_entradas_--;
    }
    
    return status;
}

std::optional<std::set<RecordId>> IndiceBTreeEntero::Buscar(const std::string& clave_str, int clave_int) const {
    if (!raiz_) {
        return std::nullopt;
    }
    
    // Buscar la hoja que contiene la clave
    NodoHoja* hoja = BuscarHoja(clave_int);
    if (!hoja) {
        return std::nullopt;
    }
    
    // Buscar la clave en la hoja
    for (uint32_t i = 0; i < hoja->numero_claves; ++i) {
        if (hoja->entradas[i].clave == clave_int) {
            return hoja->entradas[i].ubicaciones;
        }
    }
    
    return std::nullopt;
}

IndiceBTreeEntero::NodoHoja* IndiceBTreeEntero::BuscarHoja(int clave) const {
    if (!raiz_) {
        return nullptr;
    }
    
    NodoBase* nodo_actual = raiz_;
    
    // Navegar hacia abajo hasta encontrar una hoja
    while (!nodo_actual->es_hoja) {
        NodoInterno* nodo_interno = static_cast<NodoInterno*>(nodo_actual);
        uint32_t i = 0;
        
        // Encontrar el hijo correcto
        while (i < nodo_interno->numero_claves && clave >= nodo_interno->claves[i]) {
            i++;
        }
        
        nodo_actual = nodo_interno->hijos[i];
        if (!nodo_actual) {
            return nullptr;
        }
    }
    
    return static_cast<NodoHoja*>(nodo_actual);
}

Status IndiceBTreeEntero::InsertarEnHoja(NodoHoja* hoja, int clave, RecordId id_registro) {
    if (!hoja) {
        return Status::ERROR;
    }
    
    // Buscar si la clave ya existe
    for (uint32_t i = 0; i < hoja->numero_claves; ++i) {
        if (hoja->entradas[i].clave == clave) {
            // La clave ya existe, agregar la ubicaci√≥n
            hoja->entradas[i].AgregarUbicacion(id_registro);
            return Status::OK;
        }
    }
    
    // La clave no existe, verificar si hay espacio
    if (hoja->numero_claves >= ORDEN_ARBOL - 1) {
        // Necesita divisi√≥n
        return DividirHoja(hoja);
    }
    
    // Encontrar posici√≥n de inserci√≥n
    uint32_t pos = 0;
    while (pos < hoja->numero_claves && hoja->entradas[pos].clave < clave) {
        pos++;
    }
    
    // Desplazar entradas hacia la derecha
    for (uint32_t i = hoja->numero_claves; i > pos; --i) {
        hoja->entradas[i] = hoja->entradas[i - 1];
    }
    
    // Insertar nueva entrada
    hoja->entradas[pos] = EntradaIndice<int>(clave);
    hoja->entradas[pos].AgregarUbicacion(id_registro);
    hoja->numero_claves++;
    
    return Status::OK;
}

Status IndiceBTreeEntero::DividirHoja(NodoHoja* hoja) {
    // TODO: Implementar divisi√≥n de hojas para el √°rbol B+
    // Por ahora, retornar error si no hay espacio
    return Status::ERROR;
}

Status IndiceBTreeEntero::EliminarDeHoja(NodoHoja* hoja, int clave, RecordId id_registro) {
    if (!hoja) {
        return Status::ERROR;
    }
    
    // Buscar la clave en la hoja
    for (uint32_t i = 0; i < hoja->numero_claves; ++i) {
        if (hoja->entradas[i].clave == clave) {
            // Eliminar la ubicaci√≥n espec√≠fica
            hoja->entradas[i].EliminarUbicacion(id_registro);
            
            // Si no quedan ubicaciones, eliminar la entrada completa
            if (hoja->entradas[i].EstaVacia()) {
                // Desplazar entradas hacia la izquierda
                for (uint32_t j = i; j < hoja->numero_claves - 1; ++j) {
                    hoja->entradas[j] = hoja->entradas[j + 1];
                }
                hoja->numero_claves--;
            }
            
            return Status::OK;
        }
    }
    
    return Status::NOT_FOUND;
}

void IndiceBTreeEntero::LiberarNodo(NodoBase* nodo) {
    if (!nodo) {
        return;
    }
    
    if (!nodo->es_hoja) {
        NodoInterno* nodo_interno = static_cast<NodoInterno*>(nodo);
        for (uint32_t i = 0; i <= nodo_interno->numero_claves; ++i) {
            LiberarNodo(nodo_interno->hijos[i]);
        }
    }
    
    delete nodo;
}

Status IndiceBTreeEntero::Persistir(const std::string& ruta_archivo) const {
    std::ofstream archivo(ruta_archivo);
    if (!archivo.is_open()) {
        return Status::ERROR;
    }
    
    // Escribir metadatos del √≠ndice
    archivo << "TIPO_INDICE:BTREE_ENTERO\n";
    archivo << "NUMERO_ENTRADAS:" << numero_entradas_ << "\n";
    archivo << "ALTURA:" << altura_ << "\n";
    
    // TODO: Implementar serializaci√≥n completa del √°rbol
    archivo << "DATOS_ARBOL:PENDIENTE\n";
    
    archivo.close();
    return Status::OK;
}

Status IndiceBTreeEntero::Cargar(const std::string& ruta_archivo) {
    std::ifstream archivo(ruta_archivo);
    if (!archivo.is_open()) {
        return Status::ERROR;
    }
    
    // TODO: Implementar deserializaci√≥n completa del √°rbol
    archivo.close();
    return Status::OK;
}

void IndiceBTreeEntero::ImprimirEstructura() const {
    std::cout << "\n=== ESTRUCTURA DEL √çNDICE B+ TREE (ENTERO) ===" << std::endl;
    std::cout << "Altura: " << altura_ << std::endl;
    std::cout << "N√∫mero de entradas: " << numero_entradas_ << std::endl;
    
    if (!raiz_) {
        std::cout << "√Årbol vac√≠o" << std::endl;
        return;
    }
    
    // TODO: Implementar impresi√≥n recursiva del √°rbol
    std::cout << "Estructura detallada: PENDIENTE" << std::endl;
}

// === IMPLEMENTACI√ìN DE INDICEBTREECADENA ===

IndiceBTreeCadena::IndiceBTreeCadena() 
    : raiz_(nullptr), altura_(0), numero_entradas_(0) {
    // Crear nodo ra√≠z inicial como hoja
    raiz_ = new NodoHoja();
    altura_ = 1;
}

IndiceBTreeCadena::~IndiceBTreeCadena() {
    if (raiz_) {
        LiberarNodo(raiz_);
    }
}

Status IndiceBTreeCadena::Insertar(const std::string& clave_str, int clave_int, RecordId id_registro) {
    if (!raiz_) {
        return Status::ERROR;
    }
    
    // Buscar la hoja donde debe insertarse la clave
    NodoHoja* hoja = BuscarHoja(clave_str);
    if (!hoja) {
        return Status::ERROR;
    }
    
    // Insertar en la hoja
    Status status = InsertarEnHoja(hoja, clave_str, id_registro);
    if (status == Status::OK) {
        numero_entradas_++;
    }
    
    return status;
}

Status IndiceBTreeCadena::Eliminar(const std::string& clave_str, int clave_int, RecordId id_registro) {
    if (!raiz_) {
        return Status::NOT_FOUND;
    }
    
    // Buscar la hoja que contiene la clave
    NodoHoja* hoja = BuscarHoja(clave_str);
    if (!hoja) {
        return Status::NOT_FOUND;
    }
    
    // Eliminar de la hoja
    Status status = EliminarDeHoja(hoja, clave_str, id_registro);
    if (status == Status::OK) {
        numero_entradas_--;
    }
    
    return status;
}

std::optional<std::set<RecordId>> IndiceBTreeCadena::Buscar(const std::string& clave_str, int clave_int) const {
    if (!raiz_) {
        return std::nullopt;
    }
    
    // Buscar la hoja que contiene la clave
    NodoHoja* hoja = BuscarHoja(clave_str);
    if (!hoja) {
        return std::nullopt;
    }
    
    // Buscar la clave en la hoja
    for (uint32_t i = 0; i < hoja->numero_claves; ++i) {
        if (hoja->entradas[i].clave == clave_str) {
            return hoja->entradas[i].ubicaciones;
        }
    }
    
    return std::nullopt;
}

IndiceBTreeCadena::NodoHoja* IndiceBTreeCadena::BuscarHoja(const std::string& clave) const {
    if (!raiz_) {
        return nullptr;
    }
    
    NodoBase* nodo_actual = raiz_;
    
    // Navegar hacia abajo hasta encontrar una hoja
    while (!nodo_actual->es_hoja) {
        NodoInterno* nodo_interno = static_cast<NodoInterno*>(nodo_actual);
        uint32_t i = 0;
        
        // Encontrar el hijo correcto
        while (i < nodo_interno->numero_claves && clave >= nodo_interno->claves[i]) {
            i++;
        }
        
        nodo_actual = nodo_interno->hijos[i];
        if (!nodo_actual) {
            return nullptr;
        }
    }
    
    return static_cast<NodoHoja*>(nodo_actual);
}

Status IndiceBTreeCadena::InsertarEnHoja(NodoHoja* hoja, const std::string& clave, RecordId id_registro) {
    if (!hoja) {
        return Status::ERROR;
    }
    
    // Buscar si la clave ya existe
    for (uint32_t i = 0; i < hoja->numero_claves; ++i) {
        if (hoja->entradas[i].clave == clave) {
            // La clave ya existe, agregar la ubicaci√≥n
            hoja->entradas[i].AgregarUbicacion(id_registro);
            return Status::OK;
        }
    }
    
    // La clave no existe, verificar si hay espacio
    if (hoja->numero_claves >= ORDEN_ARBOL - 1) {
        // Necesita divisi√≥n
        return DividirHoja(hoja);
    }
    
    // Encontrar posici√≥n de inserci√≥n
    uint32_t pos = 0;
    while (pos < hoja->numero_claves && hoja->entradas[pos].clave < clave) {
        pos++;
    }
    
    // Desplazar entradas hacia la derecha
    for (uint32_t i = hoja->numero_claves; i > pos; --i) {
        hoja->entradas[i] = hoja->entradas[i - 1];
    }
    
    // Insertar nueva entrada
    hoja->entradas[pos] = EntradaIndice<std::string>(clave);
    hoja->entradas[pos].AgregarUbicacion(id_registro);
    hoja->numero_claves++;
    
    return Status::OK;
}

Status IndiceBTreeCadena::DividirHoja(NodoHoja* hoja) {
    // TODO: Implementar divisi√≥n de hojas para el √°rbol B+
    return Status::ERROR;
}

Status IndiceBTreeCadena::EliminarDeHoja(NodoHoja* hoja, const std::string& clave, RecordId id_registro) {
    if (!hoja) {
        return Status::ERROR;
    }
    
    // Buscar la clave en la hoja
    for (uint32_t i = 0; i < hoja->numero_claves; ++i) {
        if (hoja->entradas[i].clave == clave) {
            // Eliminar la ubicaci√≥n espec√≠fica
            hoja->entradas[i].EliminarUbicacion(id_registro);
            
            // Si no quedan ubicaciones, eliminar la entrada completa
            if (hoja->entradas[i].EstaVacia()) {
                // Desplazar entradas hacia la izquierda
                for (uint32_t j = i; j < hoja->numero_claves - 1; ++j) {
                    hoja->entradas[j] = hoja->entradas[j + 1];
                }
                hoja->numero_claves--;
            }
            
            return Status::OK;
        }
    }
    
    return Status::NOT_FOUND;
}

void IndiceBTreeCadena::LiberarNodo(NodoBase* nodo) {
    if (!nodo) {
        return;
    }
    
    if (!nodo->es_hoja) {
        NodoInterno* nodo_interno = static_cast<NodoInterno*>(nodo);
        for (uint32_t i = 0; i <= nodo_interno->numero_claves; ++i) {
            LiberarNodo(nodo_interno->hijos[i]);
        }
    }
    
    delete nodo;
}

Status IndiceBTreeCadena::Persistir(const std::string& ruta_archivo) const {
    std::ofstream archivo(ruta_archivo);
    if (!archivo.is_open()) {
        return Status::ERROR;
    }
    
    // Escribir metadatos del √≠ndice
    archivo << "TIPO_INDICE:BTREE_CADENA\n";
    archivo << "NUMERO_ENTRADAS:" << numero_entradas_ << "\n";
    archivo << "ALTURA:" << altura_ << "\n";
    
    // TODO: Implementar serializaci√≥n completa del √°rbol
    archivo << "DATOS_ARBOL:PENDIENTE\n";
    
    archivo.close();
    return Status::OK;
}

Status IndiceBTreeCadena::Cargar(const std::string& ruta_archivo) {
    std::ifstream archivo(ruta_archivo);
    if (!archivo.is_open()) {
        return Status::ERROR;
    }
    
    // TODO: Implementar deserializaci√≥n completa del √°rbol
    archivo.close();
    return Status::OK;
}

void IndiceBTreeCadena::ImprimirEstructura() const {
    std::cout << "\n=== ESTRUCTURA DEL √çNDICE B+ TREE (CADENA) ===" << std::endl;
    std::cout << "Altura: " << altura_ << std::endl;
    std::cout << "N√∫mero de entradas: " << numero_entradas_ << std::endl;
    
    if (!raiz_) {
        std::cout << "√Årbol vac√≠o" << std::endl;
        return;
    }
    
    // TODO: Implementar impresi√≥n recursiva del √°rbol
    std::cout << "Estructura detallada: PENDIENTE" << std::endl;
}
/ /   = = =   I M P L E M E N T A C I √  N   D E   M √ 0 T O D O S   A U T O M √ Å T I C O S   D E   I N D E X A C I √  N   = = =  
  
 / * *  
   *   C r e a   u n   √ ≠ n d i c e   a u t o m √ ° t i c a m e n t e   s e g √ ∫ n   l a   e s t r a t e g i a   d e l   u s u a r i o :  
   *   -   T a b l a s   d e   l o n g i t u d   v a r i a b l e :   S I E M P R E   H a s h  
   *   -   T a b l a s   d e   l o n g i t u d   f i j a :   B +   T r e e   p a r a   I N T ,   S t r i n g   B +   T r e e   p a r a   S T R / C H A R  
   * /  
 S t a t u s   G e s t o r I n d i c e s : : C r e a r I n d i c e A u t o m a t i c o ( c o n s t   s t d : : s t r i n g &   n o m b r e _ t a b l a ,    
                                                                                       c o n s t   s t d : : s t r i n g &   n o m b r e _ c o l u m n a )   {  
         / /   V a l i d a r   p a r √ ° m e t r o s  
         S t a t u s   s t a t u s   =   V a l i d a r P a r a m e t r o s I n d i c e ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
         i f   ( s t a t u s   ! =   S t a t u s : : O K )   {  
                 s t d : : c e r r   < <   " E r r o r :   P a r √ ° m e t r o s   d e   √ ≠ n d i c e   i n v √ ° l i d o s   p a r a   t a b l a   ' "    
                                     < <   n o m b r e _ t a b l a   < <   " ' ,   c o l u m n a   ' "   < <   n o m b r e _ c o l u m n a   < <   " ' "   < <   s t d : : e n d l ;  
                 r e t u r n   s t a t u s ;  
         }  
          
         / /   V e r i f i c a r   s i   y a   e x i s t e   u n   √ ≠ n d i c e   p a r a   e s t a   t a b l a   y   c o l u m n a  
         i f   ( E x i s t e I n d i c e ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) )   {  
                 s t d : : c o u t   < <   " A v i s o :   Y a   e x i s t e   u n   √ ≠ n d i c e   p a r a   l a   t a b l a   ' "   < <   n o m b r e _ t a b l a    
                                     < <   " ' ,   c o l u m n a   ' "   < <   n o m b r e _ c o l u m n a   < <   " ' .   S e   r e c o n s t r u i r √ ° . "   < <   s t d : : e n d l ;  
                 r e t u r n   R e c o n s t r u i r I n d i c e ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
         }  
          
         / /   S e l e c c i o n a r   a u t o m √ ° t i c a m e n t e   e l   t i p o   d e   √ ≠ n d i c e   s e g √ ∫ n   l a   e s t r a t e g i a  
         T i p o I n d i c e   t i p o _ s e l e c c i o n a d o   =   S e l e c c i o n a r T i p o I n d i c e A u t o m a t i c o ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
          
         s t d : : c o u t   < <   " \ n = = =   C R E A C I √  N   A U T O M √ Å T I C A   D E   √ ç N D I C E   = = = "   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   " T a b l a :   "   < <   n o m b r e _ t a b l a   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   " C o l u m n a :   "   < <   n o m b r e _ c o l u m n a   < <   s t d : : e n d l ;  
          
         / /   M o s t r a r   i n f o r m a c i √ ≥ n   s o b r e   l a   e s t r a t e g i a   s e l e c c i o n a d a  
         b o o l   e s _ v a r i a b l e   =   E s T a b l a L o n g i t u d V a r i a b l e ( n o m b r e _ t a b l a ) ;  
         C o l u m n T y p e   t i p o _ c o l u m n a   =   O b t e n e r T i p o C o l u m n a ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
          
         s t d : : c o u t   < <   " T i p o   d e   t a b l a :   "   < <   ( e s _ v a r i a b l e   ?   " L o n g i t u d   V a r i a b l e "   :   " L o n g i t u d   F i j a " )   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   " T i p o   d e   c o l u m n a :   " ;  
         s w i t c h   ( t i p o _ c o l u m n a )   {  
                 c a s e   C o l u m n T y p e : : I N T :  
                         s t d : : c o u t   < <   " I N T " ;  
                         b r e a k ;  
                 c a s e   C o l u m n T y p e : : S T R I N G :  
                         s t d : : c o u t   < <   " S T R I N G " ;  
                         b r e a k ;  
                 c a s e   C o l u m n T y p e : : C H A R :  
                         s t d : : c o u t   < <   " C H A R " ;  
                         b r e a k ;  
                 d e f a u l t :  
                         s t d : : c o u t   < <   " D E S C O N O C I D O " ;  
                         b r e a k ;  
         }  
         s t d : : c o u t   < <   s t d : : e n d l ;  
          
         s t d : : c o u t   < <   " E s t r a t e g i a   s e l e c c i o n a d a :   " ;  
         s w i t c h   ( t i p o _ s e l e c c i o n a d o )   {  
                 c a s e   T i p o I n d i c e : : H A S H _ C A D E N A :  
                         s t d : : c o u t   < <   " H A S H   ( t a b l a   d e   l o n g i t u d   v a r i a b l e ) " ;  
                         b r e a k ;  
                 c a s e   T i p o I n d i c e : : B T R E E _ E N T E R O :  
                         s t d : : c o u t   < <   " B +   T R E E   E N T E R O   ( t a b l a   f i j a ,   c o l u m n a   I N T ) " ;  
                         b r e a k ;  
                 c a s e   T i p o I n d i c e : : B T R E E _ C A D E N A :  
                         s t d : : c o u t   < <   " B +   T R E E   C A D E N A   ( t a b l a   f i j a ,   c o l u m n a   S T R / C H A R ) " ;  
                         b r e a k ;  
         }  
         s t d : : c o u t   < <   s t d : : e n d l ;  
          
         / /   C r e a r   e l   √ ≠ n d i c e   c o n   e l   t i p o   s e l e c c i o n a d o  
         s t a t u s   =   C r e a r I n d i c e ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ,   t i p o _ s e l e c c i o n a d o ) ;  
          
         i f   ( s t a t u s   = =   S t a t u s : : O K )   {  
                 s t d : : c o u t   < <   " ‚ S&   √ ç n d i c e   c r e a d o   e x i t o s a m e n t e   c o n   e s t r a t e g i a   a u t o m √ ° t i c a "   < <   s t d : : e n d l ;  
                  
                 / /   S i   l a   p e r s i s t e n c i a   a u t o m √ ° t i c a   e s t √ °   a c t i v a d a ,   g u a r d a r   i n m e d i a t a m e n t e  
                 i f   ( p e r s i s t e n c i a _ a u t o m a t i c a _ )   {  
                         S t a t u s   s t a t u s _ p e r s i s t i r   =   P e r s i s t i r I n d i c e ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
                         i f   ( s t a t u s _ p e r s i s t i r   = =   S t a t u s : : O K )   {  
                                 s t d : : c o u t   < <   " ‚ S&   √ ç n d i c e   p e r s i s t i d o   a u t o m √ ° t i c a m e n t e   e n   d i s c o "   < <   s t d : : e n d l ;  
                         }   e l s e   {  
                                 s t d : : c o u t   < <   " ‚ a† Ô ∏ è   A d v e r t e n c i a :   E r r o r   a l   p e r s i s t i r   √ ≠ n d i c e   a u t o m √ ° t i c a m e n t e "   < <   s t d : : e n d l ;  
                         }  
                 }  
         }   e l s e   {  
                 s t d : : c e r r   < <   " ‚ ù R  E r r o r   a l   c r e a r   √ ≠ n d i c e   a u t o m √ ° t i c o "   < <   s t d : : e n d l ;  
         }  
          
         r e t u r n   s t a t u s ;  
 }  
  
 / * *  
   *   S e l e c c i o n a   a u t o m √ ° t i c a m e n t e   e l   t i p o   d e   √ ≠ n d i c e   s e g √ ∫ n   l a   e s t r a t e g i a   d e l   u s u a r i o  
   * /  
 T i p o I n d i c e   G e s t o r I n d i c e s : : S e l e c c i o n a r T i p o I n d i c e A u t o m a t i c o ( c o n s t   s t d : : s t r i n g &   n o m b r e _ t a b l a ,    
                                                                                                                     c o n s t   s t d : : s t r i n g &   n o m b r e _ c o l u m n a )   c o n s t   {  
         / /   V e r i f i c a r   s i   l a   t a b l a   e s   d e   l o n g i t u d   v a r i a b l e  
         i f   ( E s T a b l a L o n g i t u d V a r i a b l e ( n o m b r e _ t a b l a ) )   {  
                 / /   R E G L A   1 :   T a b l a s   d e   l o n g i t u d   v a r i a b l e   S I E M P R E   u s a n   H a s h  
                 r e t u r n   T i p o I n d i c e : : H A S H _ C A D E N A ;  
         }  
          
         / /   R E G L A   2 :   T a b l a s   d e   l o n g i t u d   f i j a   d e p e n d e n   d e l   t i p o   d e   c o l u m n a  
         C o l u m n T y p e   t i p o _ c o l u m n a   =   O b t e n e r T i p o C o l u m n a ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
          
         s w i t c h   ( t i p o _ c o l u m n a )   {  
                 c a s e   C o l u m n T y p e : : I N T :  
                         r e t u r n   T i p o I n d i c e : : B T R E E _ E N T E R O ;  
                          
                 c a s e   C o l u m n T y p e : : S T R I N G :  
                 c a s e   C o l u m n T y p e : : C H A R :  
                         r e t u r n   T i p o I n d i c e : : B T R E E _ C A D E N A ;  
                          
                 d e f a u l t :  
                         / /   P o r   d e f e c t o ,   u s a r   B +   T r e e   d e   c a d e n a   p a r a   t i p o s   d e s c o n o c i d o s  
                         s t d : : c o u t   < <   " A d v e r t e n c i a :   T i p o   d e   c o l u m n a   d e s c o n o c i d o ,   u s a n d o   B +   T r e e   d e   c a d e n a   p o r   d e f e c t o "   < <   s t d : : e n d l ;  
                         r e t u r n   T i p o I n d i c e : : B T R E E _ C A D E N A ;  
         }  
 }  
  
 / * *  
   *   D e t e r m i n a   s i   u n a   t a b l a   e s   d e   l o n g i t u d   v a r i a b l e   c o n s u l t a n d o   e l   c a t √ ° l o g o  
   * /  
 b o o l   G e s t o r I n d i c e s : : E s T a b l a L o n g i t u d V a r i a b l e ( c o n s t   s t d : : s t r i n g &   n o m b r e _ t a b l a )   c o n s t   {  
         i f   ( ! g e s t o r _ c a t a l o g o _ )   {  
                 s t d : : c e r r   < <   " E r r o r :   G e s t o r   d e   c a t √ ° l o g o   n o   d i s p o n i b l e   p a r a   c o n s u l t a r   t i p o   d e   t a b l a "   < <   s t d : : e n d l ;  
                 r e t u r n   f a l s e ;   / /   P o r   d e f e c t o ,   a s u m i r   l o n g i t u d   f i j a  
         }  
          
         / /   T O D O :   I m p l e m e n t a r   c o n s u l t a   a l   g e s t o r   d e   c a t √ ° l o g o  
         / /   P o r   a h o r a ,   i m p l e m e n t a c i √ ≥ n   t e m p o r a l   b a s a d a   e n   c o n v e n c i √ ≥ n   d e   n o m b r e s  
         / /   L a s   t a b l a s   q u e   t e r m i n a n   e n   " _ v a r "   s e   c o n s i d e r a n   d e   l o n g i t u d   v a r i a b l e  
         r e t u r n   n o m b r e _ t a b l a . f i n d ( " _ v a r " )   ! =   s t d : : s t r i n g : : n p o s   | |    
                       n o m b r e _ t a b l a . f i n d ( " _ v a r i a b l e " )   ! =   s t d : : s t r i n g : : n p o s ;  
 }  
  
 / * *  
   *   O b t i e n e   e l   t i p o   d e   u n a   c o l u m n a   e s p e c √ ≠ f i c a   c o n s u l t a n d o   e l   c a t √ ° l o g o  
   * /  
 C o l u m n T y p e   G e s t o r I n d i c e s : : O b t e n e r T i p o C o l u m n a ( c o n s t   s t d : : s t r i n g &   n o m b r e _ t a b l a ,    
                                                                                           c o n s t   s t d : : s t r i n g &   n o m b r e _ c o l u m n a )   c o n s t   {  
         i f   ( ! g e s t o r _ c a t a l o g o _ )   {  
                 s t d : : c e r r   < <   " E r r o r :   G e s t o r   d e   c a t √ ° l o g o   n o   d i s p o n i b l e   p a r a   c o n s u l t a r   t i p o   d e   c o l u m n a "   < <   s t d : : e n d l ;  
                 r e t u r n   C o l u m n T y p e : : S T R I N G ;   / /   P o r   d e f e c t o ,   a s u m i r   S T R I N G  
         }  
          
         / /   T O D O :   I m p l e m e n t a r   c o n s u l t a   r e a l   a l   g e s t o r   d e   c a t √ ° l o g o  
         / /   P o r   a h o r a ,   i m p l e m e n t a c i √ ≥ n   t e m p o r a l   b a s a d a   e n   c o n v e n c i √ ≥ n   d e   n o m b r e s  
         s t d : : s t r i n g   c o l u m n a _ l o w e r   =   n o m b r e _ c o l u m n a ;  
         s t d : : t r a n s f o r m ( c o l u m n a _ l o w e r . b e g i n ( ) ,   c o l u m n a _ l o w e r . e n d ( ) ,   c o l u m n a _ l o w e r . b e g i n ( ) ,   : : t o l o w e r ) ;  
          
         i f   ( c o l u m n a _ l o w e r . f i n d ( " i d " )   ! =   s t d : : s t r i n g : : n p o s   | |    
                 c o l u m n a _ l o w e r . f i n d ( " a g e " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " e d a d " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " n u m e r o " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " c o u n t " )   ! =   s t d : : s t r i n g : : n p o s )   {  
                 r e t u r n   C o l u m n T y p e : : I N T ;  
         }  
          
         r e t u r n   C o l u m n T y p e : : S T R I N G ;   / /   P o r   d e f e c t o  
 }  
 / /   g e s t o r _ i n d i c e s _ a u t o m a t i c o . c p p   -   I m p l e m e n t a c i √ ≥ n   d e   i n d e x a c i √ ≥ n   a u t o m √ ° t i c a  
 / /   F u n c i o n a l i d a d   a v a n z a d a   p a r a   s e l e c c i √ ≥ n   i n t e l i g e n t e   d e   √ ≠ n d i c e s   s e g √ ∫ n   t i p o   d e   t a b l a   y   c o l u m n a  
  
 # i n c l u d e   " g e s t o r _ i n d i c e s . h "  
 # i n c l u d e   " . . / C a t a l o g _ M a n a g e r / g e s t o r _ c a t a l o g o . h "  
 # i n c l u d e   < a l g o r i t h m >  
 # i n c l u d e   < c c t y p e >  
  
 / /   = = =   I M P L E M E N T A C I √  N   D E   M √ 0 T O D O S   A U T O M √ Å T I C O S   D E   I N D E X A C I √  N   = = =  
  
 / * *  
   *   C r e a   u n   √ ≠ n d i c e   a u t o m √ ° t i c a m e n t e   s e g √ ∫ n   l a   e s t r a t e g i a   d e l   u s u a r i o :  
   *   -   T a b l a s   d e   l o n g i t u d   v a r i a b l e :   S I E M P R E   H a s h  
   *   -   T a b l a s   d e   l o n g i t u d   f i j a :   B +   T r e e   p a r a   I N T ,   S t r i n g   B +   T r e e   p a r a   S T R / C H A R  
   * /  
 S t a t u s   G e s t o r I n d i c e s : : C r e a r I n d i c e A u t o m a t i c o ( c o n s t   s t d : : s t r i n g &   n o m b r e _ t a b l a ,    
                                                                                       c o n s t   s t d : : s t r i n g &   n o m b r e _ c o l u m n a )   {  
         s t d : : c o u t   < <   " \ n  x ç   = = =   C R E A C I √  N   A U T O M √ Å T I C A   D E   √ ç N D I C E   = = = "   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   "  x `  T a b l a :   "   < <   n o m b r e _ t a b l a   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   "  x 9   C o l u m n a :   "   < <   n o m b r e _ c o l u m n a   < <   s t d : : e n d l ;  
          
         / /   V a l i d a r   p a r √ ° m e t r o s  
         S t a t u s   s t a t u s   =   V a l i d a r P a r a m e t r o s I n d i c e ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
         i f   ( s t a t u s   ! =   S t a t u s : : O K )   {  
                 s t d : : c e r r   < <   " ‚ ù R  E r r o r :   P a r √ ° m e t r o s   d e   √ ≠ n d i c e   i n v √ ° l i d o s "   < <   s t d : : e n d l ;  
                 r e t u r n   s t a t u s ;  
         }  
          
         / /   V e r i f i c a r   s i   y a   e x i s t e   u n   √ ≠ n d i c e   p a r a   e s t a   t a b l a   y   c o l u m n a  
         i f   ( E x i s t e I n d i c e ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) )   {  
                 s t d : : c o u t   < <   " ‚ a† Ô ∏ è   A v i s o :   Y a   e x i s t e   u n   √ ≠ n d i c e .   S e   r e c o n s t r u i r √ ° . "   < <   s t d : : e n d l ;  
                 r e t u r n   R e c o n s t r u i r I n d i c e ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
         }  
          
         / /   P A S O   1 :   A n a l i z a r   e l   t i p o   d e   t a b l a   ( l o n g i t u d   f i j a   v s   v a r i a b l e )  
         b o o l   e s _ t a b l a _ v a r i a b l e   =   E s T a b l a L o n g i t u d V a r i a b l e ( n o m b r e _ t a b l a ) ;  
         s t d : : c o u t   < <   "  x è   T i p o   d e   t a b l a :   "   < <   ( e s _ t a b l a _ v a r i a b l e   ?   " L o n g i t u d   V a r i a b l e "   :   " L o n g i t u d   F i j a " )   < <   s t d : : e n d l ;  
          
         / /   P A S O   2 :   O b t e n e r   e l   t i p o   d e   c o l u m n a  
         C o l u m n T y p e   t i p o _ c o l u m n a   =   O b t e n e r T i p o C o l u m n a ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
         s t d : : c o u t   < <   "  x §   T i p o   d e   c o l u m n a :   " ;  
         s w i t c h   ( t i p o _ c o l u m n a )   {  
                 c a s e   C o l u m n T y p e : : I N T :  
                         s t d : : c o u t   < <   " I N T   ( e n t e r o ) " ;  
                         b r e a k ;  
                 c a s e   C o l u m n T y p e : : S T R I N G :  
                         s t d : : c o u t   < <   " S T R I N G   ( c a d e n a ) " ;  
                         b r e a k ;  
                 c a s e   C o l u m n T y p e : : C H A R :  
                         s t d : : c o u t   < <   " C H A R   ( c a r √ ° c t e r ) " ;  
                         b r e a k ;  
                 d e f a u l t :  
                         s t d : : c o u t   < <   " D E S C O N O C I D O " ;  
                         b r e a k ;  
         }  
         s t d : : c o u t   < <   s t d : : e n d l ;  
          
         / /   P A S O   3 :   A p l i c a r   e s t r a t e g i a   d e   s e l e c c i √ ≥ n   a u t o m √ ° t i c a  
         T i p o I n d i c e   t i p o _ s e l e c c i o n a d o   =   S e l e c c i o n a r T i p o I n d i c e A u t o m a t i c o ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
          
         s t d : : c o u t   < <   "  x}Ø   E s t r a t e g i a   s e l e c c i o n a d a :   " ;  
         s w i t c h   ( t i p o _ s e l e c c i o n a d o )   {  
                 c a s e   T i p o I n d i c e : : H A S H _ C A D E N A :  
                         s t d : : c o u t   < <   " H A S H   ( t a b l a   d e   l o n g i t u d   v a r i a b l e ) "   < <   s t d : : e n d l ;  
                         s t d : : c o u t   < <   "        x ù   R a z √ ≥ n :   L a s   t a b l a s   d e   l o n g i t u d   v a r i a b l e   S I E M P R E   u s a n   H a s h   p a r a   o p t i m i z a c i √ ≥ n "   < <   s t d : : e n d l ;  
                         b r e a k ;  
                 c a s e   T i p o I n d i c e : : B T R E E _ E N T E R O :  
                         s t d : : c o u t   < <   " B +   T R E E   E N T E R O   ( t a b l a   f i j a ,   c o l u m n a   I N T ) "   < <   s t d : : e n d l ;  
                         s t d : : c o u t   < <   "        x ù   R a z √ ≥ n :   T a b l a   f i j a   +   c o l u m n a   I N T   =   B +   T r e e   o p t i m i z a d o   p a r a   e n t e r o s "   < <   s t d : : e n d l ;  
                         b r e a k ;  
                 c a s e   T i p o I n d i c e : : B T R E E _ C A D E N A :  
                         s t d : : c o u t   < <   " B +   T R E E   C A D E N A   ( t a b l a   f i j a ,   c o l u m n a   S T R / C H A R ) "   < <   s t d : : e n d l ;  
                         s t d : : c o u t   < <   "        x ù   R a z √ ≥ n :   T a b l a   f i j a   +   c o l u m n a   t e x t o   =   S t r i n g   B +   T r e e "   < <   s t d : : e n d l ;  
                         b r e a k ;  
         }  
          
         / /   P A S O   4 :   C r e a r   e l   √ ≠ n d i c e   c o n   e l   t i p o   s e l e c c i o n a d o  
         s t d : : c o u t   < <   " \ n  x ®   C r e a n d o   √ ≠ n d i c e . . . "   < <   s t d : : e n d l ;  
         s t a t u s   =   C r e a r I n d i c e ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ,   t i p o _ s e l e c c i o n a d o ) ;  
          
         i f   ( s t a t u s   = =   S t a t u s : : O K )   {  
                 s t d : : c o u t   < <   " ‚ S&   √ ç n d i c e   c r e a d o   e x i t o s a m e n t e   c o n   e s t r a t e g i a   a u t o m √ ° t i c a "   < <   s t d : : e n d l ;  
                  
                 / /   P A S O   5 :   P e r s i s t e n c i a   a u t o m √ ° t i c a   s i   e s t √ °   h a b i l i t a d a  
                 i f   ( p e r s i s t e n c i a _ a u t o m a t i c a _ )   {  
                         s t d : : c o u t   < <   "  x æ   P e r s i s t i e n d o   √ ≠ n d i c e   a u t o m √ ° t i c a m e n t e . . . "   < <   s t d : : e n d l ;  
                         S t a t u s   s t a t u s _ p e r s i s t i r   =   P e r s i s t i r I n d i c e ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
                         i f   ( s t a t u s _ p e r s i s t i r   = =   S t a t u s : : O K )   {  
                                 s t d : : c o u t   < <   " ‚ S&   √ ç n d i c e   p e r s i s t i d o   a u t o m √ ° t i c a m e n t e   e n   d i s c o "   < <   s t d : : e n d l ;  
                         }   e l s e   {  
                                 s t d : : c o u t   < <   " ‚ a† Ô ∏ è   A d v e r t e n c i a :   E r r o r   a l   p e r s i s t i r   √ ≠ n d i c e   a u t o m √ ° t i c a m e n t e "   < <   s t d : : e n d l ;  
                         }  
                 }   e l s e   {  
                         s t d : : c o u t   < <   "  x ù   N o t a :   P e r s i s t e n c i a   a u t o m √ ° t i c a   d e s h a b i l i t a d a .   U s e   P e r s i s t i r I n d i c e ( )   m a n u a l m e n t e . "   < <   s t d : : e n d l ;  
                 }  
                  
                 / /   M o s t r a r   e s t a d √ ≠ s t i c a s   a c t u a l i z a d a s  
                 s t d : : c o u t   < <   " \ n  x `  E s t a d √ ≠ s t i c a s   a c t u a l i z a d a s : "   < <   s t d : : e n d l ;  
                 E s t a d i s t i c a s I n d i c e s   s t a t s   =   O b t e n e r E s t a d i s t i c a s ( ) ;  
                 s t d : : c o u t   < <   "       ‚ ¨ ¢   T o t a l   d e   √ ≠ n d i c e s :   "   < <   ( s t a t s . i n d i c e s _ b t r e e _ e n t e r o   +   s t a t s . i n d i c e s _ b t r e e _ c a d e n a   +   s t a t s . i n d i c e s _ h a s h _ c a d e n a )   < <   s t d : : e n d l ;  
                 s t d : : c o u t   < <   "       ‚ ¨ ¢   B +   T r e e   E n t e r o :   "   < <   s t a t s . i n d i c e s _ b t r e e _ e n t e r o   < <   s t d : : e n d l ;  
                 s t d : : c o u t   < <   "       ‚ ¨ ¢   B +   T r e e   C a d e n a :   "   < <   s t a t s . i n d i c e s _ b t r e e _ c a d e n a   < <   s t d : : e n d l ;  
                 s t d : : c o u t   < <   "       ‚ ¨ ¢   H a s h   C a d e n a :   "   < <   s t a t s . i n d i c e s _ h a s h _ c a d e n a   < <   s t d : : e n d l ;  
                  
         }   e l s e   {  
                 s t d : : c e r r   < <   " ‚ ù R  E r r o r   a l   c r e a r   √ ≠ n d i c e   a u t o m √ ° t i c o "   < <   s t d : : e n d l ;  
         }  
          
         s t d : : c o u t   < <   "  x ç   = = =   F I N   C R E A C I √  N   A U T O M √ Å T I C A   = = = "   < <   s t d : : e n d l ;  
         r e t u r n   s t a t u s ;  
 }  
  
 / * *  
   *   S e l e c c i o n a   a u t o m √ ° t i c a m e n t e   e l   t i p o   d e   √ ≠ n d i c e   s e g √ ∫ n   l a   e s t r a t e g i a   d e l   u s u a r i o  
   * /  
 T i p o I n d i c e   G e s t o r I n d i c e s : : S e l e c c i o n a r T i p o I n d i c e A u t o m a t i c o ( c o n s t   s t d : : s t r i n g &   n o m b r e _ t a b l a ,    
                                                                                                                     c o n s t   s t d : : s t r i n g &   n o m b r e _ c o l u m n a )   c o n s t   {  
         / /   R E G L A   1 :   T a b l a s   d e   l o n g i t u d   v a r i a b l e   S I E M P R E   u s a n   H a s h  
         i f   ( E s T a b l a L o n g i t u d V a r i a b l e ( n o m b r e _ t a b l a ) )   {  
                 r e t u r n   T i p o I n d i c e : : H A S H _ C A D E N A ;  
         }  
          
         / /   R E G L A   2 :   T a b l a s   d e   l o n g i t u d   f i j a   d e p e n d e n   d e l   t i p o   d e   c o l u m n a  
         C o l u m n T y p e   t i p o _ c o l u m n a   =   O b t e n e r T i p o C o l u m n a ( n o m b r e _ t a b l a ,   n o m b r e _ c o l u m n a ) ;  
          
         s w i t c h   ( t i p o _ c o l u m n a )   {  
                 c a s e   C o l u m n T y p e : : I N T :  
                         r e t u r n   T i p o I n d i c e : : B T R E E _ E N T E R O ;  
                          
                 c a s e   C o l u m n T y p e : : S T R I N G :  
                 c a s e   C o l u m n T y p e : : C H A R :  
                         r e t u r n   T i p o I n d i c e : : B T R E E _ C A D E N A ;  
                          
                 d e f a u l t :  
                         / /   P o r   d e f e c t o ,   u s a r   B +   T r e e   d e   c a d e n a   p a r a   t i p o s   d e s c o n o c i d o s  
                         s t d : : c o u t   < <   " ‚ a† Ô ∏ è   A d v e r t e n c i a :   T i p o   d e   c o l u m n a   d e s c o n o c i d o ,   u s a n d o   B +   T r e e   d e   c a d e n a   p o r   d e f e c t o "   < <   s t d : : e n d l ;  
                         r e t u r n   T i p o I n d i c e : : B T R E E _ C A D E N A ;  
         }  
 }  
  
 / * *  
   *   D e t e r m i n a   s i   u n a   t a b l a   e s   d e   l o n g i t u d   v a r i a b l e   c o n s u l t a n d o   e l   c a t √ ° l o g o  
   * /  
 b o o l   G e s t o r I n d i c e s : : E s T a b l a L o n g i t u d V a r i a b l e ( c o n s t   s t d : : s t r i n g &   n o m b r e _ t a b l a )   c o n s t   {  
         i f   ( ! g e s t o r _ c a t a l o g o _ )   {  
                 s t d : : c o u t   < <   " ‚ a† Ô ∏ è   A d v e r t e n c i a :   G e s t o r   d e   c a t √ ° l o g o   n o   d i s p o n i b l e .   U s a n d o   h e u r √ ≠ s t i c a   d e   n o m b r e s . "   < <   s t d : : e n d l ;  
                  
                 / /   H e u r √ ≠ s t i c a   t e m p o r a l :   t a b l a s   q u e   c o n t i e n e n   " _ v a r "   o   " _ v a r i a b l e "   s e   c o n s i d e r a n   d e   l o n g i t u d   v a r i a b l e  
                 r e t u r n   n o m b r e _ t a b l a . f i n d ( " _ v a r " )   ! =   s t d : : s t r i n g : : n p o s   | |    
                               n o m b r e _ t a b l a . f i n d ( " _ v a r i a b l e " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                               n o m b r e _ t a b l a . f i n d ( " v a r i a b l e " )   ! =   s t d : : s t r i n g : : n p o s ;  
         }  
          
         / /   T O D O :   I m p l e m e n t a r   c o n s u l t a   r e a l   a l   g e s t o r   d e   c a t √ ° l o g o   c u a n d o   e s t √ ©   d i s p o n i b l e  
         / /   P o r   a h o r a ,   u s a r   l a   h e u r √ ≠ s t i c a   d e   n o m b r e s   c o m o   r e s p a l d o  
         r e t u r n   n o m b r e _ t a b l a . f i n d ( " _ v a r " )   ! =   s t d : : s t r i n g : : n p o s   | |    
                       n o m b r e _ t a b l a . f i n d ( " _ v a r i a b l e " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                       n o m b r e _ t a b l a . f i n d ( " v a r i a b l e " )   ! =   s t d : : s t r i n g : : n p o s ;  
 }  
  
 / * *  
   *   O b t i e n e   e l   t i p o   d e   u n a   c o l u m n a   e s p e c √ ≠ f i c a   c o n s u l t a n d o   e l   c a t √ ° l o g o  
   * /  
 C o l u m n T y p e   G e s t o r I n d i c e s : : O b t e n e r T i p o C o l u m n a ( c o n s t   s t d : : s t r i n g &   n o m b r e _ t a b l a ,    
                                                                                           c o n s t   s t d : : s t r i n g &   n o m b r e _ c o l u m n a )   c o n s t   {  
         i f   ( ! g e s t o r _ c a t a l o g o _ )   {  
                 s t d : : c o u t   < <   " ‚ a† Ô ∏ è   A d v e r t e n c i a :   G e s t o r   d e   c a t √ ° l o g o   n o   d i s p o n i b l e .   U s a n d o   h e u r √ ≠ s t i c a   d e   n o m b r e s . "   < <   s t d : : e n d l ;  
         }  
          
         / /   T O D O :   I m p l e m e n t a r   c o n s u l t a   r e a l   a l   g e s t o r   d e   c a t √ ° l o g o   c u a n d o   e s t √ ©   d i s p o n i b l e  
         / /   P o r   a h o r a ,   u s a r   h e u r √ ≠ s t i c a   b a s a d a   e n   n o m b r e s   d e   c o l u m n a s  
         s t d : : s t r i n g   c o l u m n a _ l o w e r   =   n o m b r e _ c o l u m n a ;  
         s t d : : t r a n s f o r m ( c o l u m n a _ l o w e r . b e g i n ( ) ,   c o l u m n a _ l o w e r . e n d ( ) ,   c o l u m n a _ l o w e r . b e g i n ( ) ,   : : t o l o w e r ) ;  
          
         / /   D e t e c t a r   c o l u m n a s   d e   t i p o   I N T  
         i f   ( c o l u m n a _ l o w e r . f i n d ( " i d " )   ! =   s t d : : s t r i n g : : n p o s   | |    
                 c o l u m n a _ l o w e r . f i n d ( " a g e " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " e d a d " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " n u m e r o " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " c o u n t " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " c a n t i d a d " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " t o t a l " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " y e a r " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " a √ ± o " )   ! =   s t d : : s t r i n g : : n p o s )   {  
                 r e t u r n   C o l u m n T y p e : : I N T ;  
         }  
          
         / /   D e t e c t a r   c o l u m n a s   d e   t i p o   C H A R   ( g e n e r a l m e n t e   d e   l o n g i t u d   f i j a   c o r t a )  
         i f   ( c o l u m n a _ l o w e r . f i n d ( " c o d e " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " c o d i g o " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " s t a t u s " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " e s t a d o " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " t y p e " )   ! =   s t d : : s t r i n g : : n p o s   | |  
                 c o l u m n a _ l o w e r . f i n d ( " t i p o " )   ! =   s t d : : s t r i n g : : n p o s )   {  
                 r e t u r n   C o l u m n T y p e : : C H A R ;  
         }  
          
         / /   P o r   d e f e c t o ,   a s u m i r   S T R I N G  
         r e t u r n   C o l u m n T y p e : : S T R I N G ;  
 }  
  
 / * *  
   *   M √ © t o d o   d e   c o n v e n i e n c i a   p a r a   c r e a r   m √ ∫ l t i p l e s   √ ≠ n d i c e s   a u t o m √ ° t i c a m e n t e  
   * /  
 S t a t u s   G e s t o r I n d i c e s : : C r e a r I n d i c e s A u t o m a t i c o s P o r T a b l a ( c o n s t   s t d : : s t r i n g &   n o m b r e _ t a b l a ,    
                                                                                                               c o n s t   s t d : : v e c t o r < s t d : : s t r i n g > &   c o l u m n a s )   {  
         s t d : : c o u t   < <   " \ n  x ç   = = =   C R E A C I √  N   M A S I V A   D E   √ ç N D I C E S   A U T O M √ Å T I C O S   = = = "   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   "  x `  T a b l a :   "   < <   n o m b r e _ t a b l a   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   "  x 9   C o l u m n a s :   "   < <   c o l u m n a s . s i z e ( )   < <   s t d : : e n d l ;  
          
         S t a t u s   r e s u l t a d o _ g e n e r a l   =   S t a t u s : : O K ;  
         u i n t 3 2 _ t   i n d i c e s _ c r e a d o s   =   0 ;  
         u i n t 3 2 _ t   i n d i c e s _ f a l l i d o s   =   0 ;  
          
         f o r   ( c o n s t   a u t o &   c o l u m n a   :   c o l u m n a s )   {  
                 s t d : : c o u t   < <   " \ n - - -   P r o c e s a n d o   c o l u m n a :   "   < <   c o l u m n a   < <   "   - - - "   < <   s t d : : e n d l ;  
                 S t a t u s   s t a t u s   =   C r e a r I n d i c e A u t o m a t i c o ( n o m b r e _ t a b l a ,   c o l u m n a ) ;  
                  
                 i f   ( s t a t u s   = =   S t a t u s : : O K )   {  
                         i n d i c e s _ c r e a d o s + + ;  
                 }   e l s e   {  
                         i n d i c e s _ f a l l i d o s + + ;  
                         r e s u l t a d o _ g e n e r a l   =   S t a t u s : : E R R O R ;  
                 }  
         }  
          
         s t d : : c o u t   < <   " \ n  x `  = = =   R E S U M E N   D E   C R E A C I √  N   M A S I V A   = = = "   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   " ‚ S&   √ ç n d i c e s   c r e a d o s   e x i t o s a m e n t e :   "   < <   i n d i c e s _ c r e a d o s   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   " ‚ ù R  √ ç n d i c e s   f a l l i d o s :   "   < <   i n d i c e s _ f a l l i d o s   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   "  x ∆  T a s a   d e   √ © x i t o :   "   < <   ( i n d i c e s _ c r e a d o s   *   1 0 0 . 0   /   c o l u m n a s . s i z e ( ) )   < <   " % "   < <   s t d : : e n d l ;  
          
         r e t u r n   r e s u l t a d o _ g e n e r a l ;  
 }  
  
 / * *  
   *   C a r g a   a u t o m √ ° t i c a m e n t e   t o d o s   l o s   √ ≠ n d i c e s   p e r s i s t i d o s   e n   d i s c o   a l   i n i c i a r   e l   s i s t e m a  
   * /  
 S t a t u s   G e s t o r I n d i c e s : : C a r g a r I n d i c e s A u t o m a t i c a m e n t e ( )   {  
         s t d : : c o u t   < <   " \ n  x æ   = = =   C A R G A   A U T O M √ Å T I C A   D E   √ ç N D I C E S   D E S D E   D I S C O   = = = "   < <   s t d : : e n d l ;  
          
         i f   ( d i r e c t o r i o _ i n d i c e s _ . e m p t y ( ) )   {  
                 s t d : : c e r r   < <   " ‚ ù R  E r r o r :   D i r e c t o r i o   d e   √ ≠ n d i c e s   n o   c o n f i g u r a d o "   < <   s t d : : e n d l ;  
                 r e t u r n   S t a t u s : : E R R O R ;  
         }  
          
         / /   T O D O :   I m p l e m e n t a r   e s c a n e o   d e l   d i r e c t o r i o   d e   √ ≠ n d i c e s   y   c a r g a   a u t o m √ ° t i c a  
         / /   P o r   a h o r a ,   m o s t r a r   m e n s a j e   i n f o r m a t i v o  
         s t d : : c o u t   < <   "  x Å   D i r e c t o r i o   d e   √ ≠ n d i c e s :   "   < <   d i r e c t o r i o _ i n d i c e s _   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   "  x    E s c a n e a n d o   a r c h i v o s   d e   √ ≠ n d i c e s   e x i s t e n t e s . . . "   < <   s t d : : e n d l ;  
          
         / /   P l a c e h o l d e r   p a r a   l a   i m p l e m e n t a c i √ ≥ n   c o m p l e t a  
         s t d : : c o u t   < <   " ‚ a† Ô ∏ è   F u n c i o n a l i d a d   d e   c a r g a   a u t o m √ ° t i c a :   E N   D E S A R R O L L O "   < <   s t d : : e n d l ;  
         s t d : : c o u t   < <   "  x ù   U s e   C a r g a r I n d i c e ( )   m a n u a l m e n t e   p a r a   c a r g a r   √ ≠ n d i c e s   e s p e c √ ≠ f i c o s "   < <   s t d : : e n d l ;  
          
         r e t u r n   S t a t u s : : O K ;  
 }  
 