from aiogram.fsm.state import State, StatesGroup
from aiogram.fsm.storage.memory import MemoryStorage
from aiogram import Bot, Dispatcher

storage = MemoryStorage()
dp = Dispatcher(storage=storage)

class WorkStates(StatesGroup):
    start = State()
    ready = State()

class ConnectDevice(StatesGroup):
    wait_device_type = State()
    wait_hub_id = State()
    wait_sensor_id = State()
    wait_location = State()
    wait_water_threshold = State()
    wait_battery_threshold = State()
    wait_notifications = State()
    wait_shutoff = State()

class ChangeSettings(StatesGroup):
    choose_sensor = State()
    choose_setting = State()
    wait_location = State()
    wait_alert = State()
    wait_overlap = State()
    confirm = State()

class DeleteDevice(StatesGroup):
    waiting_device_type = State()
    waiting_device_id = State()
    waiting_confirm = State()

class AddVK(StatesGroup):
    wait_vk_id = State()
