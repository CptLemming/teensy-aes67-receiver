#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <QNEthernet.h>
#include <TeensyID.h>

using namespace qindesign::network;

AudioPlayQueue           queue1;
AudioOutputI2S           i2s2;
AudioConnection          patchCord1(queue1, 0, i2s2, 0);
AudioConnection          patchCord2(queue1, 0, i2s2, 1);
AudioControlSGTL5000     sgtl5000_1;

IPAddress staticIP{192, 168, 30, 210};
IPAddress subnetMask{255, 255, 255, 0};
IPAddress gateway{192, 168, 30, 1};

IPAddress mcastip{239, 34, 13, 86}; // RPI
//IPAddress mcastip{239, 219, 130, 34}; // PC
int mport = 5004;

EthernetUDP udp;

byte mac[6];

uint8_t audioBuffer[256];
uint16_t audioBufferIndex = 0;
unsigned long timestamp = 0;
unsigned long lastTimestamp = 0;

void setup() {
  teensyMAC(mac);

  Serial.begin(115200);
  AudioMemory(128);

  stdPrint = &Serial;  // Make printf work (a QNEthernet feature)
  printf("Starting...\n");

  Ethernet.macAddress(mac);
  printf("MAC = %02x:%02x:%02x:%02x:%02x:%02x\n",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Ethernet.onLinkState([](bool state) {
    printf("[Ethernet] Link %s\n", state ? "ON" : "OFF");
  });
  printf("Starting Ethernet with static IP...\n");
  Ethernet.begin(staticIP, subnetMask, gateway);
  Serial.println(Ethernet.localIP());

  sgtl5000_1.enable();
  sgtl5000_1.volume(0.5);

  queue1.setBehaviour(AudioPlayQueue::NON_STALLING);
  queue1.setMaxBuffers(32);

  udp.beginMulticast(mcastip, mport);

  printf("Started! %d\n", AUDIO_BLOCK_SAMPLES);
  lastTimestamp = millis();
}

// L16 = size == 100 = 2
// L24 = size == 144 = 3
const unsigned short packetSize = 100;
uint8_t inputBuffer[packetSize];
void loop() {
  int size = udp.parsePacket();
  if (size < 1) {
    return;
  }
  udp.read(inputBuffer, packetSize);

//  printf("I see samples %d\n", size);
//
//  int samplesPerPacket = 44;
//  int numChannels = 1;
//  int rtpHeader = 12;
//  int bytesPerSample = 2;
//  int channelOne = 0;
//
  timestamp = millis();
  unsigned short sequenceNumber = (inputBuffer[2] << 8) | (inputBuffer[3] & 0xff);

  printf("Sequence %d   Time %d\n", sequenceNumber, timestamp - lastTimestamp);
//
//  for (int sample = 0; sample < samplesPerPacket; sample++) {
//    int channelOneIndex = (sample * numChannels + 0) * bytesPerSample + rtpHeader;
//    unsigned short channelOneAudio = (inputBuffer[channelOneIndex] << 8) | (inputBuffer[channelOneIndex + 1] & 0xff);
//    outputBuffer[sample] = channelOneAudio;
////    int channelTwoIndex = (sample * numChannels + 1) * bytesPerSample + rtpHeader;
////    unsigned short channelTwoAudio = (inputBuffer[channelTwoIndex] << 8) | (inputBuffer[channelTwoIndex + 1] & 0xff);
////    outputBuffer[sample * 2 + 1] = channelTwoAudio;
////    printf("Read %d, %d, %d\n", sample, channelOneIndex, channelTwoIndex);
//    printf("Read %d, %d\n", sample, channelOneIndex);
//  }

  for (int sample = 12; sample < packetSize; sample++) {
    audioBufferIndex += 1;

    if (audioBufferIndex >= 256) {
      audioBufferIndex = 0;
      int16_t *p=queue1.getBuffer();
      memcpy(p, audioBuffer, 256);
    }
    audioBuffer[audioBufferIndex] = inputBuffer[sample];
  }
  queue1.playBuffer();
}
