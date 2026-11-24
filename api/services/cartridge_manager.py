
import asyncio
from pathlib import Path

from services.metadata_manager import read_metadata


class CartridgeManager:

    def __init__(self, cartridge_path=None, scan_usb=True):
        self.cartridge_path = (
            Path(cartridge_path).resolve()
            if cartridge_path is not None
            else None
        )
        self.scan_usb = scan_usb

        self.cartridge_state = {
            "connected": False,
            "status": "no_cartridge",
            "game": None
        }

    def _get_mounted_drives(self):
        mount_roots = (
            Path("/media"),
            Path("/mnt"),
            Path("/run/media"),
        )

        mounts = []

        try:
            with open("/proc/self/mounts", "r", encoding="utf-8") as file:
                for line in file:
                    parts = line.split()

                    if len(parts) < 2:
                        continue

                    mount_path = Path(
                        parts[1]
                        .replace("\\040", " ")
                        .replace("\\011", "\t")
                        .replace("\\134", "\\")
                    )

                    resolved = mount_path.resolve()

                    for root in mount_roots:
                        try:
                            relative = resolved.relative_to(root.resolve())

                            if relative.parts:
                                mounts.append(resolved)

                        except ValueError:
                            continue

        except OSError:
            pass

        return list(dict.fromkeys(mounts))

    def _check_cartridge(self, path):
        try:
            path = Path(path).resolve()
            metadata = read_metadata(path)

            self.cartridge_path = path

            self.cartridge_state = {
                "connected": True,
                "status": "ready",
                "cartridge_id": metadata["cartridge_id"],
                "game": metadata["game"]
            }

            return True

        except (ValueError, FileNotFoundError, OSError):
            return False

    def detect_cartridge(self):
        invalid_drive_found = False

        # Scan real mounted USB drives first.
        if self.scan_usb:
            for drive in self._get_mounted_drives():
                if self._check_cartridge(drive):
                    return

                try:
                    has_game = (
                        (drive / "PS3_GAME" / "PARAM.SFO").is_file()
                        or any(
                            child.is_dir()
                            and (child / "PS3_GAME" / "PARAM.SFO").is_file()
                            for child in drive.iterdir()
                        )
                    )
                except OSError:
                    has_game = False

                if has_game:
                    invalid_drive_found = True

        # Optional explicitly configured test cartridge.
        if self.cartridge_path and self.cartridge_path.is_dir():
            if self._check_cartridge(self.cartridge_path):
                return

            invalid_drive_found = True

        self.cartridge_path = None

        self.cartridge_state = {
            "connected": invalid_drive_found,
            "status": (
                "invalid_cartridge"
                if invalid_drive_found
                else "no_cartridge"
            ),
            "game": None
        }

    def get_status(self):
        return self.cartridge_state

    async def monitor(self):
        while True:
            await asyncio.to_thread(self.detect_cartridge)
            await asyncio.sleep(1)
