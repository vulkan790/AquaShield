from aiogram import Router
from aiogram.filters import CommandStart, Command
from aiogram.fsm.context import FSMContext
from aiogram.types import Message

from source.stations import WorkStates

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