#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <portaudio.h>
#include <limits> 
#include <sndfile.h> // Para guardar en formato WAV

// --- Constantes de configuración ---
const int SAMPLE_RATE = 16000;          // Frecuencia de muestreo en Hz
const int FRAMES_PER_BUFFER = 512;      // Número de frames por buffer
const int NUM_CHANNELS = 1;             // 1 para mono, 2 para estéreo
const PaSampleFormat SAMPLE_FORMAT = paInt16; // Formato de sampleo: 16-bit signed integer
const char* OUTPUT_FILENAME = "grabacion.wav"; // Nombre del archivo de salida

// --- Estructura para almacenar los datos de audio ---
struct AudioData {
    std::vector<int16_t> buffer;
};

// --- Función de callback de PortAudio para el stream de entrada ---
// Esta función se llama automáticamente cuando hay nuevos datos de audio disponibles.
static int recordCallback(const void* inputBuffer, void* outputBuffer,unsigned long framesPerBuffer,const PaStreamCallbackTimeInfo* timeInfo,PaStreamCallbackFlags statusFlags,void* userData) {
    // Recuperar el puntero a nuestra estructura AudioData
    AudioData* data = static_cast<AudioData*>(userData);
    // Convertir el buffer de entrada al tipo de datos correcto (int16_t)
    const int16_t* in = static_cast<const int16_t*>(inputBuffer);

    // Asegurarse de que el buffer de entrada no sea nulo (aunque en PortAudio no debería serlo para streams de entrada)
    if (in != nullptr) {
        // Calcular el número total de samples (frames * canales)
        size_t numSamplesToInsert = framesPerBuffer * NUM_CHANNELS;
        // Añadir los samples del buffer de entrada al vector de audioData
        data->buffer.insert(data->buffer.end(), in, in + numSamplesToInsert);
    }

    // Indicar a PortAudio que el stream debe continuar
    return paContinue;
}

// --- Función para guardar los datos de audio en un archivo WAV usando libsndfile ---
bool saveWaveFile(const char* filename, const std::vector<int16_t>& buffer, int sampleRate, int numChannels) {
    SF_INFO sfinfo;
    sfinfo.samplerate = sampleRate;
    sfinfo.channels = numChannels;
    // Combinar el formato WAV con PCM de 16 bits
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    // Abrir el archivo para escritura
    SNDFILE* outfile = sf_open(filename, SFM_WRITE, &sfinfo);
    if (outfile == nullptr) {
        std::cerr << "Error al abrir el archivo para escritura (" << filename << "): " << sf_strerror(nullptr) << std::endl;
        std::cerr << "Asegúrate de que libsndfile esté correctamente instalado y accesible." << std::endl;
        return false;
    }

    // Escribir los samples de audio al archivo
    // sf_write_short se usa para datos de 16 bits
    sf_count_t frames_written = sf_write_short(outfile, buffer.data(), buffer.size());

    // Verificar si todos los samples fueron escritos
    if (frames_written != buffer.size()) {
        std::cerr << "Advertencia: No se escribieron todos los samples al archivo. Esperado: "
                  << buffer.size() << ", Escrito: " << frames_written << std::endl;
        std::cerr << "Error de escritura: " << sf_strerror(outfile) << std::endl;
        sf_close(outfile); // Asegurarse de cerrar el archivo en caso de error
        return false;
    }

    // Cerrar el archivo
    if (sf_close(outfile) != 0) {
        std::cerr << "Error al cerrar el archivo (" << filename << "): " << sf_strerror(nullptr) << std::endl;
        return false;
    }

    std::cout << "Audio guardado exitosamente en: " << filename << std::endl;
    return true;
}

