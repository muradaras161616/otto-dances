# otto-dances
Otto Robot Dance Database

## 📖 Proje Açıklaması

Bu proje, Otto Robot için online dans veritabanı içerir. ESP8266 tabanlı Otto robotlarınız için hazır dans hareketleri sunmaktadır.

## 🎭 Mevcut Danslar

| ID | İsim | İkon |
|---|---|---|
| gangnam | Gangnam Style | 🕺 |
| moonwalk | Moonwalk | 🌙 |
| floss | Floss | 🎮 |
| macarena | Macarena | 💃 |
| thriller | Thriller | 🧟 |
| russian | Russian Kick | 🦵 |
| chicken | Chicken Dance | 🐔 |
| dab | Dab | 🙆 |
| robot | Robot | 🤖 |
| shake | Shake | 🫨 |
| twerk | Twerk | 🍑 |
| wave | Wave | 👋 |
| disco | Disco | 🪩 |
| salsa | Salsa | 🌶️ |
| hype | Hype | 🔥 |

## 🚀 Kullanım Talimatları

### Dans Listesi Alma
```
GET /index.json
```

### Belirli Bir Dans Alma
```
GET /dances/{dance_id}.json
```

Örnek:
```
GET /dances/gangnam.json
```

## 📁 Dans Dosya Formatı

Her dans dosyası aşağıdaki JSON formatını kullanır:

```json
{
  "name": "Dans Adı",
  "repeat": 3,
  "steps": [
    [servo0, servo1, servo2, servo3, delay_ms],
    ...
  ]
}
```

### Alanlar

| Alan | Açıklama |
|---|---|
| name | Dansın görünen adı |
| repeat | Dansın kaç kez tekrarlanacağı |
| steps | Dans adımları dizisi |

### Step Formatı

Her step 5 değerden oluşur:
- `servo0`: Sol bacak servo açısı (0-180)
- `servo1`: Sağ bacak servo açısı (0-180)
- `servo2`: Sol ayak servo açısı (0-180)
- `servo3`: Sağ ayak servo açısı (0-180)
- `delay_ms`: Adım süresi (milisaniye)

## ➕ Yeni Dans Ekleme Rehberi

1. `dances/` klasöründe yeni bir JSON dosyası oluşturun (örn: `yeni_dans.json`)

2. Dans dosyasını yukarıdaki formata göre doldurun

3. `index.json` dosyasına yeni dansı ekleyin:
```json
{"id": "yeni_dans", "name": "Yeni Dans", "icon": "🎵"}
```

## 📡 ESP8266 Bağlantı Bilgisi

Otto robotunuzda bu veritabanını kullanmak için:

```cpp
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

const char* danceBaseUrl = "https://raw.githubusercontent.com/muradaras161616/otto-dances/main/";

// Dans listesini al
String getDanceList() {
  HTTPClient http;
  http.begin(String(danceBaseUrl) + "index.json");
  int httpCode = http.GET();
  String payload = http.getString();
  http.end();
  return payload;
}

// Belirli bir dansı al
String getDance(String danceId) {
  HTTPClient http;
  http.begin(String(danceBaseUrl) + "dances/" + danceId + ".json");
  int httpCode = http.GET();
  String payload = http.getString();
  http.end();
  return payload;
}
```

## 📜 Lisans

MIT License
