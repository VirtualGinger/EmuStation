
from pathlib import Path

from fastapi import APIRouter, HTTPException, Request

router = APIRouter(prefix="/api", tags=["Game Launch"])


@router.post("/cartridge/launch")
def launch_game(request: Request):
    app = request.app

    cartridge_manager = app.state.cartridge_manager
    emulator_manager = app.state.emulator_manager
    process_manager = app.state.process_manager

    # Check cartridge status.
    status = cartridge_manager.get_status()

    if status["status"] != "ready":
        raise HTTPException(
            status_code=400,
            detail="No valid game cartridge inserted"
        )

    game = status["game"]

    # Resolve and validate the game path.
    cartridge_path = Path(cartridge_manager.cartridge_path).resolve()
    game_path = (cartridge_path / game["file"]).resolve()

    if not game_path.is_relative_to(cartridge_path):
        raise HTTPException(
            status_code=400,
            detail="Invalid game path"
        )

    platform = game["platform"].strip().upper()

    if platform == "PS3":
        # RPCS3 needs the game directory containing PS3_GAME.
        if not (
            game_path.is_dir()
            and (game_path / "PS3_GAME").is_dir()
            and (game_path / "PS3_GAME" / "PARAM.SFO").is_file()
        ):
            raise HTTPException(
                status_code=400,
                detail="Invalid PS3 game directory"
            )

    else:
        # Other platforms currently use individual game files.
        if not game_path.is_file():
            raise HTTPException(
                status_code=400,
                detail="Game file is missing or invalid"
            )

    # Select emulator and prepare command.
    try:
        command = emulator_manager.build_command(
            platform,
            game_path
        )
    except ValueError as error:
        raise HTTPException(
            status_code=400,
            detail=str(error)
        )

    # Start emulator.
    try:
        result = process_manager.start(command)
    except RuntimeError as error:
        raise HTTPException(
            status_code=409,
            detail=str(error)
        )
    except OSError as error:
        raise HTTPException(
            status_code=503,
            detail=f"Unable to start emulator: {error}"
        )

    return {
        "message": "Emulator started",
        "emulator": command[0],
        "game": game["title"],
        "process": result
    }


@router.get("/launch/status")
def get_launch_status(request: Request):
    return request.app.state.process_manager.get_status()


@router.post("/launch/stop")
def stop_game(request: Request):
    return request.app.state.process_manager.stop()
