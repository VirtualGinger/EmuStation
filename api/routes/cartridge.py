
from pathlib import Path

from fastapi import APIRouter, HTTPException, Request
from fastapi.responses import FileResponse

router = APIRouter(prefix="/api/cartridge", tags=["Cartridge"])


@router.get("/status")
def get_cartridge_status(request: Request):
    manager = request.app.state.cartridge_manager
    return manager.get_status()


@router.get("/game")
def get_cartridge_game(request: Request):
    manager = request.app.state.cartridge_manager
    status = manager.get_status()

    if status["status"] != "ready":
        raise HTTPException(
            status_code=404,
            detail="No valid game cartridge inserted"
        )

    return status["game"]


@router.get("/cover")
def get_cartridge_cover(request: Request):
    manager = request.app.state.cartridge_manager
    status = manager.get_status()

    if status["status"] != "ready":
        raise HTTPException(status_code=404, detail="No game inserted")

    game = status["game"]
    cover = game.get("cover")

    if not cover or not manager.cartridge_path:
        raise HTTPException(
            status_code=404,
            detail="No cover image available"
        )

    root = Path(manager.cartridge_path).resolve()
    image_path = (root / cover).resolve()

    if not image_path.is_relative_to(root):
        raise HTTPException(status_code=400, detail="Invalid cover path")

    if not image_path.is_file():
        raise HTTPException(status_code=404, detail="Cover image not found")

    return FileResponse(image_path, media_type="image/png")
