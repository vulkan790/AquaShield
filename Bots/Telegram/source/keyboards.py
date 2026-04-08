from aiogram.types import InlineKeyboardButton
from aiogram.utils.keyboard import InlineKeyboardBuilder

from database.requests import get_sensors

async def choose_device_type():
    keyboard = InlineKeyboardBuilder()
    keyboard.add(InlineKeyboardButton(text="Хаб", callback_data="hub"))
    keyboard.add(InlineKeyboardButton(text="Датчик", callback_data="sensor"))
    return keyboard.adjust(1).as_markup()

async def settings_menu():
    keyboard = InlineKeyboardBuilder()
    keyboard.add(InlineKeyboardButton(text="Уведомления", callback_data="alert"))
    keyboard.add(InlineKeyboardButton(text="Режим перекрытия", callback_data="overlap"))
    keyboard.add(InlineKeyboardButton(text="Место размещения", callback_data="location"))
    #keyboard.add(InlineKeyboardButton(text="", callback_data="overlap"))

    return keyboard.adjust(1).as_markup()

async def choose_sensor(hub_id, page=0, page_size=4):
    sensors = await get_sensors(hub_id)

    # Разбиение по страницам
    total_pages = (len(sensors) + page_size - 1) // page_size
    start_idx = page * page_size
    end_idx = start_idx + page_size
    page_sensors = sensors[start_idx:end_idx]

    keyboard = InlineKeyboardBuilder()

    # Датчики текущей страницы
    for sensor in page_sensors:
        keyboard.button(
            text=f"Датчик #{sensor.id}, {sensor.location}",
            callback_data=f"sensor_{sensor.id}"
        )

    # Кнопки навигации
    navigation_buttons = []
    if page > 0:
        navigation_buttons.append(InlineKeyboardButton(
            text="⬅️ Назад",
            callback_data=f"page_{page - 1}"
        ))
    if page < total_pages - 1:
        navigation_buttons.append(InlineKeyboardButton(
            text="Вперед ➡️",
            callback_data=f"page_{page + 1}"
        ))

    if navigation_buttons:
        keyboard.row(*navigation_buttons)

    return keyboard.adjust(1).as_markup()

async def alert_menu():
    keyboard = InlineKeyboardBuilder()
    keyboard.add(InlineKeyboardButton(text="Только уведомления в чат", callback_data="just_chat"))
    keyboard.add(InlineKeyboardButton(text="Только звуковое уведомление", callback_data="just_sound"))
    keyboard.add(InlineKeyboardButton(text="Оба уведомления", callback_data="both"))
    return keyboard.adjust(1).as_markup()

async def overlap_menu():
    keyboard = InlineKeyboardBuilder()
    keyboard.add(InlineKeyboardButton(text="Да", callback_data="overlap_on"))
    keyboard.add(InlineKeyboardButton(text="Нет", callback_data="overlap_off"))
    return keyboard.adjust(2).as_markup()

async def battery_threshold_menu():
    keyboard = InlineKeyboardBuilder()
    keyboard.add(InlineKeyboardButton(text="20%", callback_data="fifth_part"))
    keyboard.add(InlineKeyboardButton(text="50%", callback_data="half"))
    return keyboard.adjust(2).as_markup()

async def confirm_menu():
    keyboard = InlineKeyboardBuilder()
    keyboard.add(InlineKeyboardButton(text="✅", callback_data="confirm"))
    keyboard.add(InlineKeyboardButton(text="❌", callback_data="cancellation"))
    return keyboard.adjust(2).as_markup()