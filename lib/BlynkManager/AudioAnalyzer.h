#ifndef AUDIO_ANALYZER_H
#define AUDIO_ANALYZER_H

#include <Arduino.h>
#include "arduinoFFT.h"
#include "Config.h"

struct AudioResult {
    float totalEnergy;
    float energyDoorbell;
    float energySmoke;
    float ratioDoorbell;
    float ratioSmoke;
    float* spectrum;
    bool isValid;
    
    AudioResult() : totalEnergy(0), energyDoorbell(0), energySmoke(0),
                    ratioDoorbell(0), ratioSmoke(0), spectrum(nullptr), isValid(false) {}
};

class AudioAnalyzer {
public:
    AudioAnalyzer();
    bool begin();
    AudioResult analyze();
    float* getSpectrum();
    
private:
    arduinoFFT m_fft;
    double m_vReal[SAMPLES_FFT];
    double m_vImag[SAMPLES_FFT];
    int16_t m_sampleBuffer[BUFFER_SIZE];
    float m_spectrum[12];  // 12 frequency bands for display
    
    void readI2S();
    void performFFT();
    void computeBandEnergies(AudioResult& result);
    void computeSpectrumBars();
    void initI2SMicrophone();
};

#endif // AUDIO_ANALYZER_H