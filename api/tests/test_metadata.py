
import json
import pytest

from services.metadata_manager import read_metadata


def test_read_valid_metadata(valid_cartridge):
    result = read_metadata(valid_cartridge)

    assert result["cartridge_id"] == "CART-001"
    assert result["game"]["title"] == "Crash Bandicoot"
    assert result["game"]["platform"] == "PS3"


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

    assert result["game"]["platform"] == "PS3"
    assert result["game"]["file"] == "."


def test_detect_ps3_game_inside_game_folder(tmp_path):
    game_folder = tmp_path / "God of War"
    ps3_game = game_folder / "PS3_GAME"

    ps3_game.mkdir(parents=True)
    (ps3_game / "PARAM.SFO").touch()

    result = read_metadata(tmp_path)

    assert result["game"]["title"] == "God of War"
    assert result["game"]["platform"] == "PS3"
    assert result["game"]["file"] == "God of War"


def test_missing_ps3_game_folder(tmp_path):
    with pytest.raises(FileNotFoundError):
        read_metadata(tmp_path)

