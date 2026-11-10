import asyncio
from pathlib import Path

from services.metadata_manager import read_metadata


class CartridgeManager:

    def __init__(self, cartridge_path):

        self.cartridge_path = Path(cartridge_path)

        self.cartridge_state = {
            "connected": False,
            "status": "no_cartridge",
            "game": None
        }

    def detect_cartridge(self):

        if not self.cartridge_path.is_dir():
            self.cartridge_state = {
                "connected": False,
                "status": "no_cartridge",
                "game": None
            }
            return

        try:
            metadata = read_metadata(self.cartridge_path)

            self.cartridge_state = {
                "connected": True,
                "status": "ready",
                "cartridge_id": metadata["cartridge_id"],
                "game": metadata["game"]
            }

        except (ValueError, FileNotFoundError, OSError):
            self.cartridge_state = {
                "connected": True,
                "status": "invalid_cartridge",
                "game": None
            }

    def get_status(self):
        return self.cartridge_state

    async def monitor(self):

        while True:
            self.detect_cartridge()
            await asyncio.sleep(1)