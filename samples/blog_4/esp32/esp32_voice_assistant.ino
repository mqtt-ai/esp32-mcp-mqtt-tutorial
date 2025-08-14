#include <WiFi.h>
#include <PubSubClient.h>
#include <SPIFFS.h>
#include <AudioFileSourceSPIFFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <driver/i2s.h>

// WiFi Configuration - Replace with your actual credentials
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

// MQTT Configuration - Using public EMQX broker
const char *mqtt_broker   = "broker.emqx.io";
const char *mqtt_upload_topic    = "emqx/esp32/audio";
const char *mqtt_download_topic  = "emqx/esp32/playaudio";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int   mqtt_port     = 1883;

// I2S Recording Configuration
#define I2S_SAMPLE_RATE   8000
#define I2S_SAMPLE_BITS   16
#define I2S_CHANNEL_NUM   1
#define I2S_REC_PORT      I2S_NUM_0

#define I2S_REC_BCLK  7
#define I2S_REC_LRCL  8
#define I2S_REC_DOUT  9

// I2S Playback Configuration
#define I2S_PLAY_PORT     I2S_NUM_1
#define I2S_PLAY_BCLK  2
#define I2S_PLAY_LRCL  1
#define I2S_PLAY_DOUT  42

#define RECORD_SECONDS    3
#define AUDIO_BUFFER_SIZE (I2S_SAMPLE_RATE * RECORD_SECONDS * 2)

// Voice Detection Parameters
#define ENERGY_THRESHOLD  350      // Voice detection threshold
#define DETECTION_WINDOW  2        // Consecutive detections needed
#define SILENCE_RESET     5        // Reset after silent samples

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// Audio playback objects
AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSPIFFS *file = nullptr;
AudioOutputI2S *out = nullptr;
const char* tempMp3Path = "/temp.mp3";

// System states
enum SystemState {
  STATE_IDLE,
  STATE_RECORDING,
  STATE_PLAYING,
  STATE_COOLDOWN
};

volatile SystemState currentState = STATE_IDLE;
volatile bool playbackRequested = false;
volatile bool stopRecording = false;

bool i2sRecordInitialized = false;
bool i2sPlayInitialized = false;

// Cooldown and anti-loop mechanism
unsigned long cooldownStartTime = 0;
const unsigned long COOLDOWN_DURATION = 3000;
const unsigned long POST_COOLDOWN_DELAY = 1000;
unsigned long postCooldownTime = 0;

unsigned long lastRecordingTime = 0;
const unsigned long MIN_RECORDING_INTERVAL = 5000;
int consecutiveDetections = 0;
const int MAX_CONSECUTIVE_DETECTIONS = 3;

// Pending audio data for playback
uint8_t* pendingAudioData = nullptr;
size_t pendingAudioLength = 0;

// Voice detection state
int detectionCount = 0;
int silenceCount = 0;

// WAV file header structure
typedef struct WAVHeader {
  char riff_header[4];
  uint32_t wav_size;
  char wave_header[4];
  char fmt_header[4];
  uint32_t fmt_chunk_size;
  uint16_t audio_format;
  uint16_t num_channels;
  uint32_t sample_rate;
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;
  char data_header[4];
  uint32_t data_bytes;
} WAVHeader;

/**
 * Build WAV header for audio data
 * @param header WAV header structure to fill
 * @param data_size Size of audio data in bytes
 */
void buildWavHeader(WAVHeader &header, uint32_t data_size) {
  memcpy(header.riff_header, "RIFF", 4);
  header.wav_size = data_size + sizeof(WAVHeader) - 8;
  memcpy(header.wave_header, "WAVE", 4);
  memcpy(header.fmt_header, "fmt ", 4);
  header.fmt_chunk_size = 16;
  header.audio_format = 1;
  header.num_channels = I2S_CHANNEL_NUM;
  header.sample_rate = I2S_SAMPLE_RATE;
  header.bits_per_sample = I2S_SAMPLE_BITS;
  header.byte_rate = I2S_SAMPLE_RATE * I2S_CHANNEL_NUM * I2S_SAMPLE_BITS / 8;
  header.block_align = I2S_CHANNEL_NUM * I2S_SAMPLE_BITS / 8;
  memcpy(header.data_header, "data", 4);
  header.data_bytes = data_size;
}

