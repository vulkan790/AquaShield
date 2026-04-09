from aiogram import Router, F
from aiogram.enums import ParseMode
from aiogram.filters import Command
from aiogram.fsm.context import FSMContext
from aiogram.types import Message, CallbackQuery

from source.stations import DeleteDevice, WorkStates
from database.utils import get_sensor_settings
import source.keyboards as kb
import database.requests as rq

router = Router()

@router.message(Command("delete"))
async def delete_command(message: Message, state: FSMContext):
    data = await state.get_data()
    await state.set_state(DeleteDevice.waiting_device_type)
    await state.update_data(hub_id = data.get("hub_id"))

    await message.answer("Какой тип устройства вы хотите удалить?",
                         reply_markup=await kb.choose_device_type())

@router.callback_query(DeleteDevice.waiting_device_type, F.data.in_(["sensor", "hub"]))
async def got_device_type(callback: CallbackQuery, state: FSMContext):
    data = await state.get_data()
    hub_id = data.get("hub_id")

    if callback.data == "sensor" and hub_id:
        await state.set_state(DeleteDevice.waiting_device_id)
        await state.update_data(hub_id=hub_id)
        await state.update_data(type=callback.data)

        await callback.message.edit_text("Датчик с каким идентификатором вы хотите удалить?",
                         reply_markup=await kb.choose_sensor(hub_id))
    elif callback.data == "hub" and hub_id:
        await state.set_state(DeleteDevice.waiting_confirm)
        await state.update_data(hub_id=hub_id)
        await state.update_data(type=callback.data)

        await callback.message.edit_text(
            text="Вы действительно хотите удалить этот хаб со всеми прикреплёнными к нему датчиками?",
            reply_markup=await kb.confirm_menu())
    else:
        await callback.message.edit_text("Ошибка")

@router.callback_query(DeleteDevice.waiting_device_id, F.data.startswith("sensor_"))
async def got_sensor_id(callback: CallbackQuery, state: FSMContext):
    data = await state.get_data()
    hub_id = data.get("hub_id")
    device_type = data.get("type")
    await state.set_state(DeleteDevice.waiting_confirm)
    await state.update_data(hub_id=hub_id)
    await state.update_data(type=device_type)
    await state.update_data(sensor_id=callback.data[7:])

    if (device_type == "sensor"):
        msg = await get_sensor_settings(hub_id, callback.data[7:])
        await callback.message.edit_text("Вы действительно хотите удалить этот датчик?\nД" + msg[15:],
                                         parse_mode=ParseMode.HTML,
                                         reply_markup=await kb.confirm_menu())

@router.callback_query(DeleteDevice.waiting_confirm, F.data.in_(["confirm", "cancellation"]))
async def delete_confirm(callback: CallbackQuery, state: FSMContext):
    data = await state.get_data()
    hub_id = data.get("hub_id")
    device_type = data.get("type")
    sensor_id = data.get("sensor_id")
    await state.set_state(WorkStates.ready)
    await state.update_data(hub_id=hub_id)

    if (callback.data == "confirm"):

        if (device_type == "sensor"):
            result = await rq.delete_sensor(hub_id, sensor_id)
        else:
            result = await rq.disconnect_hub(hub_id)

        await callback.message.answer(
            "Устройство успешно удалено." if result else "Произошла ошибка во время удаления"
        )

    else:
        await callback.message.answer("Удаление отменено")
