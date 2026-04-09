from aiogram import Router, F
from aiogram.enums import ParseMode
from aiogram.filters import Command
from aiogram.fsm.context import FSMContext
from aiogram.types import Message, CallbackQuery
from re import match

from source.stations import ChangeSettings, WorkStates
from database.utils import get_sensor_settings
import source.keyboards as kb
import database.requests as rq

router = Router()

@router.message(Command("settings"))
async def settings_command(message: Message, state: FSMContext):
    data = await state.get_data()
    current_state = await state.get_state()
    hub_id = data.get('hub_id')

    print(f"current state: {current_state}, hub_id: {hub_id}")

    await state.set_state(ChangeSettings.choose_sensor)
    await state.update_data(hub_id=hub_id)

    await message.answer("Какой из датчиков вы хотите настроить?",
                         reply_markup=await kb.choose_sensor(hub_id))

@router.callback_query(ChangeSettings.choose_sensor, F.data.startswith("sensor_"))
async def selected_sensor(callback: CallbackQuery, state: FSMContext):
    sensor_id = match(r"^sensor_(\d+)$", callback.data).group(1)

    data = await state.get_data()
    hub_id = data.get('hub_id')

    msg = await get_sensor_settings(hub_id, int(sensor_id))
    msg = "Текущие н" + msg[1:]
    await callback.message.edit_text(msg, parse_mode=ParseMode.HTML)
    await callback.message.answer("Что хотите изменить?", reply_markup=await kb.settings_menu())

    await state.set_state(ChangeSettings.choose_setting)
    await state.update_data(hub_id=hub_id, sensor_id=sensor_id)

@router.callback_query(ChangeSettings.choose_setting, F.data.in_(["alert", "overlap", "location"]))
async def selected_settings(callback: CallbackQuery, state: FSMContext):
    await callback.answer("")
    data = await state.get_data()
    hub_id = data.get('hub_id')
    sensor_id = data.get('sensor_id')

    if  (callback.data == "alert"):
        await callback.message.edit_text("Изменение режима уведомлений:", reply_markup=await kb.alert_menu())
        await state.set_state(ChangeSettings.wait_alert)
    elif ( callback.data == "overlap"):
        await callback.message.edit_text("Изменение прав на перекрытие:", reply_markup=await kb.overlap_menu())
        await state.set_state(ChangeSettings.wait_overlap)
    else:
        await callback.message.edit_text("Введите новое положение датчика: ", reply_markup=None)
        await state.set_state(ChangeSettings.wait_location)

    await state.update_data(hub_id=hub_id, sensor_id=sensor_id)

@router.message(ChangeSettings.wait_location)
async def got_new_location(message: Message, state: FSMContext):
    new_location = message.text
    if len(new_location) > 64:
        await message.answer("Слишком длинное название! Попробуйте ещё раз")
        return
    await state.update_data(new_location=new_location)
    await state.set_state(ChangeSettings.confirm)
    await confirm_new_message(message, state)   # новое сообщение

@router.callback_query(ChangeSettings.wait_alert, F.data.in_(["just_chat", "just_sound", "both"]))
async def got_new_alert(callback: CallbackQuery, state: FSMContext):
    convert = {"just_chat": 0, "just_sound": 1, "both": 2}
    new_alert = convert[callback.data]
    await state.update_data(new_notifications=new_alert)
    await state.set_state(ChangeSettings.confirm)
    await show_confirmation(callback.message, state)
    await callback.answer()

@router.callback_query(ChangeSettings.wait_overlap, F.data.in_(["overlap_on", "overlap_off"]))
async def got_new_overlap(callback: CallbackQuery, state: FSMContext):
    convert = {"overlap_on": True, "overlap_off": False}
    new_overlap = convert[callback.data]
    await state.update_data(new_shutoff=new_overlap)
    await state.set_state(ChangeSettings.confirm)
    await show_confirmation(callback.message, state)
    await callback.answer()


async def show_confirmation(message: Message, state: FSMContext):
    data = await state.get_data()
    hub_id = data['hub_id']
    sensor_id = data['sensor_id']
    sensor = await rq.get_sensor(hub_id, sensor_id)

    new_location = data.get('new_location', sensor.location)
    new_notifications = data.get('new_notifications', sensor.notifications)
    new_shutoff = data.get('new_shutoff', sensor.shutoff)

    msg = f"<b>Новые настройки датчика #{sensor_id}</b>\nХаб №{hub_id}\n"
    msg += f"Расположение: <code>{new_location}</code>\n"
    msg += f"Порог воды: <code>{sensor.water_threshold}</code>\n"
    msg += f"Порог батареи: <code>{'50%' if sensor.battery_threshold else '20%'}</code>\n"
    msg += f"Уведомления: <code>{['Только чат', 'Только звук', 'Чат и звук'][new_notifications]}</code>\n"
    msg += f"Перекрытие воды: <code>{'Разрешено' if new_shutoff else 'Запрещено'}</code>\n"
    await message.edit_text(msg, parse_mode=ParseMode.HTML)
    await message.answer("Подтвердите применение настроек:", reply_markup=await kb.confirm_menu())

async def confirm_new_message(message: Message, state: FSMContext):
    data = await state.get_data()
    hub_id = data['hub_id']
    sensor_id = data['sensor_id']
    sensor = await rq.get_sensor(hub_id, sensor_id)

    new_location = data.get('new_location', sensor.location)
    new_notifications = data.get('new_notifications', sensor.notifications)
    new_shutoff = data.get('new_shutoff', sensor.shutoff)

    msg = f"<b>Новые настройки датчика #{sensor_id}</b>\nХаб №{hub_id}\n"
    msg += f"Расположение: <code>{new_location}</code>\n"
    msg += f"Порог воды: <code>{sensor.water_threshold}</code>\n"
    msg += f"Порог батареи: <code>{'50%' if sensor.battery_threshold else '20%'}</code>\n"
    msg += f"Уведомления: <code>{['Только чат', 'Только звук', 'Чат и звук'][new_notifications]}</code>\n"
    msg += f"Перекрытие воды: <code>{'Разрешено' if new_shutoff else 'Запрещено'}</code>\n"
    await message.answer(msg, parse_mode=ParseMode.HTML)
    await message.answer("Подтвердите применение настроек:", reply_markup=await kb.confirm_menu())

@router.callback_query(ChangeSettings.confirm, F.data.in_(["confirm", "cancellation"]))
async def apply_settings(callback: CallbackQuery, state: FSMContext):
    await callback.answer()
    data = await state.get_data()
    hub_id = data['hub_id']
    sensor_id = data['sensor_id']

    if callback.data == "confirm":
        update_params = {}
        if 'new_location' in data:
            update_params['location'] = data['new_location']
        if 'new_notifications' in data:
            update_params['notifications'] = data['new_notifications']
        if 'new_shutoff' in data:
            update_params['shutoff'] = data['new_shutoff']

        try:
            await rq.change_sensor(hub_id=hub_id, ind=sensor_id, **update_params)
            msg = "Настройки применены. Через время их получит датчик.\n"
            msg += "Для срочного изменения зажмите кнопку сначала на хабе, а затем на датчике, который хотите обновить.\n"
        except Exception as e:
            msg = f"Ошибка при сохранении: {e}"
    else:
        msg = "Внесение изменений отменено."

    await callback.message.edit_text(msg, parse_mode=ParseMode.HTML, reply_markup=None)
    await state.set_state(WorkStates.ready)
    await state.update_data(hub_id=hub_id)
