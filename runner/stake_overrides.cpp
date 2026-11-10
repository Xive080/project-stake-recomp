// Overrides propios de Project Stake para el ELF SCES_504.11 (PAL). Mismo mecanismo que usan otros proyectos de PS2Recomp:
// un fichero aparte del runtime, registrado con PS2_REGISTER_GAME_OVERRIDE. Cada override explica por que existe.
#include "game_overrides.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include <chrono>
#include <thread>

namespace
{
    // ADXM_WaitVsync (0x16D690): despierta al hilo de vsync de ADX y duerme el hilo principal hasta que el vsync lo despierta.
    // El runtime todavia no entrega esa interrupcion al hilo de ADX, asi que el principal dormia para siempre (reproduciendo la
    // pelicula de arranque). Se sustituye por una espera de un fotograma.
    void ovAdxmWaitVsync(uint8_t *, R5900Context *ctx, PS2Runtime *)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        ctx->pc = GPR_U32(ctx, 31);
    }

    void applyStake(PS2Runtime &runtime)
    {
        runtime.replaceFunction(0x0016D690u, ovAdxmWaitVsync);
    }
}

PS2_REGISTER_GAME_OVERRIDE("Project Stake (SCES_504.11)", "SCES_504.11", 0x00140008u, 0u, applyStake)
