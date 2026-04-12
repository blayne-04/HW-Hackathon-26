#include "AudioAnalyzer.h"
#include <driver/i2s.h>

AudioAnalyzer::AudioAnalyzer() : m_fft() {
    memset(m_vReal, 0, sizeof(m_vReal));
    memset(m_vImag, 0, sizeof(m_vImag));
    memset(m_sampleBuffer, 0, sizeof(m_sampleBuffer));
    memset(m_spectrum, 0, sizeof(m_spectrum));
}

bool AudioAnalyzer::begin() {
    initI2SMicrophone();
    Serial.println("Audio Analyzer initialized");
    return true;
}

void AudioAnalyzer::initI2SMicrophone() {
    i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };
    
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

void AudioAnalyzer::readI2S() {
    size_t bytesRead;
    i2s_read(I2S_NUM_0, m_sampleBuffer, sizeof(m_sampleBuffer), &bytesRead, portMAX_DELAY);
    int samplesRead = bytesRead / sizeof(int16_t);
    
    if (samplesRead < SAMPLES_FFT) return;
    
    for (int i = 0; i < SAMPLES_FFT; i++) {
        m_vReal[i] = (double)m_sampleBuffer[i];
        m_vImag[i] = 0.0;
    }
}

void AudioAnalyzer::performFFT() {
    m_fft.Windowing(m_vReal, SAMPLES_FFT, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    m_fft.Compute(m_vReal, m_vImag, SAMPLES_FFT, FFT_FORWARD);
    m_fft.ComplexToMagnitude(m_vReal, m_vImag, SAMPLES_FFT);
}

void AudioAnalyzer::computeBandEnergies(AudioResult& result) {
    result.totalEnergy = 0;
    result.energyDoorbell = 0;
    result.energySmoke = 0;
    
    int binWidth = SAMPLE_RATE / SAMPLES_FFT;
    
    for (int i = 1; i < SAMPLES_FFT / 2; i++) {
        float freq = i * binWidth;
        float magnitude = m_vReal[i];
        
        result.totalEnergy += magnitude;
        
        if (freq >= DOORBELL_FREQ_MIN && freq <= DOORBELL_FREQ_MAX) {result.energyDoorbell += magnitude;}
        if (freq >= SMOKE_FREQ_MIN && freq <= SMOKE_FREQ_MAX) {result.energySmoke += magnitude;}
    }
    
    if (result.totalEnergy > 0) {
        result.ratioDoorbell = result.energyDoorbell / result.totalEnergy;
        result.ratioSmoke = result.energySmoke / result.totalEnergy;
    }
    
    result.isValid = result.totalEnergy > MIN_TOTAL_ENERGY;
}

void AudioAnalyzer::computeSpectrumBars() {
    int binsPerBar = (SAMPLES_FFT / 2) / 12;
    
    for (int i = 0; i < 12; i++) {
        float sum = 0;
        int startBin = i * binsPerBar;
        int endBin = (i + 1) * binsPerBar;
        
        for (int j = startBin; j < endBin && j < SAMPLES_FFT / 2; j++) {sum += m_vReal[j];}
        
        m_spectrum[i] = sum / binsPerBar;
    }
}

AudioResult AudioAnalyzer::analyze() {
    AudioResult result;
    
    readI2S();
    performFFT();
    computeBandEnergies(result);
    computeSpectrumBars();
    
    result.spectrum = m_spectrum;
    
    return result;
}

float* AudioAnalyzer::getSpectrum() {
    return m_spectrum;
}