# 🏠 Smart Home Automation using ESP32 & Flask

> 🚀 An IoT-based Smart Home Automation System developed using ESP32, Flask, HTML, CSS, JavaScript, and SQLite.

---

# 📖 Project Overview

This project is a Smart Home Automation System that allows users to control electrical appliances through a web application.

The ESP32 acts as the IoT controller and communicates with the Flask web server over Wi-Fi. Users can securely log in, monitor devices, and control appliances from the dashboard. The project also supports voice commands for hands-free operation.

This project was developed as part of my Embedded Systems & IoT Internship.

---

# ✨ Features

✅ User Login & Registration

✅ Smart Dashboard

✅ ESP32 Wi-Fi Communication

✅ Relay-Based Appliance Control

✅ Light ON/OFF Control

✅ Fan ON/OFF Control

✅ Voice Command Support

✅ Responsive Web Interface

✅ SQLite Database

✅ Real-Time Device Control

---

# 🛠 Hardware Used

- ESP32 Development Board
- Relay Module
- 12V DC Fan
- LED Bulb
- Breadboard
- Jumper Wires
- USB Cable
- Power Supply

---

# 💻 Software Used

- Python
- Flask
- HTML5
- CSS3
- JavaScript
- SQLite
- Arduino IDE
- Git
- GitHub
- PyCharm

---

# 📂 Project Structure

```
Smart-Home-ESP32-Flask
│
├── 📁 ESP32_CODE
│   └── SmartHome.ino
│
├── 📁 templates
│   ├── login.html
│   ├── register.html
│   └── dashboard.html
│
├── 📁 static
│   ├── style.css
│   └── script.js
│
├── 🐍 app.py            # app + config, reads settings from env vars
├── 🐍 wsgi.py            # production entry point (gunicorn)
├── 📄 requirements.txt
├── 📄 .env.example       # copy to .env for local dev
├── 📄 Procfile            # for Render/Heroku-style platforms
├── 🐳 Dockerfile
├── 🐳 docker-compose.yml
└── 📘 README.md

Note: users.db is created automatically on first run and is git-ignored —
it is no longer committed to the repo.
```

---

# ⚙️ Working Principle

1️⃣ User opens the web application.

2️⃣ User logs into the system.

3️⃣ Flask receives the request.

4️⃣ Flask queues the command.

5️⃣ The ESP32 polls Flask every few seconds, picks up the command, and executes it.

6️⃣ Relay switches the appliance ON or OFF.

7️⃣ Device status is updated on the dashboard.

8️⃣ Users can also control devices using voice commands.

---

# 🚀 Local Development Setup

### 1️⃣ Clone the Repository

```bash
git clone https://github.com/VenkateshMS10/Smart-Home-ESP32-Flask.git
cd Smart-Home-ESP32-Flask
```

### 2️⃣ Create a virtual environment & install dependencies

```bash
python -m venv .venv
source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -r requirements.txt
```

### 3️⃣ Configure environment variables

```bash
cp .env.example .env
```

Edit `.env` and set at least `SECRET_KEY` and `ESP32_IP`. If you leave
`ADMIN_PASSWORD` unset, a random one-time password is generated for the
`admin` user and printed to the console log on first run — copy it from
there and change it after logging in.

### 4️⃣ Flash the ESP32 code

