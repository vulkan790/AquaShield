import database.requests as rq

async def get_sensor_settings(hub_id, sensor_id) -> str:
    sensor = await rq.get_sensor(hub_id, sensor_id)
    if not sensor:
        raise RuntimeError(f"Датчик #{sensor_id} у хаба #{hub_id} не найден")

    notify = ["Только в чате", "Только звук", "И звуковое, и в чате"]

    sensor_data = f"""Настройки для датчика #<b>{sensor.id}</b>:
    Хаб №<b>{sensor.hub_id}</b>
    Место: <code>{sensor.location}</code>
    Низкий заряд начиная с <code>{"50%" if sensor.battery_threshold == 1 or sensor.battery_threshold == True else "20%"}</code>
    Режим уведомлений: <code>{notify[sensor.notifications]}</code>
    Право на перекрытие воды: <code>{"Да" if sensor.shutoff == 1 or sensor.shutoff == True else "Нет"}</code>"""

    return sensor_data