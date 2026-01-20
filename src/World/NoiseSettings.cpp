#include "NoiseSettings.h"

NoiseSettings::NoiseSettings(float _frequency, float _amplitude, float _offset)
    : amplitude(_amplitude), frequency(_frequency), offset(_offset), chance(0), block(0), maxHeight(0) {
}

NoiseSettings::NoiseSettings(float _frequency, float _amplitude, float _offset, float _chance, unsigned int _block, int _maxHeight)
    : amplitude(_amplitude), frequency(_frequency), offset(_offset), chance(_chance), block(_block), maxHeight(_maxHeight){}

NoiseSettings::~NoiseSettings()= default;
