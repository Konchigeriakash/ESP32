FROM python:3.12-slim

WORKDIR /app

# Install dependencies first for better layer caching
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# App code
COPY . .

# Persisted volume for the SQLite DB so it survives container restarts
RUN mkdir -p /data
ENV DATABASE_PATH=/data/users.db

RUN useradd --create-home appuser \
    && chown -R appuser:appuser /app /data
USER appuser

EXPOSE 5000

CMD ["gunicorn", "-w", "2", "-b", "0.0.0.0:5000", "wsgi:app"]
