
import json
import re
from pathlib import Path


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


def _detect_ps3_game(cartridge_path):
    candidates = []

    if (cartridge_path / "PS3_GAME" / "PARAM.SFO").is_file():
        candidates.append(cartridge_path)

    try:
        for child in cartridge_path.iterdir():
            if (
                child.is_dir()
                and (child / "PS3_GAME" / "PARAM.SFO").is_file()
            ):
                candidates.append(child)
    except OSError:
        return None

    if len(candidates) != 1:
        return None

    game_path = candidates[0]

    title = game_path.name if game_path != cartridge_path else cartridge_path.name
    game_id = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")

    return {
        "cartridge_id": f"USB-{cartridge_path.name}",
        "game": {
            "id": game_id or "ps3-game",
            "title": title,
            "platform": "PS3",
            "file": str(game_path.relative_to(cartridge_path)) or ".",
        }
    }


def read_metadata(cartridge_path: Path):
    cartridge_path = Path(cartridge_path).resolve()
    metadata_file = cartridge_path / "cartridge.json"

    # Preserve support for manually configured cartridges.
    if metadata_file.is_file():
        with open(metadata_file, "r", encoding="utf-8") as file:
            metadata = json.load(file)

        if not isinstance(metadata, dict):
            raise ValueError("Invalid cartridge metadata")

        game = metadata.get("game")
        if not isinstance(game, dict):
            raise ValueError("Invalid game information")

        for field in ("id", "title", "platform", "file"):
            if not isinstance(game.get(field), str) or not game[field].strip():
                raise ValueError(f"Invalid game field: {field}")

        game_path = (cartridge_path / game["file"]).resolve()

        if not game_path.is_relative_to(cartridge_path):
            raise ValueError("Invalid game path")

        if game["platform"].upper() == "PS3":
            valid = (
                game_path.is_dir()
                and (game_path / "PS3_GAME" / "PARAM.SFO").is_file()
            )
        else:
            valid = game_path.is_file()

        if not valid:
            raise FileNotFoundError("Game is missing or invalid")

        cover = game_path / "PS3_GAME" / "ICON0.PNG"
        if cover.is_file():
            game["cover"] = str(cover.relative_to(cartridge_path))

        return metadata

    # Automatically detect an unpacked PS3 game.
    candidates = []

    if (cartridge_path / "PS3_GAME" / "PARAM.SFO").is_file():
        candidates.append(cartridge_path)

    for child in cartridge_path.iterdir():
        if child.is_dir() and (
            child / "PS3_GAME" / "PARAM.SFO"
        ).is_file():
            candidates.append(child)

    if len(candidates) != 1:
        raise FileNotFoundError("No unique PS3 game found")

    game_path = candidates[0]
    title = game_path.name
    cover_path = game_path / "PS3_GAME" / "ICON0.PNG"

    game_path = candidates[0]
    title = game_path.name if game_path != cartridge_path else cartridge_path.name

    game_id = re.sub(
        r"[^a-z0-9]+",
        "-",
        title.lower()
    ).strip("-")

    cover_path = game_path / "PS3_GAME" / "ICON0.PNG"

    return {
        "cartridge_id": f"USB-{cartridge_path.name}",
        "game": {
            "id": game_id or "ps3-game",
            "title": title,
            "platform": "PS3",
            "file": str(game_path.relative_to(cartridge_path)) or ".",
            "cover": (
                str(cover_path.relative_to(cartridge_path))
                if cover_path.is_file()
                else None
            )
        }
    }
