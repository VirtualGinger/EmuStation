
import json
import pytest


@pytest.fixture
def valid_cartridge(tmp_path):

    cartridge = tmp_path / "cartridge"
    games = cartridge / "games"

    games.mkdir(parents=True)

    (games / "crash-bandicoot.chd").touch()

    metadata = {
        "cartridge_id": "CART-001",
        "game": {
            "id": "crash-bandicoot",
            "title": "Crash Bandicoot",
            "platform": "PS1",
            "file": "games/crash-bandicoot.chd",
            "cover": "cover.jpg"
        }
    }

    with open(cartridge / "cartridge.json", "w") as file:
        json.dump(metadata, file)

    return cartridge
