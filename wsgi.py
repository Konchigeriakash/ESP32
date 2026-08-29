"""
Production WSGI entry point.

Run with:
    gunicorn -w 2 -b 0.0.0.0:5000 wsgi:app
"""
from app import app

if __name__ == "__main__":
    app.run()
