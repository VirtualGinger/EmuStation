
import json
import pytest

from services.metadata_manager import read_metadata
from tests.conftest import make_ps3_game


def test_read_valid_metadata(valid_cartridge):
    result = read_metadata(valid_cartridge)

    assert result["cartridge_id"] == "CART-001"
    assert result["games"][0]["title"] == "Crash Bandicoot"
    assert result["games"][0]["platform"] == "PS3"


def test_missing_metadata(tmp_path):
    with pytest.raises(FileNotFoundError):
        read_metadata(tmp_path)


def test_invalid_json(valid_cartridge):
    metadata_file = valid_cartridge / "cartridge.json"
    metadata_file.write_text("{invalid json")

    with pytest.raises(json.JSONDecodeError):
        read_metadata(valid_cartridge)


def test_missing_cartridge_id(valid_cartridge):
    metadata_file = valid_cartridge / "cartridge.json"

    metadata_file.write_text(json.dumps({
        "game": {}
    }))

    with pytest.raises(ValueError):
        read_metadata(valid_cartridge)


def test_missing_game_file(valid_cartridge):
    metadata_file = valid_cartridge / "cartridge.json"

    metadata = json.loads(metadata_file.read_text())
    metadata["game"]["file"] = "games/missing.chd"

    metadata_file.write_text(json.dumps(metadata))

    with pytest.raises(FileNotFoundError):
        read_metadata(valid_cartridge)


def test_invalid_game_path(valid_cartridge):
    metadata_file = valid_cartridge / "cartridge.json"

    metadata = json.loads(metadata_file.read_text())
    metadata["game"]["file"] = "../../outside.chd"

    metadata_file.write_text(json.dumps(metadata))

    with pytest.raises(ValueError):
        read_metadata(valid_cartridge)


def test_missing_platform(valid_cartridge):
    metadata_file = valid_cartridge / "cartridge.json"

    metadata = json.loads(metadata_file.read_text())
    del metadata["game"]["platform"]

    metadata_file.write_text(json.dumps(metadata))

    with pytest.raises(ValueError):
        read_metadata(valid_cartridge)


def test_detect_ps3_game_at_usb_root(tmp_path):
    ps3_game = tmp_path / "PS3_GAME"
    ps3_game.mkdir()
    (ps3_game / "PARAM.SFO").touch()

    result = read_metadata(tmp_path)

    assert result["games"][0]["platform"] == "PS3"
    assert result["games"][0]["file"] == "."


def test_detect_ps3_game_inside_game_folder(tmp_path):
    game_folder = tmp_path / "God of War"
    ps3_game = game_folder / "PS3_GAME"

    ps3_game.mkdir(parents=True)
    (ps3_game / "PARAM.SFO").touch()

    result = read_metadata(tmp_path)

    assert result["games"][0]["title"] == "God of War"
    assert result["games"][0]["platform"] == "PS3"
    assert result["games"][0]["file"] == "God of War"


def test_missing_ps3_game_folder(tmp_path):
    with pytest.raises(FileNotFoundError):
        read_metadata(tmp_path)


def test_detect_multiple_ps3_games(tmp_path):
    make_ps3_game(tmp_path / "God of War")
    make_ps3_game(tmp_path / "Uncharted", with_cover=True)

    games = read_metadata(tmp_path)["games"]

    assert sorted(game["title"] for game in games) == ["God of War", "Uncharted"]
    covers = {game["title"]: game["cover"] for game in games}
    assert covers["God of War"] is None
    assert covers["Uncharted"].endswith("ICON0.PNG")


def test_param_sfo_details(tmp_path):
    make_ps3_game(
        tmp_path / "Minecraft (USA)",
        sfo_fields={
            "TITLE": "Minecraft: PlayStation®3 Edition",
            "TITLE_ID": "BLUS31426",
            "VERSION": "01.01",
        },
    )

    game = read_metadata(tmp_path)["games"][0]

    assert game["title"] == "Minecraft: PlayStation®3 Edition"
    assert game["title_id"] == "BLUS31426"
    assert game["version"] == "01.01"
    assert game["id"] == "minecraft-usa"


def test_manifest_with_games_list(valid_cartridge):
    make_ps3_game(valid_cartridge / "games" / "Spyro")
    metadata_file = valid_cartridge / "cartridge.json"

    metadata = json.loads(metadata_file.read_text())
    metadata["games"] = [
        metadata.pop("game"),
        {"id": "spyro", "title": "Spyro", "platform": "PS3", "file": "games/Spyro"},
        {"id": "broken", "title": "Broken", "platform": "PS3", "file": "games/missing"},
    ]
    metadata_file.write_text(json.dumps(metadata))

    games = read_metadata(valid_cartridge)["games"]

    assert [game["id"] for game in games] == ["crash-bandicoot", "spyro"]


def test_duplicate_ids_are_made_unique(tmp_path):
    make_ps3_game(tmp_path / "Game A")
    make_ps3_game(tmp_path / "Game-A")

    ids = sorted(game["id"] for game in read_metadata(tmp_path)["games"])

    assert ids == ["game-a", "game-a-2"]