// --- Función principal ---
int main() {
    PaError err = paNoError; // Inicializar err a paNoError
    PaStream* stream = nullptr; // Inicializar stream a nullptr para un manejo seguro

    // 1. Inicializar PortAudio
    err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "Error de inicialización de PortAudio: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    // 2. Obtener y listar los dispositivos de entrada disponibles
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        std::cerr << "Error al obtener la cuenta de dispositivos de PortAudio: " << Pa_GetErrorText(numDevices) << std::endl;
        Pa_Terminate();
        return 1;
    }
    if (numDevices == 0) {
        std::cerr << "Error: No se encontraron dispositivos de audio de PortAudio." << std::endl;
        Pa_Terminate();
        return 1;
    }

    std::cout << "Dispositivos de entrada de PortAudio disponibles:" << std::endl;
    int inputDeviceCount = 0;
    // vector para almacenar solo los índices de dispositivos de entrada válidos
    std::vector<int> inputDeviceIndices;

    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo != nullptr && deviceInfo->maxInputChannels > 0) {
            std::cout << "[" << i << "] " << deviceInfo->name << std::endl;
            inputDeviceCount++;
            inputDeviceIndices.push_back(i); // Almacenar el índice real del dispositivo
        }
    }

    if (inputDeviceCount == 0) {
        std::cerr << "Error: No se encontraron dispositivos de entrada de audio." << std::endl;
        Pa_Terminate();
        return 1;
    }

    // 3. Permitir al usuario seleccionar un dispositivo
    int selectedDeviceIndex = -1;
    std::cout << "Ingrese el índice del dispositivo de entrada a usar: ";
    std::cin >> selectedDeviceIndex;

    // Consumir el resto de la línea después de leer el entero para evitar problemas con std::cin.get()
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // Validar la selección del usuario
    bool isValidSelection = false;
    for (int idx : inputDeviceIndices) {
        if (idx == selectedDeviceIndex) {
            isValidSelection = true;
            break;
        }
    }

    if (!isValidSelection) {
        std::cerr << "Índice de dispositivo seleccionado inválido o no es un dispositivo de entrada." << std::endl;
        Pa_Terminate();
        return 1;
    }

    const PaDeviceInfo* selectedDeviceInfo = Pa_GetDeviceInfo(selectedDeviceIndex);
    std::cout << "Usando el dispositivo de entrada: " << selectedDeviceInfo->name << std::endl;
    std::cout << "Frecuencia de muestreo nativa por defecto: " << selectedDeviceInfo->defaultSampleRate << " Hz" << std::endl;
    std::cout << "Formatos de sampleo comúnmente soportados (puede variar): Int16, Float32" << std::endl;

    if (selectedDeviceInfo->maxInputChannels < NUM_CHANNELS) {
        std::cerr << "Error: El dispositivo de entrada seleccionado no soporta " << NUM_CHANNELS << " canales." << std::endl;
        Pa_Terminate();
        return 1;
    }

    // 4. Configurar parámetros del stream de PortAudio
    AudioData audioData; // Objeto para almacenar los datos de audio grabados

    PaStreamParameters inputParameters;
    inputParameters.device = selectedDeviceIndex;
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.sampleFormat = SAMPLE_FORMAT;
    inputParameters.suggestedLatency = selectedDeviceInfo->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    // 5. Abrir el stream de PortAudio
    err = Pa_OpenStream(
        &stream,
        &inputParameters,
        nullptr, // No stream de salida (solo grabación)
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff, // No necesitamos clipping, ya que estamos capturando
        recordCallback, // Nuestro callback para procesar el audio
        &audioData);    // Datos de usuario que se pasan al callback
    if (err != paNoError) {
        std::cerr << "Error al abrir el stream: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        return 1;
    }

    // 6. Iniciar el stream de grabación
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Error al iniciar el stream: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream); // Intentar cerrar el stream si no se pudo iniciar
        Pa_Terminate();
        return 1;
    }

    std::cout << "Grabando audio... Presione Enter para detener." << std::endl;
    std::cin.get(); // Esperar a que el usuario presione Enter

    // 7. Detener el stream
    err = Pa_StopStream(stream);
    if (err != paNoError) {
        std::cerr << "Error al detener el stream: " << Pa_GetErrorText(err) << std::endl;
    }

    // 8. Cerrar el stream
    err = Pa_CloseStream(stream);
    if (err != paNoError) {
        std::cerr << "Error al cerrar el stream: " << Pa_GetErrorText(err) << std::endl;
    }

    // 9. Terminar PortAudio
    Pa_Terminate();

    std::cout << "Grabación de audio finalizada." << std::endl;
    std::cout << "Tamaño total del buffer capturado: " << audioData.buffer.size() << " samples." << std::endl;

    // 10. Guardar el audio en un archivo WAV si se capturó algo
    if (!audioData.buffer.empty()) {
        if (saveWaveFile(OUTPUT_FILENAME, audioData.buffer, SAMPLE_RATE, NUM_CHANNELS)) {
            // Mensaje ya impreso por saveWaveFile
        } else {
            std::cerr << "Error al guardar el archivo WAV." << std::endl;
        }

        // Mostrar algunos samples y el rango de valores
        size_t printCount = std::min((size_t)100, audioData.buffer.size());
        std::cout << "Primeros " << printCount << " samples: ";
        for (size_t i = 0; i < printCount; ++i) {
            std::cout << audioData.buffer[i] << " ";
        }
        std::cout << (audioData.buffer.size() > printCount ? "..." : "") << std::endl;

        int16_t minVal = std::numeric_limits<int16_t>::max();
        int16_t maxVal = std::numeric_limits<int16_t>::min();
        if (!audioData.buffer.empty()) { // Asegurarse de que el buffer no esté vacío antes de buscar min/max
            for (const auto& sample : audioData.buffer) {
                minVal = std::min(minVal, sample);
                maxVal = std::max(maxVal, sample);
            }
        }
        std::cout << "Valor mínimo del sample: " << minVal << std::endl;
        std::cout << "Valor máximo del sample: " << maxVal << std::endl;
    } else {
        std::cout << "No se capturó audio (el buffer está vacío)." << std::endl;
    }

    return 0;
}