- Open `ESP32_CODE/SmartHome.ino` in Arduino IDE
- Install the **ArduinoJson** library (Library Manager)
- Set `WIFI_SSID`, `WIFI_PASSWORD`, `SERVER_URL` (your Flask app's URL —
  for local testing this can be `http://<your-computer's-LAN-IP>:5000`),
  and `DEVICE_API_KEY` (must match the server's `.env`)
- Select ESP32 Board + COM Port, then upload

The ESP32 doesn't run its own web server anymore — it polls your Flask
app every couple of seconds, so it just needs internet/Wi-Fi access, not
a reachable IP.

### 5️⃣ Run Flask (dev server)

```bash
python app.py
```

### 6️⃣ Open Browser

```
http://127.0.0.1:5000
```

---

# 🌐 Deployment

The app is production-ready via [Gunicorn](https://gunicorn.org/) and reads
all sensitive config from environment variables — nothing is hardcoded.

**Architecture:** Flask never calls the ESP32 directly. Instead, the ESP32
polls Flask every couple of seconds (`POST /device/sync`), reporting its
current fan/light state and receiving the next queued command in the same
response. This means Flask can be deployed to any public cloud host — the
ESP32 just needs outbound internet access, which is normally allowed by
home routers with zero configuration (no port forwarding, no static IP,
no VPN required).

### Option A — Docker

```bash
cp .env.example .env   # fill in SECRET_KEY, DEVICE_API_KEY, ADMIN_PASSWORD, etc.
docker compose up --build -d
```

This builds the image, runs it with Gunicorn on port 5000, and persists
`users.db` in a named volume so it survives container restarts.

### Option B — Any platform with a Procfile (Render, Railway, Heroku, etc.)

1. Push the repo (the included `Procfile` runs
   `gunicorn -w 2 -b 0.0.0.0:$PORT wsgi:app`).
2. Set these environment variables in the platform's dashboard:
   - `SECRET_KEY` — generate with `python -c "import secrets; print(secrets.token_hex(32))"`
   - `DEVICE_API_KEY` — generate with `python -c "import secrets; print(secrets.token_hex(16))"`; paste the same value into `DEVICE_API_KEY` in `SmartHome.ino`
   - `ADMIN_USERNAME` / `ADMIN_PASSWORD` — seed login for the first deploy
   - `SESSION_COOKIE_SECURE=true` (default) since the platform serves HTTPS
3. Deploy, note the public URL, and put it in `SERVER_URL` in the firmware.
4. Flash the ESP32 and power it on — it will start polling within seconds.

### Option C — Bare VM / server

```bash
pip install -r requirements.txt
export SECRET_KEY=... DEVICE_API_KEY=... ADMIN_PASSWORD=...
gunicorn -w 2 -b 0.0.0.0:5000 wsgi:app
```

Put this behind Nginx/Caddy for TLS, or run it directly behind a load
balancer that terminates HTTPS.

### Flashing the ESP32

In `ESP32_CODE/SmartHome.ino`, set:
- `WIFI_SSID` / `WIFI_PASSWORD` — your home Wi-Fi
- `SERVER_URL` — your deployed Flask URL, e.g. `https://your-app.onrender.com`
- `DEVICE_API_KEY` — must exactly match the server's `DEVICE_API_KEY`

The firmware requires the **ArduinoJson** library (install via Arduino IDE
Library Manager). It reports state and checks for commands every
`POLL_INTERVAL_MS` (2s by default) — lower this for snappier response,
raise it to reduce data usage.

### Production checklist

- [ ] `SECRET_KEY` set to a real random value (not regenerated on every restart)
- [ ] `DEVICE_API_KEY` set explicitly and matches the value flashed into the ESP32
- [ ] `ADMIN_PASSWORD` set explicitly, or the generated one rotated after first login
- [ ] `FLASK_DEBUG` unset or `false`
- [ ] Served over HTTPS, with `SESSION_COOKIE_SECURE=true` (default)
- [ ] `users.db` stored on a persistent volume, not the container's ephemeral filesystem
- [ ] A `GET /healthz` check configured with your host/uptime monitor
- [ ] Firmware's `secureClient.setInsecure()` replaced with a pinned root CA before long-term production use

Note: `/fan/*`, `/light/*`, and `/esp32/status` require an authenticated
browser session. `/device/sync` requires the separate `DEVICE_API_KEY` —
neither can be triggered by an anonymous visitor.

---

# 🎤 Voice Control

Example Commands:

🗣️ Turn on light

🗣️ Turn off light

🗣️ Turn on fan

🗣️ Turn off fan

---

# 📸 Project Demo

🎥 **Project demonstration video will be added soon.**

---

# 🔮 Future Improvements

- 📱 Android Application
- ☁ Firebase Integration
- 📡 MQTT Communication
- 🌡 Temperature & Humidity Sensors
- 📷 Camera Monitoring
- 🤖 AI Assistant
- 🏡 Alexa Integration
- 🎙 Google Assistant Integration

---

# 👨‍💻 Author

## Venkatesh M S

🎓 Embedded Systems & IoT Enthusiast

💻 Passionate about IoT, Embedded Systems, Python, ESP32, Flask, and Smart Home Automation.

🌐 GitHub

https://github.com/VenkateshMS10

---

# ⭐ If you like this project

Please consider giving this repository a ⭐ Star on GitHub.

It motivates me to build more exciting IoT and Embedded Systems projects.

---

# 📜 License

This project is developed for educational and learning purposes.
