from aiogram import Router, F
from aiogram.enums import ParseMode
from aiogram.filters import Command
from aiogram.fsm.context import FSMContext
from aiogram.types import Message, CallbackQuery

from source.stations import ConnectHub, ConnectSensor, WorkStates
from database.utils import get_sensor_settings
import source.keyboards as kb
import database.requests as rq

router = Router()


@router.message(Command("add"))
async def add(message: Message, state: FSMContext):
    current_state = await state.get_state()
    data = await state.get_data()
    hub_id = data.get('hub_id')
    print(f"current state: {current_state}, hub_id: {hub_id}")

    if current_state == WorkStates.ready.state:
        await message.answer("Что именно вы хотите подключить?", reply_markup=await kb.choose_device_type())
    else:
        await state.set_state(ConnectHub.wait_hub_id)
        await message.answer("Отправьте текстом в чат серийный номер вашего хаба. Он находится внутри коробки. Будьте внимательны при вводе серийного номера.")


@router.callback_query(F.data.in_(["sensor", "hub"]))
async def chose_device(callback: CallbackQuery, state: FSMContext):
    await callback.answer("")

    if callback.data == "hub":
        msg = "Вы выбрали подключение <b>хаба</b>. Отправьте текстом в чат его серийный номер. Он находится внутри коробки."
        await state.set_state(ConnectHub.wait_hub_id)
    else:
        data = await state.get_data()
        hub_id = data.get('hub_id')

        if hub_id < 0:
            msg = "Не получилось получить данные о хабе, к которому необходимо подключить датчик."
        else:
            await state.set_state(ConnectSensor.wait_sensor_id)
            await state.update_data(data)
            msg = """Вы выбрали подключение <b>датчика</b>. 
       Чтобы добавить датчик, сначала подключите его к хабу. Для этого необходимо зажать кнопку на корпусе датчика и подождать мигания светодиода на хабе.
       После этого отправьте серийный номер датчика (снизу корпуса) в чат."""

    await callback.message.answer(msg, parse_mode=ParseMode.HTML)


@router.message(ConnectHub.wait_hub_id, F.text.isdigit())
async def connect_hub(message: Message, state: FSMContext):
    try:
        hub_id = int(message.text)

        result = await rq.is_available(hub_id, tg_id=message.from_user.id)
        print(f"Result: {result} \n tg_id: {message.from_user.id}")

        if result in ["TgBot already connected", "Full Available"]:
            await state.set_state(WorkStates.ready)
            await state.update_data(hub_id=hub_id)

            await rq.add_tg(hub_id, message.from_user.id)

            await message.answer(
                f"Хаб с id: <code>{hub_id}</code> успешно подключён. Проверьте ещё раз этот серийный номер и в случае несоответствия используйте /delete",
                parse_mode=ParseMode.HTML
            )
        elif result == "User is login":
            await message.answer(f"С возвращением!")
            await state.set_state(WorkStates.ready)
            await state.update_data(hub_id=hub_id)
        else:
            raise RuntimeError("Неопознаное сообщение")

    except Exception as e:
        await message.answer(f"Ошибка: {e}. \nВведите заново серийный номер.")


@router.message(ConnectSensor.wait_sensor_id, F.text.isdigit())
async def got_sensor_id(message: Message, state: FSMContext):
    await state.update_data(sensor_id=int(message.text))
    await state.set_state(ConnectSensor.wait_location)
    await message.answer("Введите название или расположение датчика (например, «Кухня», «Ванная») <b>не более 64 символов</b>:",
                         parse_mode=ParseMode.HTML)


@router.message(ConnectSensor.wait_location)
async def got_location(message: Message, state: FSMContext):
    if len(message.text) < 65:
        await state.update_data(location=message.text)
        await state.set_state(ConnectSensor.wait_water_threshold)
        await message.answer("[временно] Введите порог срабатывания по воде (только число):")
    else:
        await message.answer("Слишком длинное название! Попробуйте ещё раз")


@router.message(ConnectSensor.wait_water_threshold, F.text.regexp(r'^\d+(\.\d+)?$'))
async def got_water_threshold(message: Message, state: FSMContext):
    await state.update_data(water_threshold=int(message.text))
    await state.set_state(ConnectSensor.wait_battery_threshold)
    await message.answer("Выберите порог, при котором будете получать уведомление о низком заряде:",
                         reply_markup=await kb.battery_threshold_menu())


@router.message(ConnectSensor.wait_water_threshold)
async def invalid_water_threshold(message: Message):
    await message.answer("Пожалуйста, введите число (можно с десятичной точкой).")


@router.callback_query(ConnectSensor.wait_battery_threshold, F.data.in_(["half", "fifth_part"]))
async def got_battery_threshold(callback: CallbackQuery, state: FSMContext):
    await callback.answer()
    convert = {"half": True, "fifth_part": False}
    await state.update_data(battery_threshold=convert[callback.data])
    await state.set_state(ConnectSensor.wait_notifications)
    await callback.message.edit_text(
        "Выберите тип уведомлений:",
        reply_markup=await kb.alert_menu()
    )


@router.message(ConnectSensor.wait_battery_threshold)
async def invalid_battery_threshold(message: Message):
    await message.answer("Пожалуйста, введите число.")


@router.callback_query(ConnectSensor.wait_notifications, F.data.in_(["just_chat", "just_sound", "both"]))
async def got_notifications(callback: CallbackQuery, state: FSMContext):
    await callback.answer()
    convert = {"just_chat": 0, "just_sound": 1, "both": 2}
    await state.update_data(notifications=convert[callback.data])
    await state.set_state(ConnectSensor.wait_shutoff)
    await callback.message.edit_text(
        "Разрешить датчику перекрывать воду?",
        reply_markup=await kb.overlap_menu()
    )


@router.callback_query(ConnectSensor.wait_shutoff, F.data.in_(["overlap_on", "overlap_off"]))
async def got_shutoff(callback: CallbackQuery, state: FSMContext):
    await callback.answer()
    convert = {"overlap_on": True, "overlap_off": False}
    await state.update_data(shutoff=convert[callback.data])
    data = await state.get_data()

    msg = await get_sensor_settings(data.get('hub_id'), data.get('sensor_id'))
    await callback.message.edit_text(msg, parse_mode=ParseMode.HTML)
    await callback.message.answer("Подтвердите применение настроек:", reply_markup=await kb.confirm_menu())


@router.callback_query(ConnectSensor.wait_shutoff, F.data.in_(["confirm", "cancellation"]))
async def connect_sensor(callback: CallbackQuery, state: FSMContext):
    await callback.answer()

    data = await state.get_data()
    hub_id = data.get('hub_id')

    if callback.data == "confirm":
        try:
            await rq.add_sensor(
                hub_id=hub_id,
                id=data.get('sensor_id'),
                location=data.get('location'),
                water_threshold=data.get('water_threshold'),
                battery_threshold=data.get('battery_threshold'),
                notifications=data.get('notifications'),
                shutoff=data.get('shutoff')
            )
            await callback.message.edit_text(f"Датчик #{data['sensor_id']} успешно подключён!")
            await state.set_state(WorkStates.ready)
            await state.update_data(hub_id=hub_id)

        except Exception as e:
            await callback.message.edit_text(f"Ошибка при сохранении: {e}. Попробуйте снова.")
            await state.set_state(WorkStates.ready)
            await state.update_data(hub_id=hub_id)
    else:
        await callback.message.edit_text("Добавление отменено")
        await state.set_state(WorkStates.ready)
        await state.update_data(hub_id=hub_id)