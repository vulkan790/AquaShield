from aiogram import Router, F
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

@router.callback_query(F.data.in_(["sensor", "hub"]))
async def got_device_type(callback: CallbackQuery, state: FSMContext):
    data = await state.get_data()
    hub_id = data.get("hub_id")
    await state.set_state(DeleteDevice.waiting_device_id)
    await state.update_data(hub_id = hub_id)
    await state.update_data(type = callback.data)

    if callback.data == "sensor" and hub_id:
        await callback.message.edit_text("Датчик с каким идентификатором вы хотите удалить?",
                         reply_markup=await kb.choose_sensor(hub_id))
    else:
        #TODO сделать клаву для выбора хаба
        await callback.message.edit_text("Такое пока не умею")

@router.callback_query(F.data.startswith("sensor_"))
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
                                         reply_markup=await kb.confirm_menu())

@router.callback_query(F.data.in_(["confirm", "cancellation"]))
async def delete_confirm(callback: CallbackQuery, state: FSMContext):
    data = await state.get_data()
    hub_id = data.get("hub_id")
    device_type = data.get("type")
    sensor_id = data.get("sensor_id")
    await state.set_state(WorkStates.ready)
    await state.update_data(hub_id=hub_id)

    if (callback.data == "confirm"):
        result = False

        if (device_type == "sensor"):
            result = await rq.delete_sensor(hub_id, sensor_id)
        else:
            pass

        await callback.message.edit_text(
            "Устройство успешно удалено" if result else "Произошла ошибка во время удаления")

    else:
        await callback.message.edit_text("Удаление отменено")