
import json
from pathlib import Path


def read_metadata(cartridge_path: Path):

    cartridge_path = Path(cartridge_path).resolve()
    metadata_file = cartridge_path / "cartridge.json"

    if not metadata_file.is_file():
        raise FileNotFoundError("Cartridge metadata not found")

    with open(metadata_file, "r", encoding="utf-8") as file:
        metadata = json.load(file)

    if not isinstance(metadata, dict):
        raise ValueError("Invalid cartridge metadata format")

    if not metadata.get("cartridge_id"):
        raise ValueError("Missing cartridge ID")

    game = metadata.get("game")

    if not isinstance(game, dict):
        raise ValueError("Invalid game information")

    required_fields = ["id", "title", "platform", "file"]

    for field in required_fields:
        if not isinstance(game.get(field), str) or not game[field].strip():
            raise ValueError(f"Missing or invalid game field: {field}")

    game_path = (cartridge_path / game["file"]).resolve()

    # Prevent accessing files outside the cartridge.
    if not game_path.is_relative_to(cartridge_path):
        raise ValueError("Invalid game file path")

    if not game_path.is_file():
        raise FileNotFoundError("Game file not found")

    return metadata
