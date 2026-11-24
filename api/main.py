import asyncio
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI

from routes.cartridge import router as cartridge_router
from routes.launch import router as launch_router

from services.cartridge_manager import CartridgeManager
from services.emulator_manager import EmulatorManager
from services.process_manager import ProcessManager


BASE_DIR = Path(__file__).parent

CARTRIDGE_PATH = Path("/mnt/emustation-usb")
EMULATOR_CONFIG = BASE_DIR / "config" / "emulators.json"


cartridge_manager = CartridgeManager(
    CARTRIDGE_PATH,
    scan_usb=False
)
emulator_manager = EmulatorManager(EMULATOR_CONFIG)
process_manager = ProcessManager()


@asynccontextmanager
async def lifespan(app: FastAPI):
    monitor_task = asyncio.create_task(
        app.state.cartridge_manager.monitor()
    )

    yield

    monitor_task.cancel()

    try:
        await monitor_task
    except asyncio.CancelledError:
        pass


app = FastAPI(
    title="Game Console API",
    description="Cartridge detection and game launching API",
    version="1.0.0",
    lifespan=lifespan
)


app.state.cartridge_manager = cartridge_manager
app.state.emulator_manager = emulator_manager
app.state.process_manager = process_manager


app.include_router(cartridge_router)
app.include_router(launch_router)


@app.get("/")
def home():
    return {"message": "Game Console API is running"}
