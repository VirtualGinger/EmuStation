
import json
import re
import struct
from pathlib import Path


SFO_MAGIC = b"\x00PSF"
SFO_FORMAT_UINT32 = 0x0404


def _validate_game_path(cartridge_path, game):
    required_fields = ["id", "title", "platform", "file"]

    for field in required_fields:
        if not isinstance(game.get(field), str) or not game[field].strip():
            raise ValueError(f"Missing or invalid game field: {field}")

    game_path = (cartridge_path / game["file"]).resolve()

    if not game_path.is_relative_to(cartridge_path):
        raise ValueError("Invalid game file path")

    platform = game["platform"].strip().upper()

    if platform == "PS3":
        if not (
            game_path.is_dir()
            and (game_path / "PS3_GAME").is_dir()
            and (game_path / "PS3_GAME" / "PARAM.SFO").is_file()
        ):
            raise FileNotFoundError("Invalid PS3 game directory")
    elif not game_path.is_file():
        raise FileNotFoundError("Game file not found")

    return game_path


def _read_param_sfo(sfo_path):
    content = Path(sfo_path).read_bytes()

    if content[:4] != SFO_MAGIC:
        raise ValueError("Invalid PARAM.SFO")

    key_table, data_table, entry_count = struct.unpack_from("<III", content, 8)
    fields = {}

    for index in range(entry_count):
        key_offset, value_format, length, _, data_offset = struct.unpack_from(
            "<HHIII", content, 20 + index * 16
        )

        key_start = key_table + key_offset
        key = content[key_start:content.index(b"\x00", key_start)].decode("ascii")
        value_start = data_table + data_offset

        if value_format == SFO_FORMAT_UINT32:
            fields[key] = struct.unpack_from("<I", content, value_start)[0]
        else:
            fields[key] = (
                content[value_start:value_start + length]
                .rstrip(b"\x00")
                .decode("utf-8", errors="replace")
            )

    return fields


def _add_ps3_details(game, game_path, cartridge_path):
    ps3_game = game_path / "PS3_GAME"
    cover = ps3_game / "ICON0.PNG"

    if cover.is_file():
        game["cover"] = str(cover.relative_to(cartridge_path))

    try:
        sfo = _read_param_sfo(ps3_game / "PARAM.SFO")
    except (OSError, ValueError, struct.error):
        sfo = {}

    if sfo.get("TITLE"):
        game.setdefault("title", sfo["TITLE"])

    game.setdefault("title_id", sfo.get("TITLE_ID", ""))
    game.setdefault("version", sfo.get("VERSION", ""))


def _read_manifest(metadata_file, cartridge_path):
    with open(metadata_file, "r", encoding="utf-8") as file:
        metadata = json.load(file)

    if not isinstance(metadata, dict):
        raise ValueError("Invalid cartridge metadata")

    entries = metadata.get("games")
    if entries is None and isinstance(metadata.get("game"), dict):
        entries = [metadata["game"]]

    if not isinstance(entries, list):
        raise ValueError("Invalid game information")

    games = []
    last_error = ValueError("No games listed in cartridge metadata")

    # One broken entry should not hide the other games on the cartridge.
    for game in entries:
        if not isinstance(game, dict):
            continue

        try:
            game_path = _validate_game_path(cartridge_path, game)
        except (ValueError, FileNotFoundError) as error:
            last_error = error
            continue

        if game["platform"].strip().upper() == "PS3":
            _add_ps3_details(game, game_path, cartridge_path)

        games.append(game)

    if not games:
        raise last_error

    cartridge_id = metadata.get("cartridge_id", f"USB-{cartridge_path.name}")
    return cartridge_id, games


def _detect_ps3_games(cartridge_path):
    candidates = []

    if (cartridge_path / "PS3_GAME" / "PARAM.SFO").is_file():
        candidates.append(cartridge_path)

    for child in cartridge_path.iterdir():
        if child.is_dir() and (child / "PS3_GAME" / "PARAM.SFO").is_file():
            candidates.append(child)

    if not candidates:
        raise FileNotFoundError("No PS3 game found")

    games = []

    for game_path in candidates:
        folder_name = game_path.name
        game_id = re.sub(r"[^a-z0-9]+", "-", folder_name.lower()).strip("-")

        game = {
            "id": game_id or "ps3-game",
            "platform": "PS3",
            "file": str(game_path.relative_to(cartridge_path)) or ".",
            "cover": None,
        }

        _add_ps3_details(game, game_path, cartridge_path)
        game.setdefault("title", folder_name or "PS3 Game")
        games.append(game)

    return games


def read_metadata(cartridge_path: Path):
    cartridge_path = Path(cartridge_path).resolve()
    metadata_file = cartridge_path / "cartridge.json"

    if metadata_file.is_file():
        cartridge_id, games = _read_manifest(metadata_file, cartridge_path)
    else:
        cartridge_id = f"USB-{cartridge_path.name}"
        games = _detect_ps3_games(cartridge_path)

    # Games are launched by id, so ids must be unique on a cartridge.
    used_ids = set()
    for game in games:
        base_id = game["id"]
        suffix = 2

        while game["id"] in used_ids:
            game["id"] = f"{base_id}-{suffix}"
            suffix += 1

        used_ids.add(game["id"])

    return {"cartridge_id": cartridge_id, "games": games}
