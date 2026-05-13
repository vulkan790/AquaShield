import requests
import sqlite3
import uvicorn
import logging

from aiogram.types import location
from fastapi import FastAPI, HTTPException
from config import TOKEN
import database.requests as rq

app = FastAPI()

@app.get("/sensor/alert/leak")
async def sensor_leak(hub_id, sensor_id):
    if not hub_id or not sensor_id:
        raise HTTPException(status_code=400, detail="hub_id is required")

    chat_id = await rq.get_tg(hub_id)
    sensor = await rq.get_sensor(hub_id, sensor_id)

    if not chat_id or not sensor:
        return {"status": "error", "error" : "No tg chat or hub"}

    response = requests.post(
    f"https://api.telegram.org/bot{TOKEN}/sendMessage",
    json={
        "chat_id": chat_id,
        "text": f"🚨 ДАТЧИК #{sensor_id}\nМестоположение: {sensor.location}\nОбнаружена протечка воды!",
        "parse_mode": "HTML"
    },
    timeout=5
    )

    return {"status": "received", "leak": "true"}

@app.get("/sensor/alert/dry")
async def relay_worked(hub_id):
    if not hub_id:
        raise HTTPException(status_code=400, detail="hub_id is required")

    chat_id = await rq.get_tg(hub_id)

    if not chat_id:
        return {"status": "error", "error" : "No tg chat or hub"}

    response = requests.post(
    f"https://api.telegram.org/bot{TOKEN}/sendMessage",
    json={
        "chat_id": chat_id,
        "text": f"🔧 Вода перекрыта!",
        "parse_mode": "HTML"
    },
    timeout=5
    )

    return {"status": "received", "leak": "false"}

@app.get("/sensor/alert/battery")
async def sensor_battery(hub_id, sensor_id):
    if not hub_id:
        raise HTTPException(status_code=400, detail="hub_id is required")

    chat_id = await rq.get_tg(hub_id)

    if not chat_id:
        return {"status": "error", "error" : "No tg chat or hub"}

    response = requests.post(
    f"https://api.telegram.org/bot{TOKEN}/sendMessage",
    json={
        "chat_id": chat_id,
        "text": f"Низкий заряд у датчика#{sensor_id}\n",
        "parse_mode": "HTML"
    },
    timeout=5
    )

    return {"status": "received", "low_battery": "true"}

@app.get("/sensors/configuration")
async def get_sensors_configuration(hub_id):
    if not hub_id:
        raise HTTPException(status_code=400, detail="hub_id is required")

    try:
        sensors = await rq.get_sensors(hub_id)

        sensors_data = {
            "hub_id": hub_id,
            "total_sensors": len(sensors),
            "sensors": [
                {
                    "id": sensor.id,
                    "water_threshold": sensor.water_threshold,
                    "battery_threshold": sensor.battery_threshold,
                    "work_mode": sensor.work_mode,
                    "alert_mode": sensor.notifications,
                    "shutoff": sensor.shutoff
                }
                for sensor in sensors
            ]
        }

        return sensors_data

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/")
async def root():
    return {"message": "AquaShield Sensor Server", "status": "running"}


if __name__ == "__main__":

    logging.basicConfig(level=logging.INFO)

    uvicorn.run(app, host="0.0.0.0", port=8000)
