from fastapi import APIRouter, HTTPException, Request

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