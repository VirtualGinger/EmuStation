import json
import pytest

from services.emulator_manager import EmulatorManager


@pytest.fixture
def emulator_config(tmp_path):
    config = {
        "emulators": {
            "rpcs3": {
                "name": "RPCS3",
                "executable": "rpcs3",
                "platforms": ["PS3"],
                "arguments": ["{game_path}"]
            }
        }
    }

    config_file = tmp_path / "emulators.json"

    with open(config_file, "w", encoding="utf-8") as file:
        json.dump(config, file)

    return config_file


def test_recognizes_ps3(emulator_config):
    manager = EmulatorManager(emulator_config)

    emulator = manager.get_emulator("PS3")

    assert emulator["id"] == "rpcs3"
    assert emulator["name"] == "RPCS3"


def test_platform_is_case_insensitive(emulator_config):
    manager = EmulatorManager(emulator_config)

    emulator = manager.get_emulator("ps3")

    assert emulator["id"] == "rpcs3"


def test_builds_launch_command(emulator_config, tmp_path):
    manager = EmulatorManager(emulator_config)

    game_path = tmp_path / "MyGame"

    command = manager.build_command("PS3", game_path)

    assert command[0] == "rpcs3"
    assert command[1] == str(game_path.resolve())


def test_rejects_unsupported_platform(emulator_config):
    manager = EmulatorManager(emulator_config)

    with pytest.raises(ValueError):
        manager.get_emulator("PS2")