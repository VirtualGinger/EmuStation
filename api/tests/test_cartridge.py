
from services.cartridge_manager import CartridgeManager


def test_cartridge_detected(valid_cartridge):

    manager = CartridgeManager(valid_cartridge)

    manager.detect_cartridge()

    status = manager.get_status()

    assert status["connected"] is True
    assert status["status"] == "ready"
    assert status["game"]["title"] == "Crash Bandicoot"


def test_cartridge_not_detected(tmp_path):

    missing_cartridge = tmp_path / "missing"

    manager = CartridgeManager(missing_cartridge)

    manager.detect_cartridge()

    status = manager.get_status()

    assert status["connected"] is False
    assert status["status"] == "no_cartridge"
    assert status["game"] is None


def test_invalid_cartridge(tmp_path):

    cartridge = tmp_path / "cartridge"
    cartridge.mkdir()

    manager = CartridgeManager(cartridge)

    manager.detect_cartridge()

    status = manager.get_status()

    assert status["connected"] is True
    assert status["status"] == "invalid_cartridge"
    assert status["game"] is None


def test_cartridge_removal(valid_cartridge):

    manager = CartridgeManager(valid_cartridge)

    manager.detect_cartridge()

    assert manager.get_status()["connected"] is True

    import shutil
    shutil.rmtree(valid_cartridge)

    manager.detect_cartridge()

    status = manager.get_status()

    assert status["connected"] is False
    assert status["status"] == "no_cartridge"


def test_cartridge_reinsertion(valid_cartridge):

    manager = CartridgeManager(valid_cartridge)

    manager.detect_cartridge()

    assert manager.get_status()["status"] == "ready"

    import shutil
    shutil.rmtree(valid_cartridge)

    manager.detect_cartridge()

    assert manager.get_status()["status"] == "no_cartridge"

    import json

    valid_cartridge.mkdir()
    (valid_cartridge / "games").mkdir()

    (valid_cartridge / "games" / "crash-bandicoot.chd").touch()

    metadata = {
        "cartridge_id": "CART-001",
        "game": {
            "id": "crash-bandicoot",
            "title": "Crash Bandicoot",
            "platform": "PS3",
            "file": "games/crash-bandicoot.chd"
        }
    }

    (valid_cartridge / "cartridge.json").write_text(
        json.dumps(metadata)
    )

    manager.detect_cartridge()

    assert manager.get_status()["status"] == "ready"
    assert manager.get_status()["connected"] is True
