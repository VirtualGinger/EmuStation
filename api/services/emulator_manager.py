import json
from pathlib import Path


class EmulatorManager:
    def __init__(self, config_path):
        self.config_path = Path(config_path)
        self.config = self._load_config()

    def _load_config(self):
        if not self.config_path.is_file():
            raise FileNotFoundError("Emulator configuration not found")

        with open(self.config_path, "r", encoding="utf-8") as file:
            config = json.load(file)

        if not isinstance(config, dict):
            raise ValueError("Invalid emulator configuration")

        if not isinstance(config.get("emulators"), dict):
            raise ValueError("Missing emulator configuration")

        return config

    def get_emulator(self, platform):
        platform = platform.strip().upper()

        for emulator_id, emulator in self.config["emulators"].items():
            platforms = emulator.get("platforms", [])

            if platform in [p.upper() for p in platforms]:
                return {
                    "id": emulator_id,
                    **emulator
                }

        raise ValueError(f"No emulator configured for platform: {platform}")

    def build_command(self, platform, game_path):
        emulator = self.get_emulator(platform)

        executable = emulator.get("executable")
        arguments = emulator.get("arguments", [])

        if not isinstance(executable, str) or not executable.strip():
            raise ValueError("Invalid emulator executable")

        if not isinstance(arguments, list):
            raise ValueError("Emulator arguments must be a list")

        game_path = str(Path(game_path).resolve())

        command = [executable]

        for argument in arguments:
            if not isinstance(argument, str):
                raise ValueError("Emulator arguments must be strings")

            command.append(
                argument.replace("{game_path}", game_path)
            )

        return command