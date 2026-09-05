#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ================= WIFI =================
const char* ssid = "x8promax";
const char* password = "tiara0925";

// ================= TELEGRAM =================
String botToken = "8733518377:AAFVSPOGz913Q2Tdw4LXmih-QF6mrk2-X1I";
String chatID = "1696078708";

WiFiClientSecure client;

// ================= PIR =================
#define PIR_PIN 13

bool lastPirState = LOW;

// ================= CAMERA =================
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// ================= TELEGRAM UPDATE =================
int lastUpdateId = 0;
unsigned long lastTelegramCheck = 0;

// ================= DEBUG =================
void debug(String msg) {
  Serial.println("[DEBUG] " + msg);
}

// ================= WIFI =================
bool connectWiFi() {

  debug("Connecting WiFi...");

  WiFi.begin(ssid, password);

  int retry = 0;

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");

    retry++;

    if (retry > 40) {

      debug("❌ WiFi FAILED");
      return false;
    }
  }

  Serial.println();

  debug("✅ WiFi CONNECTED");

  Serial.println(WiFi.localIP());

  return true;
}

// ================= CAMERA =================
bool initCamera() {

  debug("Init Camera...");

  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;

  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  // ================= CAMERA SETTINGS =================

  config.xclk_freq_hz = 12000000;

  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_QVGA;

  config.jpeg_quality = 12;

  config.fb_count = 1;

  config.grab_mode = CAMERA_GRAB_LATEST;

  config.fb_location = CAMERA_FB_IN_PSRAM;

  // ================= INIT CAMERA =================

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {

    Serial.printf("Camera init failed: 0x%x\n", err);

    debug("❌ CAMERA FAILED");

    return false;
  }

  sensor_t *s = esp_camera_sensor_get();

  if (!s) {

    debug("❌ SENSOR FAILED");

    return false;
  }

  // ================= SENSOR SETTINGS =================

  s->set_framesize(s, FRAMESIZE_QVGA);

  s->set_brightness(s, 0);
  s->set_contrast(s, 0);
  s->set_saturation(s, 0);

  s->set_special_effect(s, 0);

  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);

  s->set_exposure_ctrl(s, 1);
  s->set_aec2(s, 0);
  s->set_ae_level(s, 0);

  s->set_gain_ctrl(s, 1);
  s->set_gainceiling(s, GAINCEILING_2X);

  s->set_bpc(s, 0);
  s->set_wpc(s, 1);
  s->set_raw_gma(s, 0);
  s->set_lenc(s, 1);

  s->set_hmirror(s, 0);
  s->set_vflip(s, 0);

  s->set_dcw(s, 0);
  s->set_colorbar(s, 0);

  debug("✅ CAMERA READY");

  return true;
}

// ================= SEND MESSAGE =================
void sendMessage(String text) {

  client.stop();

  if (!client.connect("api.telegram.org", 443)) {

    debug("❌ Telegram Connect Failed");

    return;
  }

  text.replace(" ", "%20");

  String url =
    "/bot" + botToken +
    "/sendMessage?chat_id=" + chatID +
    "&text=" + text;

  client.println("GET " + url + " HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Connection: close");
  client.println();

  while (client.connected()) {

    if (client.readStringUntil('\n') == "\r") break;
  }

  client.stop();
}

// ================= SEND PHOTO =================
void sendPhoto(String caption) {

  debug("📸 Ambil foto...");

  camera_fb_t * fb = esp_camera_fb_get();

  if (!fb) {

    debug("❌ Capture FAILED");

    return;
  }

  client.stop();

  if (!client.connect("api.telegram.org", 443)) {

    debug("❌ Telegram Connect Failed");

    esp_camera_fb_return(fb);

    return;
  }

  String boundary = "----ESP32";

  String head =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
    chatID + "\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" +
    caption + "\r\n"
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"photo\"; filename=\"cam.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t totalLen = fb->len + head.length() + tail.length();

  client.println("POST /bot" + botToken + "/sendPhoto HTTP/1.1");

  client.println("Host: api.telegram.org");

  client.println("Content-Type: multipart/form-data; boundary=" + boundary);

  client.println("Content-Length: " + String(totalLen));

  client.println();

  client.print(head);

  uint8_t *fbBuf = fb->buf;

  size_t fbLen = fb->len;

  for (size_t n = 0; n < fbLen; n += 1024) {

    size_t chunk = fbLen - n;

    if (chunk > 1024) chunk = 1024;

    client.write(fbBuf + n, chunk);
  }

  client.print(tail);

  esp_camera_fb_return(fb);

  debug("📤 Foto terkirim");

  while (client.connected()) {

    if (client.readStringUntil('\n') == "\r") break;
  }

  client.stop();
}

// ================= TELEGRAM COMMAND =================
void checkTelegram() {

  client.stop();

  if (!client.connect("api.telegram.org", 443)) {

    debug("❌ Telegram Connect Failed");

    return;
  }

  String url =
    "/bot" + botToken +
    "/getUpdates?offset=" +
    String(lastUpdateId + 1);

  client.println("GET " + url + " HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Connection: close");
  client.println();

  String response = "";

  while (client.connected() || client.available()) {

    if (client.available()) {

      char c = client.read();

      response += c;
    }
  }

  client.stop();

  // ================= UPDATE ID =================

  int updatePos = response.lastIndexOf("\"update_id\":");

  if (updatePos != -1) {

    int start = updatePos + 12;

    int end = response.indexOf(",", start);

    String idStr = response.substring(start, end);

    lastUpdateId = idStr.toInt();
  }

  // ================= STATUS =================

  if (response.indexOf("\"text\":\"status\"") != -1) {

    debug("STATUS command");

    sendMessage("📷 CAMERA READY");

    delay(1000);
  }

  // ================= PHOTO =================

  if (response.indexOf("\"text\":\"photo\"") != -1) {

    debug("PHOTO command");

    sendPhoto("📸 Manual Capture");

    delay(1000);
  }
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  delay(1000);

  debug("===== START =====");

  pinMode(PIR_PIN, INPUT);

  if (!connectWiFi()) return;

  client.setInsecure();

  client.setTimeout(5000);

  // ================= INIT CAMERA =================

  if (!initCamera()) return;

  // ================= FLASH =================
  // NYALAKAN SETELAH CAMERA READY

  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  // ================= PIR WARMING =================

  debug("Kalibrasi PIR (10 detik)...");

  delay(10000);

  debug("PIR siap");

  // ================= BOOT MESSAGE =================

  sendMessage("📷 ESP32 CAMERA READY");

  delay(1000);

  sendPhoto("Startup Camera");
}

// ================= LOOP =================
void loop() {

  // ================= PIR =================

  bool motion = digitalRead(PIR_PIN);

  // GERAKAN BARU
  if (motion == HIGH && lastPirState == LOW) {

    debug("🚨 Gerakan terdeteksi!");

    sendPhoto("🚨 Motion Detected!");
  }

  // SIMPAN STATUS PIR
  lastPirState = motion;

  // ================= TELEGRAM =================

  if (millis() - lastTelegramCheck > 3000) {

    checkTelegram();

    lastTelegramCheck = millis();
  }

  delay(1);
}