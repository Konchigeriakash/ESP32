import os
import time
import secrets
import sqlite3
import logging

from flask import Flask, render_template, request, redirect, session, jsonify

try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass

from werkzeug.security import generate_password_hash, check_password_hash

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("smarthome")

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def str_to_bool(value, default=False):
    if value is None:
        return default
    return value.strip().lower() in ("1", "true", "yes", "on")


class Config:
    # SECRET_KEY must be set in production. A random one is generated for
    # local/dev runs so the app still boots, but it changes on every
    # restart (which logs everyone out), so always set it explicitly
    # when deploying.
    SECRET_KEY = os.environ.get("SECRET_KEY") or secrets.token_hex(32)

    DATABASE = os.environ.get("DATABASE_PATH", os.path.join(BASE_DIR, "users.db"))

    DEBUG = str_to_bool(os.environ.get("FLASK_DEBUG"), default=False)

    # Only send session cookies over HTTPS in production. Set
    # SESSION_COOKIE_SECURE=false only for local HTTP development.
    SESSION_COOKIE_SECURE = str_to_bool(os.environ.get("SESSION_COOKIE_SECURE"), default=not DEBUG)
    SESSION_COOKIE_HTTPONLY = True
    SESSION_COOKIE_SAMESITE = "Lax"

    # Seed admin credentials, only used the first time the DB is created.
    ADMIN_USERNAME = os.environ.get("ADMIN_USERNAME", "admin")
    ADMIN_PASSWORD = os.environ.get("ADMIN_PASSWORD")  # if unset, a random one is generated

    # Shared secret the ESP32 firmware sends on every poll/report call.
    # This is NOT a user password -- it's how the server tells "my real
    # ESP32" apart from anyone else on the internet hitting these routes.
    DEVICE_API_KEY = os.environ.get("DEVICE_API_KEY")

    # If the ESP32 hasn't reported in for longer than this, it's shown as offline.
    DEVICE_OFFLINE_AFTER = float(os.environ.get("DEVICE_OFFLINE_AFTER", "10"))


def create_app(config_object=Config):
    app = Flask(__name__)
    app.config.from_object(config_object)

    if not app.config["DEVICE_API_KEY"]:
        app.config["DEVICE_API_KEY"] = secrets.token_hex(16)
        logger.warning(
            "No DEVICE_API_KEY set. Generated a temporary one for this "
            "process: %s -- this will change on every restart, which will "
            "lock out your ESP32. Set DEVICE_API_KEY explicitly and flash "
            "it into the firmware.",
            app.config["DEVICE_API_KEY"]
        )

    with app.app_context():
        init_db(app)

    register_routes(app)

    return app


# ==========================
# DATABASE
# ==========================

def get_db(app):
    conn = sqlite3.connect(app.config["DATABASE"])
    conn.row_factory = sqlite3.Row
    return conn


def init_db(app):
    conn = get_db(app)
    cur = conn.cursor()

    cur.execute("""
    CREATE TABLE IF NOT EXISTS users(
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT UNIQUE NOT NULL,
        password TEXT NOT NULL
    )
    """)

    # Single-row table holding the ESP32's last self-reported state.
    cur.execute("""
    CREATE TABLE IF NOT EXISTS device_state(
        id INTEGER PRIMARY KEY CHECK (id = 1),
        fan TEXT NOT NULL DEFAULT 'OFF',
        light TEXT NOT NULL DEFAULT 'OFF',
        last_seen REAL
    )
    """)
    cur.execute("INSERT OR IGNORE INTO device_state (id, fan, light, last_seen) VALUES (1, 'OFF', 'OFF', NULL)")

    # Outbound command queue. The ESP32 drains this on every poll.
    cur.execute("""
    CREATE TABLE IF NOT EXISTS device_commands(
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        command TEXT NOT NULL,
        created_at REAL NOT NULL,
        delivered_at REAL
    )
    """)
    conn.commit()

    admin_username = app.config["ADMIN_USERNAME"]
    admin_password = app.config["ADMIN_PASSWORD"]
    generated = admin_password is None

    if generated:
        admin_password = secrets.token_urlsafe(12)

    # INSERT OR IGNORE + rowcount check (instead of SELECT-then-INSERT) makes
    # this safe when multiple worker processes boot at the same time and
    # both race to create the seed admin user -- only one of them actually
    # inserts a row, and the others are silent no-ops instead of crashing
    # on a UNIQUE constraint violation.
    cur.execute(
        "INSERT OR IGNORE INTO users(username,password) VALUES(?,?)",
        (admin_username, generate_password_hash(admin_password))
    )
    conn.commit()

    if generated and cur.rowcount > 0:
        logger.warning(
            "No ADMIN_PASSWORD set. Generated a one-time password for "
            "user '%s': %s -- change it after logging in, or set "
            "ADMIN_PASSWORD before the next deploy.",
            admin_username, admin_password
        )

    conn.close()


