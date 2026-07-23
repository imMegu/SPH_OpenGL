// gyro_source.h
//
// Polls Phyphox's gyroscope over HTTP on a background thread, and exposes
// the latest angular velocities + a deltaTime for the render loop to
// consume without blocking on network I/O.

#pragma once
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

class GyroSource {
public:
  GyroSource(std::string phone_ip, std::string port)
      : base_url_("http://" + phone_ip + ":" + port) {}

  ~GyroSource() { stop(); }

  void start() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    running_ = true;
    worker_ = std::thread(&GyroSource::pollLoop, this);
  }

  void stop() {
    running_ = false;
    if (worker_.joinable())
      worker_.join();
    curl_global_cleanup();
  }

  // Call this once per frame from your render loop.
  // Returns the gyro deltas (rad) accumulated since the last call, and
  // resets the accumulator -- so consuming it also "consumes" the motion.
  void consume(float &dPitchRad, float &dYawRad, float &dRollRad) {
    std::lock_guard<std::mutex> lock(mutex_);
    dPitchRad = accumPitch_;
    dYawRad = accumYaw_;
    dRollRad = accumRoll_;
    accumPitch_ = accumYaw_ = accumRoll_ = 0.0f;
  }

private:
  static size_t writeCallback(char *ptr, size_t size, size_t nmemb,
                              void *userdata) {
    auto *out = static_cast<std::string *>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
  }

  std::string httpGet(CURL *curl, const std::string &url) {
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
    curl_easy_perform(
        curl); // ignore transient errors; we just skip that sample
    return response;
  }

  void pollLoop() {
    CURL *curl = curl_easy_init();
    if (!curl)
      return;
    httpGet(curl, base_url_ + "/control?cmd=start");

    auto lastTime = std::chrono::steady_clock::now();

    while (running_) {
      auto now = std::chrono::steady_clock::now();
      float dt = std::chrono::duration<float>(now - lastTime).count();
      lastTime = now;

      std::string body = httpGet(curl, base_url_ + "/get?gyrX&gyrY&gyrZ");
      nlohmann::json j = nlohmann::json::parse(body, nullptr, false);
      if (!j.is_discarded()) {
        try {
          auto &buf = j["buffer"];
          float gx = buf["gyrX"]["buffer"][0].get<float>();
          float gy = buf["gyrY"]["buffer"][0].get<float>();
          float gz = buf["gyrZ"]["buffer"][0].get<float>();

          std::lock_guard<std::mutex> lock(mutex_);
          accumPitch_ += gx * dt; // tilt (X) -> pitch
          accumYaw_ += gy * dt;   // twist about vertical (Y) -> yaw
          accumRoll_ += gz * dt;  // twist about screen-normal (Z) -> roll
        } catch (...) {
          // malformed/partial response; skip this sample
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(20)); // ~50 Hz poll
    }
    curl_easy_cleanup(curl);
  }

  std::string base_url_;
  std::thread worker_;
  std::atomic<bool> running_{false};

  std::mutex mutex_;
  float accumPitch_ = 0.0f;
  float accumYaw_ = 0.0f;
  float accumRoll_ = 0.0f;
};