/**
 * Connect to WiFi network
 */
void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");
  Serial.printf("IP: %s, RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

/**
 * Clean up I2S recording resources
 */
void cleanupRecording() {
  if (i2sRecordInitialized) {
    i2s_driver_uninstall(I2S_REC_PORT);
    i2sRecordInitialized = false;
    Serial.println("Recording I2S cleaned up");
  }
}

/**
 * Clean up audio playback resources
 */
void cleanupPlayback() {
  if (mp3) {
    mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }
  if (file) {
    delete file;
    file = nullptr;
  }
  if (out) {
    out->stop();
    delete out;
    out = nullptr;
  }

  if (i2sPlayInitialized) {
    i2s_driver_uninstall(I2S_PLAY_PORT);
    i2sPlayInitialized = false;
    Serial.println("Playback I2S cleaned up");
  }
}

/**
 * MQTT message callback handler
 * @param topic MQTT topic
 * @param payload Message payload
 * @param length Payload length
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("MQTT message received: %s, %d bytes\n", topic, length);

  if (strcmp(topic, mqtt_download_topic) != 0) return;

  // Free previous pending audio data
  if (pendingAudioData) {
    free(pendingAudioData);
  }

  // Allocate memory for new audio data
  pendingAudioData = (uint8_t*)malloc(length);
  if (pendingAudioData) {
    memcpy(pendingAudioData, payload, length);
    pendingAudioLength = length;
    playbackRequested = true;

    // Interrupt current recording if active
    if (currentState == STATE_RECORDING) {
      stopRecording = true;
    }
  } else {
    Serial.println("Failed to allocate memory for audio data");
  }
}

/**
 * Connect to MQTT broker
 */
void connectToMQTT() {
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);

  while (!mqtt_client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "esp32-audio-client-" + WiFi.macAddress();

    if (mqtt_client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT broker");
      mqtt_client.subscribe(mqtt_download_topic);
      Serial.printf("Subscribed to: %s\n", mqtt_download_topic);
    } else {
      Serial.printf("Failed, rc=%d. Retry in 5 seconds.\n", mqtt_client.state());
      delay(5000);
    }
  }
}

/**
 * Initialize I2S for audio recording
 * @return true if successful, false otherwise
 */
bool initRecording() {
  if (i2sRecordInitialized) return true;

  cleanupPlayback();
  delay(50);

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_REC_BCLK,
    .ws_io_num = I2S_REC_LRCL,
    .data_out_num = -1,
    .data_in_num = I2S_REC_DOUT
  };

  esp_err_t err = i2s_driver_install(I2S_REC_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Record I2S driver install failed: %d\n", err);
    return false;
  }

  err = i2s_set_pin(I2S_REC_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Record I2S set pin failed: %d\n", err);
    i2s_driver_uninstall(I2S_REC_PORT);
    return false;
  }

  i2s_zero_dma_buffer(I2S_REC_PORT);
  i2sRecordInitialized = true;
  Serial.println("Recording I2S initialized");
  return true;
}

/**
 * Initialize I2S for audio playback
 * @return true if successful, false otherwise
 */
