import json
import subprocess

import pytest
from fastapi.testclient import TestClient

from main import app
from pathlib import Path
from services.cartridge_manager import CartridgeManager
from services.emulator_manager import EmulatorManager
from services.process_manager import ProcessManager


class FakeProcess:
    def __init__(self):
        self.pid = 1234
        self.returncode = None

    def poll(self):
        return self.returncode

    def terminate(self):
        self.returncode = 0

    def kill(self):
        self.returncode = -9

    def wait(self, timeout=None):
        return self.returncode



@pytest.fixture
def api_client(valid_cartridge, tmp_path):
    config_file = tmp_path / "emulators.json"

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

    with open(config_file, "w", encoding="utf-8") as file:
        json.dump(config, file)

    cartridge_manager = CartridgeManager(
        valid_cartridge,
        scan_usb=False
    )
    cartridge_manager.detect_cartridge()

    app.state.cartridge_manager = cartridge_manager
    app.state.emulator_manager = EmulatorManager(config_file)
    app.state.process_manager = ProcessManager()

    with TestClient(app) as client:
        yield client



def test_api_home(api_client):
    response = api_client.get("/")

    assert response.status_code == 200
    assert response.json()["message"] == "Game Console API is running"


def test_get_cartridge_status(api_client):
    response = api_client.get("/api/cartridge/status")

    assert response.status_code == 200
    assert response.json()["status"] == "ready"


def test_get_cartridge_games(api_client):
    response = api_client.get("/api/cartridge/games")

    assert response.status_code == 200
    assert response.json()[0]["title"] == "Crash Bandicoot"
    assert response.json()[0]["platform"] == "PS3"


def test_cover_for_unknown_game(api_client):
    response = api_client.get("/api/cartridge/games/missing-game/cover")

    assert response.status_code == 404
    assert response.json()["detail"] == "Game not found"


def test_launch_unknown_game(api_client):
    response = api_client.post("/api/cartridge/launch/missing-game")

    assert response.status_code == 404
    assert response.json()["detail"] == "Game not found"


def test_launch_game(api_client, monkeypatch):
    fake_process = FakeProcess()

    def fake_popen(command):
        assert command[0] == "rpcs3"
        assert command[1].endswith("Crash Bandicoot")
        assert Path(command[1]).is_dir()
        assert (Path(command[1]) / "PS3_GAME" / "PARAM.SFO").is_file()
        return fake_process

    monkeypatch.setattr(
        "services.process_manager.subprocess.Popen",
        fake_popen
    )

    response = api_client.post("/api/cartridge/launch/crash-bandicoot")
    print("HTTP status:", response.status_code)
    print("Response:", response.json())

    manager = app.state.cartridge_manager
    print("Cartridge path:", manager.cartridge_path)
    print("Cartridge status:", manager.get_status())

    game_path = Path(manager.cartridge_path) / "games" / "Crash Bandicoot"
    print("PS3 directory exists:", game_path.is_dir())
    print("PARAM.SFO exists:", (game_path / "PS3_GAME" / "PARAM.SFO").is_file())

    assert response.status_code == 200
    assert response.json()["emulator"] == "rpcs3"
    assert response.json()["process"]["running"] is True


def test_launch_status(api_client, monkeypatch):
    fake_process = FakeProcess()

    monkeypatch.setattr(
        "services.process_manager.subprocess.Popen",
        lambda command: fake_process
    )

    api_client.post("/api/cartridge/launch/crash-bandicoot")

    response = api_client.get("/api/launch/status")

    assert response.status_code == 200
    assert response.json()["running"] is True


def test_stop_game(api_client, monkeypatch):
    fake_process = FakeProcess()

    monkeypatch.setattr(
        "services.process_manager.subprocess.Popen",
        lambda command: fake_process
    )

    api_client.post("/api/cartridge/launch/crash-bandicoot")

    response = api_client.post("/api/launch/stop")

    assert response.status_code == 200
    assert response.json()["running"] is False


def test_launch_without_cartridge(api_client):
    manager = app.state.cartridge_manager

    manager.configured_path = (
        manager.configured_path.parent / "missing"
    )

    manager.detect_cartridge()

    assert manager.get_status()["status"] == "no_cartridge"

    response = api_client.post("/api/cartridge/launch/crash-bandicoot")

    assert response.status_code == 400
    assert response.json()["detail"] == "No valid game cartridge inserted"