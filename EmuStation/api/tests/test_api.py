import json
import subprocess

import pytest
from fastapi.testclient import TestClient

from main import app
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
    # Make this test cartridge a PS3 game.
    metadata_file = valid_cartridge / "cartridge.json"

    with open(metadata_file, "r", encoding="utf-8") as file:
        metadata = json.load(file)

    metadata["game"]["platform"] = "PS3"

    with open(metadata_file, "w", encoding="utf-8") as file:
        json.dump(metadata, file)

    # Create a temporary emulator configuration.
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

    # Initialize independent managers for the test.
    cartridge_manager = CartridgeManager(valid_cartridge)
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


def test_get_cartridge_game(api_client):
    response = api_client.get("/api/cartridge/game")

    assert response.status_code == 200
    assert response.json()["title"] == "Crash Bandicoot"
    assert response.json()["platform"] == "PS3"


def test_launch_game(api_client, monkeypatch):
    fake_process = FakeProcess()

    def fake_popen(command):
        assert command[0] == "rpcs3"
        assert command[1].endswith("crash-bandicoot.chd")
        return fake_process

    monkeypatch.setattr(
        "services.process_manager.subprocess.Popen",
        fake_popen
    )

    response = api_client.post("/api/cartridge/launch")

    assert response.status_code == 200
    assert response.json()["emulator"] == "rpcs3"
    assert response.json()["process"]["running"] is True


def test_launch_status(api_client, monkeypatch):
    fake_process = FakeProcess()

    monkeypatch.setattr(
        "services.process_manager.subprocess.Popen",
        lambda command: fake_process
    )

    api_client.post("/api/cartridge/launch")

    response = api_client.get("/api/launch/status")

    assert response.status_code == 200
    assert response.json()["running"] is True


def test_stop_game(api_client, monkeypatch):
    fake_process = FakeProcess()

    monkeypatch.setattr(
        "services.process_manager.subprocess.Popen",
        lambda command: fake_process
    )

    api_client.post("/api/cartridge/launch")

    response = api_client.post("/api/launch/stop")

    assert response.status_code == 200
    assert response.json()["running"] is False


def test_launch_without_cartridge(api_client):
    app.state.cartridge_manager.cartridge_path = (
        app.state.cartridge_manager.cartridge_path.parent / "missing"
    )

    app.state.cartridge_manager.detect_cartridge()

    response = api_client.post("/api/cartridge/launch")

    assert response.status_code == 400