def register_routes(app):

    # ==========================
    # HOME
    # ==========================

    @app.route("/")
    def home():
        if "user" in session:
            return redirect("/dashboard")
        return redirect("/login")

    # ==========================
    # LOGIN
    # ==========================

    @app.route("/login", methods=["GET", "POST"])
    def login():
        if request.method == "POST":
            username = request.form.get("username", "")
            password = request.form.get("password", "")

            conn = get_db(app)
            user = conn.execute(
                "SELECT * FROM users WHERE username=?",
                (username,)
            ).fetchone()
            conn.close()

            if user and check_password_hash(user["password"], password):
                session.clear()
                session["user"] = username
                return redirect("/dashboard")

            return render_template("login.html", error="Invalid Username or Password")

        return render_template("login.html")

    # ==========================
    # REGISTER
    # ==========================

    @app.route("/register", methods=["GET", "POST"])
    def register():
        if request.method == "POST":
            username = request.form.get("username", "").strip()
            password = request.form.get("password", "")

            if not username or not password:
                return render_template("register.html", error="Username and password are required.")

            conn = get_db(app)
            try:
                conn.execute(
                    "INSERT INTO users(username,password) VALUES(?,?)",
                    (username, generate_password_hash(password))
                )
                conn.commit()
                return redirect("/login")
            except sqlite3.IntegrityError:
                return render_template("register.html", error="Username already exists.")
            finally:
                conn.close()

        return render_template("register.html")

    # ==========================
    # DASHBOARD
    # ==========================

    @app.route("/dashboard")
    def dashboard():
        if "user" not in session:
            return redirect("/login")
        return render_template("dashboard.html", username=session["user"])

    # ==========================
    # LOGOUT
    # ==========================

    @app.route("/logout")
    def logout():
        session.clear()
        return redirect("/login")

    # ==========================
    # DEVICE COMMAND QUEUE (used by the browser dashboard, session auth)
    # ==========================

    def queue_command(command):
        conn = get_db(app)
        conn.execute(
            "INSERT INTO device_commands (command, created_at) VALUES (?, ?)",
            (command, time.time())
        )
        conn.commit()
        conn.close()

    def require_login():
        return "user" in session

    @app.route("/fan/on")
    def fan_on():
        if not require_login():
            return jsonify({"success": False, "message": "Unauthorized"}), 401
        queue_command("fan_on")
        return jsonify({"success": True, "queued": "fan_on"})

    @app.route("/fan/off")
    def fan_off():
        if not require_login():
            return jsonify({"success": False, "message": "Unauthorized"}), 401
        queue_command("fan_off")
        return jsonify({"success": True, "queued": "fan_off"})

    @app.route("/light/on")
    def light_on():
        if not require_login():
            return jsonify({"success": False, "message": "Unauthorized"}), 401
        queue_command("light_on")
        return jsonify({"success": True, "queued": "light_on"})

    @app.route("/light/off")
    def light_off():
        if not require_login():
            return jsonify({"success": False, "message": "Unauthorized"}), 401
        queue_command("light_off")
        return jsonify({"success": True, "queued": "light_off"})

    @app.route("/esp32/status")
    def esp32_status():
        if not require_login():
            return jsonify({"online": False, "message": "Unauthorized"}), 401

        conn = get_db(app)
        row = conn.execute("SELECT * FROM device_state WHERE id = 1").fetchone()
        conn.close()

        last_seen = row["last_seen"]
        online = last_seen is not None and (time.time() - last_seen) < app.config["DEVICE_OFFLINE_AFTER"]

        return jsonify({
            "online": online,
            "fan": row["fan"],
            "light": row["light"],
        })

    # ==========================
    # DEVICE-FACING ENDPOINTS (ESP32 firmware, API-key auth, no session)
    # ==========================

    def require_device_key():
        supplied = request.headers.get("X-Device-Key") or request.args.get("key", "")
        return secrets.compare_digest(supplied, app.config["DEVICE_API_KEY"])

    @app.route("/device/sync", methods=["POST"])
    def device_sync():
        """
        The ESP32 calls this every couple of seconds. It reports its
        current state and, in the same round trip, receives the oldest
        pending command (if any) to execute.
        """
        if not require_device_key():
            return jsonify({"error": "unauthorized"}), 401

        payload = request.get_json(silent=True) or {}
        fan = payload.get("fan")
        light = payload.get("light")

        conn = get_db(app)
        cur = conn.cursor()

        if fan in ("ON", "OFF") or light in ("ON", "OFF"):
            existing = cur.execute("SELECT * FROM device_state WHERE id = 1").fetchone()
            cur.execute(
                "UPDATE device_state SET fan = ?, light = ?, last_seen = ? WHERE id = 1",
                (fan if fan in ("ON", "OFF") else existing["fan"],
                 light if light in ("ON", "OFF") else existing["light"],
                 time.time())
            )
        else:
            cur.execute("UPDATE device_state SET last_seen = ? WHERE id = 1", (time.time(),))

        conn.commit()

        next_cmd = cur.execute(
            "SELECT * FROM device_commands WHERE delivered_at IS NULL ORDER BY id ASC LIMIT 1"
        ).fetchone()

        command_to_send = None
        if next_cmd is not None:
            command_to_send = next_cmd["command"]
            cur.execute(
                "UPDATE device_commands SET delivered_at = ? WHERE id = ?",
                (time.time(), next_cmd["id"])
            )
            conn.commit()

        conn.close()

        return jsonify({"command": command_to_send})

    # ==========================
    # HEALTH CHECK (for load balancers / uptime monitors)
    # ==========================

    @app.route("/healthz")
    def healthz():
        return jsonify({"status": "ok"})


# Module-level app instance for `flask run` / gunicorn (`app:app`)
app = create_app()


if __name__ == "__main__":
    app.run(
        host="0.0.0.0",
        port=int(os.environ.get("PORT", 5000)),
        debug=app.config["DEBUG"]
    )
