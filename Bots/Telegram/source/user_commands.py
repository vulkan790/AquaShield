from http.client import HTTPMessage

from aiogram import Router, F
from aiogram.filters import CommandStart, Command
from aiogram.fsm.context import FSMContext
from aiogram.types import Message, CallbackQuery
from aiogram.enums import ParseMode

from source.stations import WorkStates
from database.requests import get_hubs, get_sensors
from database.utils import get_sensor_settings

import source.keyboards as kb

router = Router()

@router.message(CommandStart())
async def cmd_start(message: Message, state: FSMContext):
    current_user = message.from_user.id
    hub = await get_hubs(tg_id=current_user)

    if len(hub) == 0:
        hello_txt = '''  Приветствуем вас! Спасибо за покупку нашего набора датчиков и доверие к нам.
Для получения уведомлений о состоянии труб, подключите сначала хаб, а затем датчики к нему с помощью команды /add.
    
Вы всегда можете ввести /help для получения информации о списке команд.
'''
        await state.set_state(WorkStates.start)
        await state.update_data(tg_id=current_user)
    else:
        hello_txt = '''  С возвращением! Рады, что вы снова с нами.
Ваши датчики и хаб уже знакомы с системой. Убедитесь, что всё данные актуальны с помощью /status.

Если вы добавили новые устройства или решили перенастроить систему, воспользуйтесь командой /add. Приятного использования!
'''
        await state.set_state(WorkStates.ready)
        await state.update_data(tg_id=current_user)
        # Временно, т.к. нет поддержки нескольких хабов
        await state.update_data(hub_id=hub[1])

    await message.answer(hello_txt)


@router.message(Command("help"))
async def help(message: Message, state: FSMContext):
    help_txt = '''
/add - подключение хаба или датчика.
/settings - настройка конкретного датчика.
/delete - удаление датчика или хаба со всеми подключёнными к нему датчиками (не протестировано)
/status - получение информации о подключенных устройствах
    '''
    current_state = await state.get_state()
    if current_state == WorkStates.start.state:
        help_txt += "\n\nНа данный момент у вас нет ни одного подключённого хаба. Воспользуйтесь командой /add."

    await message.answer(help_txt)

@router.message(Command("status"))
async def status(message: Message, state: FSMContext):
    data = await state.get_data()
    hub_id = data.get('hub_id')
    sensors = await get_sensors(hub_id)

    if len(sensors) == 0:
        await message.answer(f"Не найдены датчики для хаба #{hub_id}")
    else:
        sensors.sort(key=lambda item: item.id)
        ids = ""
        user_sensors_txt = []

        await state.set_state(WorkStates.viewing)
        await state.update_data(hub_id=hub_id)

        for sensor in sensors:
            ids += str(sensor.id) + ", "
            user_sensors_txt.append(await get_sensor_settings(sensor=sensor))

        await state.update_data(sensors_txt=user_sensors_txt)
        await message.answer(f"Найдены датчики со следующими идентификаторами: {ids[:-2]}")
        await message.answer(
            user_sensors_txt[0],
            reply_markup=await kb.text_navigation_keyboard(0, len(user_sensors_txt)),
            parse_mode=ParseMode.HTML
        )


@router.callback_query(WorkStates.viewing, F.data.startswith("textnav_"))
async def handle_text_navigation(callback: CallbackQuery, state: FSMContext):
    user_id = callback.from_user.id
    data = await state.get_data()
    user_sensors_txt = data.get("sensors_txt")

    if len(user_sensors_txt) == 0:
        await callback.message.edit_text(
            "Похоже, данные были изменены. Если хотите посмотреть подключенные устройства, введите команду /status снова")

    data_parts = callback.data.split("_")
    action = data_parts[1]

    if action == "info":
        await callback.answer()
        return

    new_index = int(data_parts[2])

    # Проверяем границы
    if new_index < 0 or new_index >= len(user_sensors_txt):
        await callback.answer()
        return

    text = user_sensors_txt[new_index]

    await callback.message.edit_text(
        text,
        reply_markup = await kb.text_navigation_keyboard(new_index, len(user_sensors_txt)),
        parse_mode = ParseMode.HTML
    )

    await callback.answer()

@router.callback_query(F.data.startswith("page_"))
async def change_sensor_page(callback: CallbackQuery, state: FSMContext):
    page = int(callback.data.split("_")[1])
    data = await state.get_data()
    hub_id = data.get('hub_id')
    if hub_id is None:
        await callback.answer("Ошибка: не найден хаб", show_alert=True)
        return
    keyboard = await kb.choose_sensor(hub_id, page=page)
    await callback.message.edit_reply_markup(reply_markup=keyboard)
    await callback.answer()
