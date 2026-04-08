from sqlalchemy import BigInteger, String, ForeignKey, Integer, SmallInteger
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column
from sqlalchemy.ext.asyncio import AsyncAttrs, async_sessionmaker, create_async_engine

engine = create_async_engine(url="sqlite+aiosqlite:///db.sqlite3")

async_session = async_sessionmaker(engine)

class Base(AsyncAttrs, DeclarativeBase):
    pass

class Hub(Base):
    __tablename__ = "hub_info"

    id: Mapped[int] = mapped_column(primary_key=True)
    tg_id: Mapped[int] = mapped_column(BigInteger, unique=False, nullable=True)
    vk_id: Mapped[int] = mapped_column(BigInteger, unique=False, nullable=True)
    notify_mode: Mapped[int] = mapped_column(SmallInteger, unique=False, nullable=False, default=0) # 0 - все, 1 - тг, 2 - вк
    sensor_count: Mapped[int] = mapped_column(Integer, unique=False, default=0)

class Sensor(Base):
    __tablename__ = "sensors"
    hub_id: Mapped[int] = mapped_column(ForeignKey(Hub.id), primary_key=True, nullable=False, unique=False)
    id: Mapped[int] = mapped_column(primary_key=True)
    location: Mapped[str] = mapped_column(String(64), nullable=True)
    water_threshold: Mapped[int] = mapped_column(SmallInteger, default=0, nullable=False) # ?
    battery_threshold: Mapped[bool] = mapped_column(default=False, nullable=False) # 0 -> 20, 1 -> 50
    work_mode: Mapped[int] = mapped_column(SmallInteger, default=0, nullable=False) # 0 - max security, 1 - classic, 2 (void), 3 - max energy saving
    notifications: Mapped[int] = mapped_column(SmallInteger, default=2, nullable=False) # 0 - just chat, 1 - just sound, 2 - both
    shutoff: Mapped[bool] = mapped_column(default=False) # право на перекрытие

async def init_database():
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)