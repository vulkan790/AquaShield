from aiogram import Router, F
from aiogram.filters import CommandStart, Command
from aiogram.fsm.context import FSMContext
from aiogram.types import Message, CallbackQuery

from source.stations import WorkStates

import source.keyboards as kb

router = Router()


@router.message(CommandStart())
async def cmd_start(message: Message, state: FSMContext):
    hello_txt = '''  Приветствуем вас! Спасибо за покупку нашего набора датчиков и доверие к нам.
    Для получения уведомлений о состоянии труб, подключите сначала хаб, а затем датчики к нему с помощью команды /add.

    Вы всегда можете ввести /help для получения информации о списке команд.'''

    await state.set_state(WorkStates.start)
    await state.update_data(tg_id=message.from_user.id)

    await message.answer(hello_txt)


@router.message(Command("help"))
async def help(message: Message, state: FSMContext):
    help_txt = '''
    /add - подключение хаба или датчика.
/settings - настройка конкретного датчика.
    '''
    current_state = await state.get_state()
    if current_state == WorkStates.start.state:
        help_txt += "\n\nНа данный момент у вас нет ни одного подключённого хаба. Воспользуйтесь командой /add."

    await message.answer(help_txt)

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
