#include "arduinoFFT.h"

#define SAMPLES             512     // Must be a power of 2
#define SAMPLING_FREQUENCY  40000   // May need to be less than 10000 due to ADC

unsigned int sampling_period_us = round(1000000 / SAMPLING_FREQUENCY);
unsigned long timer_start;

double lReal[SAMPLES];      // left channel
double lImag[SAMPLES];

double rReal[SAMPLES];      // right channel
double rImag[SAMPLES];

double lpeak = 0.;          //  left channel peak frequency
double rpeak = 0.;          //  right channel peak frequency

arduinoFFT FFT = arduinoFFT();

void setup() {
  // start USB serial communication
  Serial.begin(115200);
  while(!Serial){ ; }
  Serial.println("Serial Ready!\n\n");

}

void loop() {
  audio_analysis();

}

void audio_analysis() {
  /* Sampling */
  for(int i=0; i<SAMPLES; i++) {
    timer_start = micros();

    lReal[i] = analogRead(A0);
    lImag[i] = 0;

    rReal[i] = analogRead(A1);
    rImag[i] = 0;

    while(micros() < (timer_start + sampling_period_us)) {}
  }

  /* FFT */
  FFT.Windowing(lReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(lReal, lImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(lReal, lImag, SAMPLES);
  lpeak = FFT.MajorPeak(lReal, SAMPLES, SAMPLING_FREQUENCY);

  FFT.Windowing(rReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(rReal, rImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(rReal, rImag, SAMPLES);
  rpeak = FFT.MajorPeak(rReal, SAMPLES, SAMPLING_FREQUENCY);
  
}