from sqlalchemy.sql.sqltypes import NULLTYPE

from database.models import async_session
from database.models import Sensor, Hub
from sqlalchemy import select, delete

async def is_available(hub_id, tg_id=None, vk_id=None):
    async with async_session() as session:
        hub = await session.get(Hub, hub_id)

        if not hub:
            return "Incorrect hub_id"

        if hub.tg_id == "NULL" and hub.vk_id == "NULL":
            return "Full Available"

        if tg_id:
            if hub.tg_id == tg_id:
                return "User is login"
            if hub.tg_id != tg_id:
                raise ValueError("Other TgBot already connected")

        if vk_id:
            if hub.vk_id == vk_id:
                return "User is login"
            if hub.vk_id != vk_id:
                raise ValueError("Other VkBot already connected")

async def add_tg(hub_id: int, tg_id):
    async with async_session() as session:
        hub = await session.get(Hub, hub_id)

        if not hub:
            raise ValueError("Хаб не найден")
        if hub.tg_id != "NULL":
            raise ValueError("Хаб уже привязан к другому пользователю")

        hub.tg_id = tg_id
        await session.commit()

async def add_vk(hub_id: int, vk_id):
    async with async_session() as session:
        hub = await session.get(Hub, hub_id)

        if not hub:
            raise ValueError("Хаб не найден")
        if hub.vk_id != "NULL":
            raise ValueError("Хаб уже привязан к другому пользователю")

        hub.vk_id = vk_id
        await session.commit()

async def get_tg(hub_id):
    async with async_session() as session:
        hub = await session.get(Hub, hub_id)

        if not hub:
            return None

        return hub.tg_id

async def get_hubs(tg_id=None, vk_id=None) -> list[int]:
    async with async_session() as session:
        if tg_id:
            raw_hubs = await session.scalars(
                select(Hub)
                .where(Hub.tg_id == tg_id)
                .order_by(Hub.id)
            )
        if vk_id and len(raw_hubs) == 0:
            raw_hubs = await session.scalars(
                select(Hub)
                .where(Hub.vk_id == vk_id)
                .order_by(Hub.id)
            )

        if len(raw_hubs) == 0: return []

        return [hub.id for hub in raw_hubs]

async def get_vk(hub_id):
    async with async_session() as session:
        hub = await session.get(Hub, hub_id)

        if not hub:
            return None

        return hub.vk_id

async def add_sensor(hub_id: int,
                     id: int,
                     location: str = "",
                     water_threshold: int = 1,
                     battery_threshold: bool = False,
                     work_mode: int = 2,
                     notifications: int = 2,
                     shutoff: bool = False):
    async with async_session() as session:

        hub = await session.get(Hub, hub_id)

        if not hub:
            raise ValueError("Хаб не найден")

        session.add(Sensor(
            hub_id = hub_id,
            id = id,
            location = location,
            water_threshold = water_threshold,
            battery_threshold = battery_threshold ,
            work_mode = work_mode,
            notifications = notifications,
            shutoff = shutoff
        ))

        hub.sensor_count += 1

        await session.commit()

async def get_sensors(hub_id):
    async with async_session() as session:

        sensors = await session.scalars(
            select(Sensor)
            .where(Sensor.hub_id == hub_id)
            .order_by(Sensor.id)
        )

        return sensors.all()

async def get_sensor(hub_id, sensor_id):
    async with async_session() as session:
        return await session.get(Sensor, (hub_id, sensor_id))

async def change_sensor(hub_id: int,
                     ind: int,
                     location: str = None,
                     water_threshold: int = None,
                     battery_threshold: bool = None,
                     work_mode: int = None,
                     notifications: int = None,
                     shutoff: bool = None):
    async with async_session() as session:

        sensor = await session.scalar(
            select(Sensor).where(
                Sensor.hub_id == hub_id,
                Sensor.id == ind
            )
        )

        print(f"""
            new location: {location}
            new notifications: {notifications}
            new shutoff: {shutoff}    
        """)

        if not sensor:
            raise ValueError("Датчик не найден")

        if location is not None:
            sensor.location = location
        if water_threshold is not None:
            sensor.water_threshold = water_threshold
        if battery_threshold is not None:
            sensor.battery_threshold = battery_threshold
        if work_mode is not None:
            sensor.work_mode = work_mode
        if notifications is not None:
            sensor.notifications = notifications
        if shutoff is not None:
            sensor.shutoff = shutoff

        await session.commit()

async def delete_sensor(hub_id: int, ind: int ) -> bool:
    async with async_session() as session:
        result = await session.execute(
            delete(Sensor)
            .where(
                Sensor.hub_id == hub_id,
                Sensor.id == ind
            )
        )
        await session.commit()
        return result.rowcount > 0

async def disconnect_hub(hub_id: int) -> bool:
    async with async_session() as session:
        hub = await session.get(Hub, hub_id)

        if not hub:
            raise ValueError("Хаб не найден")
        if hub.tg_id != "NULL":
            raise ValueError("Хаб уже привязан к другому пользователю")

        hub.tg_id = NULLTYPE
        hub.vk_id = NULLTYPE

        result = await session.execute(
            delete(Sensor)
            .where(Sensor.hub_id == hub_id)
        )
        
        await session.commit()
        return result
