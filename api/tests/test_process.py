import subprocess
import pytest

from services.process_manager import ProcessManager


class FakeProcess:
    def __init__(self, pid=1234):
        self.pid = pid
        self.returncode = None
        self.terminated = False
        self.killed = False

    def poll(self):
        return self.returncode

    def terminate(self):
        self.terminated = True
        self.returncode = 0

    def kill(self):
        self.killed = True
        self.returncode = -9

    def wait(self, timeout=None):
        if self.returncode is None:
            raise subprocess.TimeoutExpired("rpcs3", timeout)

        return self.returncode


def test_start_process(monkeypatch):
    manager = ProcessManager()
    fake_process = FakeProcess()

    def fake_popen(command):
        assert command == ["rpcs3", "game_path"]
        return fake_process

    monkeypatch.setattr(
        "services.process_manager.subprocess.Popen",
        fake_popen
    )

    result = manager.start(["rpcs3", "game_path"])

    assert result["running"] is True
    assert result["pid"] == 1234


def test_get_running_status(monkeypatch):
    manager = ProcessManager()
    fake_process = FakeProcess()

    monkeypatch.setattr(
        "services.process_manager.subprocess.Popen",
        lambda command: fake_process
    )

    manager.start(["rpcs3", "game_path"])

    status = manager.get_status()

    assert status["running"] is True
    assert status["pid"] == 1234


def test_prevent_multiple_processes(monkeypatch):
    manager = ProcessManager()
    fake_process = FakeProcess()

    monkeypatch.setattr(
        "services.process_manager.subprocess.Popen",
        lambda command: fake_process
    )

    manager.start(["rpcs3", "game_path"])

    with pytest.raises(RuntimeError):
        manager.start(["rpcs3", "another_game"])


def test_stop_process(monkeypatch):
    manager = ProcessManager()
    fake_process = FakeProcess()

    monkeypatch.setattr(
        "services.process_manager.subprocess.Popen",
        lambda command: fake_process
    )

    manager.start(["rpcs3", "game_path"])

    result = manager.stop()

    assert result["running"] is False
    assert fake_process.terminated is True


def test_reject_empty_command():
    manager = ProcessManager()

    with pytest.raises(ValueError):
        manager.start([])


def test_stop_when_no_process_running():
    manager = ProcessManager()

    result = manager.stop()

    assert result["running"] is False