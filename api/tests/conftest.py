
import json
import struct

import pytest


def build_param_sfo(fields):
    keys = b""
    values = b""
    entries = b""

    for key, value in fields.items():
        encoded = value.encode("utf-8") + b"\x00"
        entries += struct.pack(
            "<HHIII", len(keys), 0x0204, len(encoded), len(encoded), len(values)
        )
        keys += key.encode("ascii") + b"\x00"
        values += encoded

    key_table = 20 + len(entries)
    data_table = key_table + len(keys)
    header = b"\x00PSF" + struct.pack("<IIII", 0x0101, key_table, data_table, len(fields))

    return header + entries + keys + values


def make_ps3_game(folder, sfo_fields=None, with_cover=False):
    ps3_game = folder / "PS3_GAME"
    ps3_game.mkdir(parents=True)

    sfo = build_param_sfo(sfo_fields) if sfo_fields else b""
    (ps3_game / "PARAM.SFO").write_bytes(sfo)

    if with_cover:
        (ps3_game / "ICON0.PNG").write_bytes(b"png")


@pytest.fixture
def valid_cartridge(tmp_path):

    cartridge = tmp_path / "cartridge"
    make_ps3_game(cartridge / "games" / "Crash Bandicoot")

    metadata = {
        "cartridge_id": "CART-001",
        "game": {
            "id": "crash-bandicoot",
            "title": "Crash Bandicoot",
            "platform": "PS3",
            "file": "games/Crash Bandicoot",
            "cover": "cover.jpg"
        }
    }

    with open(cartridge / "cartridge.json", "w") as file:
        json.dump(metadata, file)

    return cartridge
