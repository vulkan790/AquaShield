import asyncio
from database.requests import *
from database.models import init_database
from database.utils import *

async def main():
    await init_database()


if __name__ == "__main__":
    asyncio.run(main())