bool initPlayback() {
  // Complete cleanup first
  cleanupPlayback();
  cleanupRecording();
  delay(200);  // Wait for complete resource release

  i2s_config_t play_i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t play_pin_config = {
    .bck_io_num = I2S_PLAY_BCLK,
    .ws_io_num = I2S_PLAY_LRCL,
    .data_out_num = I2S_PLAY_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  esp_err_t err = i2s_driver_install(I2S_PLAY_PORT, &play_i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Play I2S driver install failed: %d\n", err);
    return false;
  }

  err = i2s_set_pin(I2S_PLAY_PORT, &play_pin_config);
  if (err != ESP_OK) {
    Serial.printf("Play I2S set pin failed: %d\n", err);
    i2s_driver_uninstall(I2S_PLAY_PORT);
    return false;
  }

  i2sPlayInitialized = true;
  Serial.println("Playback I2S initialized");
  return true;
}

/**
 * Calculate audio energy level for voice detection
 * @param samples Audio sample buffer
 * @param count Number of samples
 * @return Audio energy level
 */
uint32_t calculateAudioEnergy(int16_t *samples, size_t count) {
  uint64_t sum = 0;
  for (size_t i = 0; i < count; i++) {
    int32_t s = abs(samples[i]);  // Use absolute value
    sum += s;
  }
  return (uint32_t)(sum / count);  // Simple average of absolute values
}

/**
 * Perform audio recording and send to MQTT
 */
void performRecording() {
  currentState = STATE_RECORDING;
  stopRecording = false;

  size_t bytes_to_record = AUDIO_BUFFER_SIZE;
  int16_t *audio_buffer = (int16_t *)malloc(bytes_to_record);
  if (!audio_buffer) {
    Serial.println("Failed to allocate audio buffer");
    currentState = STATE_IDLE;
    return;
  }

  Serial.println("Recording started...");
  size_t total_bytes_read = 0;
  unsigned long recordStartTime = millis();

  // Record audio data
  while (total_bytes_read < bytes_to_record &&
         !stopRecording &&
         (millis() - recordStartTime) < (RECORD_SECONDS * 1000 + 500)) {

    size_t bytes_read = 0;
    size_t bytes_to_read = (total_bytes_read < 1024) ? 1024 : 512;
    if (bytes_to_read > (bytes_to_record - total_bytes_read)) {
      bytes_to_read = bytes_to_record - total_bytes_read;
    }

    esp_err_t result = i2s_read(I2S_REC_PORT,
                                (uint8_t *)audio_buffer + total_bytes_read,
                                bytes_to_read, &bytes_read, 100);

    if (result == ESP_OK && bytes_read > 0) {
      total_bytes_read += bytes_read;
    }

    mqtt_client.loop();
  }

  if (stopRecording) {
    Serial.println("Recording interrupted by playback request");
    free(audio_buffer);
    currentState = STATE_IDLE;
    return;
  }

  Serial.printf("Recording finished, captured %d bytes\n", total_bytes_read);

  // Build WAV file and send via MQTT
  if (total_bytes_read > 0) {
    size_t wav_size = sizeof(WAVHeader) + total_bytes_read;
    uint8_t *wav_buffer = (uint8_t *)malloc(wav_size);
    if (wav_buffer) {
      WAVHeader header;
      buildWavHeader(header, total_bytes_read);
      memcpy(wav_buffer, &header, sizeof(WAVHeader));
      memcpy(wav_buffer + sizeof(WAVHeader), audio_buffer, total_bytes_read);

      if (wav_size > mqtt_client.getBufferSize()) {
        mqtt_client.setBufferSize(wav_size + 1024);
      }

      if (mqtt_client.connected()) {
        bool result = mqtt_client.publish(mqtt_upload_topic, wav_buffer, wav_size);
        Serial.printf("Audio sent: %s\n", result ? "Success" : "Failed");
      }

      free(wav_buffer);
    }
  }

  free(audio_buffer);
  currentState = STATE_IDLE;
}

/**
 * Voice detection and recording trigger
 */
void detectVoiceAndRecord() {
  // Check minimum recording interval
  if (millis() - lastRecordingTime < MIN_RECORDING_INTERVAL) {
    return;
  }

  // Check consecutive detection limit
  if (consecutiveDetections >= MAX_CONSECUTIVE_DETECTIONS) {
    Serial.println("Too many consecutive recordings, brief pause");
    delay(2000);
    consecutiveDetections = 0;
    return;
  }

  if (!i2sRecordInitialized) {
    if (!initRecording()) {
      delay(1000);
      return;
    }
    // Clear initial buffers
    int16_t dummy_buf[512];
    size_t dummy_read;
    for (int i = 0; i < 3; i++) {
      i2s_read(I2S_REC_PORT, dummy_buf, sizeof(dummy_buf), &dummy_read, 50);
      delay(10);
    }
    // Reset detection state
    detectionCount = 0;
    silenceCount = 0;
  }

  // Check for voice trigger
  int16_t sample_buf[256];
  size_t bytes_read = 0;

  esp_err_t result = i2s_read(I2S_REC_PORT, sample_buf, sizeof(sample_buf), &bytes_read, 50);
  if (result != ESP_OK) {
    Serial.println("I2S read failed, reinitializing...");
    cleanupRecording();
    delay(100);
    return;
  }

  if (bytes_read > 0) {
    size_t sample_count = bytes_read / 2;
    uint32_t energy = calculateAudioEnergy(sample_buf, sample_count);

    // Debug output every 10 cycles
    static int debugCounter = 0;
    debugCounter++;
    if (debugCounter >= 10) {
      Serial.printf("Energy: %u, Threshold: %d, Count: %d/%d\n",
                   energy, ENERGY_THRESHOLD, detectionCount, DETECTION_WINDOW);
      debugCounter = 0;
    }

    // Voice detection logic
    if (energy > ENERGY_THRESHOLD) {
      detectionCount++;
      silenceCount = 0;

      if (detectionCount >= DETECTION_WINDOW) {
        Serial.printf("Voice detected! Energy: %u\n", energy);

        // Reset detection state
        detectionCount = 0;
        silenceCount = 0;

        // Update timing and counters
        lastRecordingTime = millis();
        consecutiveDetections++;

        performRecording();
      }
    } else {
      silenceCount++;
      if (silenceCount >= SILENCE_RESET) {
        detectionCount = 0;  // Reset detection count after silence
      }
    }
  }
}

/**
 * Play received MP3 audio
 */
void playAudio() {
  if (!pendingAudioData || pendingAudioLength == 0) {
    currentState = STATE_IDLE;
    return;
  }

  currentState = STATE_PLAYING;
  Serial.println("Starting audio playback...");

  // Save MP3 file to SPIFFS
  if (SPIFFS.exists(tempMp3Path)) {
    SPIFFS.remove(tempMp3Path);
  }

  File f = SPIFFS.open(tempMp3Path, FILE_WRITE);
  if (!f) {
    Serial.println("Failed to open SPIFFS file");
    free(pendingAudioData);
    pendingAudioData = nullptr;
    pendingAudioLength = 0;
    currentState = STATE_IDLE;
    return;
  }

  f.write(pendingAudioData, pendingAudioLength);
  f.close();

  // Free audio data memory
  free(pendingAudioData);
  pendingAudioData = nullptr;
  pendingAudioLength = 0;

  // Initialize playback
  if (!initPlayback()) {
    currentState = STATE_IDLE;
    return;
  }

  out = new AudioOutputI2S(I2S_PLAY_PORT);
  out->begin();

  file = new AudioFileSourceSPIFFS(tempMp3Path);
  mp3 = new AudioGeneratorMP3();

  if (!mp3->begin(file, out)) {
    Serial.println("Failed to start MP3 playback");
    cleanupPlayback();
    currentState = STATE_IDLE;
    return;
  }

  Serial.println("Playing audio...");

  // Playback loop
  while (mp3 && mp3->isRunning() && !playbackRequested) {
    if (!mp3->loop()) {
      break;
    }
    mqtt_client.loop();
    yield();
  }

  if (playbackRequested) {
    Serial.println("Playback interrupted by new audio");
  } else {
    Serial.println("Playback finished normally");
  }

  cleanupPlayback();
  currentState = STATE_IDLE;

  // Reset detection state after playback
  detectionCount = 0;
  silenceCount = 0;
  consecutiveDetections = 0;
}

/**
 * Arduino setup function
 */
void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("ESP32 Audio Record & Play System Starting...");

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    while (1) delay(1000);
  }
  Serial.println("SPIFFS Mounted");

  // Connect to WiFi
  connectToWiFi();

  // Setup MQTT
  mqtt_client.setBufferSize(262144);  // 256KB buffer
  mqtt_client.setKeepAlive(60);
  mqtt_client.setSocketTimeout(30);

  connectToMQTT();

  currentState = STATE_IDLE;
  Serial.println("System ready!");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

/**
 * Arduino main loop
 */
void loop() {
  // Maintain connections
  if (!mqtt_client.connected()) {
    Serial.println("MQTT reconnecting...");
    connectToMQTT();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi reconnecting...");
    connectToWiFi();
  }

  mqtt_client.loop();

  // Handle pending audio playback
  if (playbackRequested) {
    playbackRequested = false;

    // Stop current playback if active
    if (currentState == STATE_PLAYING) {
      cleanupPlayback();
    }

    playAudio();
  }

  // Voice detection and recording when idle
  if (currentState == STATE_IDLE && !playbackRequested) {
    detectVoiceAndRecord();
  }

  delay(10);  // Fast response
}