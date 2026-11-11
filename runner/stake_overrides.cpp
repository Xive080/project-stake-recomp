// Overrides propios de Project Stake para el ELF SCES_504.11 (PAL). Mismo mecanismo que usan otros proyectos de PS2Recomp:
// un fichero aparte del runtime, registrado con PS2_REGISTER_GAME_OVERRIDE. Cada override explica por que existe.
#include "game_overrides.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h> // funciones generadas a las que reenvian las sustituciones
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <thread>

namespace
{
    // apADXPS2_UpdateStatus (0x1E9AA0): ejecuta el servidor de ADX solo si el contador de vblank del juego (gp-0x7250, lo sube
    // IntrFunc_VBLKIN) cambio desde la ultima vez (gp-0x7274). Los bucles de carga (FileMgrClass, EntryDatClass, LoadRomData,
    // apADXPS2 GetStat) la llaman en un bucle cerrado junto a ADXF_GetPtStat/GetStat: en PS2 eso es una espera activa hasta el
    // siguiente vblank. Recompilado, cada vuelta cuesta mucho tiempo de host y adelanta muy pocos ciclos del EE, asi que el
    // vblank tardaba ~85k vueltas (~928k vueltas para la particion 0). Si no hay vblank nuevo, en vez de volver a girar se duerme
    // el hilo hasta el siguiente vblank y se vuelve al llamador: mismo resultado logico (no habria hecho nada), sin el giro.
    // El bucle de fotograma principal (retorno 0x1F2DD0) no se toca.
    void ovApAdxUpdateStatus(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t gp = GPR_U32(ctx, 28);
        const uint32_t ra = GPR_U32(ctx, 31);
        uint32_t seen, vb;
        std::memcpy(&seen, rdram + ((gp - 0x7274u) & 0x1FFFFFFu), 4);
        std::memcpy(&vb, rdram + ((gp - 0x7250u) & 0x1FFFFFFu), 4);
        if (seen == vb && ra != 0x001F2DD0u)
            runtime->eeWaitVSyncTicks(1u, ra);
        apADXPS2_UpdateStatus__Fv_0x1e9aa0(rdram, ctx, runtime); // original: hay vblank nuevo, o es el bucle principal
    }

    // ADXM_WaitVsync (0x16D690): despierta al hilo de vsync de ADX y duerme el hilo principal hasta que el vsync lo despierta.
    // El runtime todavia no entrega esa interrupcion al hilo de ADX, asi que el principal dormia para siempre (reproduciendo la
    // pelicula de arranque). Se sustituye por una espera de un fotograma.
    void ovAdxmWaitVsync(uint8_t *, R5900Context *ctx, PS2Runtime *runtime)
    {
        runtime->eeWaitVSyncTicks(1u, GPR_U32(ctx, 31)); // espera de vsync guiada por el planificador
    }

    // Video (Sofdec) por software, sin IPU ni FFmpeg: multiplataforma.
    // mwPlySfdCreate (0x18F004) llama SFD_SetSceMpeg(1) de forma fija, asi que Sofdec decodifica con la libreria sceMpeg (IPU)
    // y entrega RGB32 por macrobloques. Este runtime no modela BDEC/VDEC de la IPU, asi que se fuerza SFD_SetSceMpeg(0): Sofdec usa
    // entonces su decodificador software de CRI (MPV_DecodeFrm) en el EE, que deja el fotograma en YCbCr 4:2:0 plano.
    void ovSfdSetSceMpeg(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        SET_GPR_U32(ctx, 4, 0u);
        SFD_SetSceMpeg_0x199dc0(rdram, ctx, runtime);
    }

    // SFMPV_Create (0x198598) lee el campo 0x38 del contexto SFMPV y lo pasa como condicion 6 de MPV_SetCond. sfmpv_SetFrmPara
    // (0x1980E8) solo calcula los punteros de planos de referencia (Cb/Cr/Y que MPV_DecodeFrm copia a su objeto en 0x1D0..0x1EF y
    // que mpvcmc_OutputIntra6blk usa como destino) cuando ese campo vale 3. Con el valor por defecto (2) la tabla de planos queda a
    // cero, el decodificador escribe en la direccion 0 + offset y el fotograma sale como basura (franjas de color). Con 3 el primer
    // fotograma de M_SELECT coincide byte a byte con el de FFmpeg.
    void ovSfmpvCreate(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t hn = GPR_U32(ctx, 4);
        WRITE32(hn + 0x38u, 3u);
        SFMPV_Create_0x198598(rdram, ctx, runtime);
    }

    // UsrLoadMvFrm (0x2159A0): sube un fotograma de pelicula a VRAM. Espera RGBA32 en orden de macrobloque (lo que deja la IPU con
    // CSC32): columnas de 16 px de ancho, cada una de a2 filas, y las lanza con nlPutLocalMem (psm 0, x = 16*i). Con el decodificador
    // software el fotograma esta en planos YCbCr 4:2:0 (Y de ancho redondeado a 64, Cb y Cr a 128/64 por ancho de MB), asi que
    // aqui se hace la conversion de color (CSC de la IPU: Y-16, coeficientes 0x95/0xCC/-0x68/-0x32/0x102 sobre 128, alfa 0x80)
    // columna a columna en un hueco del propio bufer del fotograma (reservado para RGBA32 y sin uso tras los planos).
    void ovUsrLoadMvFrm(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const uint32_t buf = GPR_U32(ctx, 4);
        const int32_t width = static_cast<int32_t>(GPR_U32(ctx, 5));
        const int32_t height = static_cast<int32_t>(GPR_U32(ctx, 6));
        const uint32_t tex = GPR_U32(ctx, 7);
        const int32_t strips = (width + (width < 0 ? 15 : 0)) >> 4;
        const int32_t mbw = (width + 15) >> 4;
        const int32_t mbh = (height + 15) >> 4;
        const uint32_t yStride = static_cast<uint32_t>((mbw + 3) >> 2) << 6;
        const uint32_t cStride = static_cast<uint32_t>((mbw + 7) >> 3) << 6;
        const uint32_t ySize = yStride * static_cast<uint32_t>(mbh) * 16u;
        const uint32_t cSize = cStride * static_cast<uint32_t>(mbh) * 8u;
        const uint32_t scratch = (buf + ySize + 2u * cSize + 15u) & ~15u;
        const uint32_t stripBytes = 16u * static_cast<uint32_t>(height) * 4u;
        const uint32_t desc = scratch + stripBytes; // descriptor de nlPutLocalMem (24 bytes)
        if (strips <= 0 || height <= 0 || desc + 32u > buf + static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 4u)
            return; // formato inesperado: no se sube nada

        const uint8_t *yPlane = rdram + (buf & 0x1FFFFFFu);
        const uint8_t *cbPlane = yPlane + ySize;
        const uint8_t *crPlane = cbPlane + cSize;
        uint8_t *out = rdram + (scratch & 0x1FFFFFFu);
        const int32_t tbp = static_cast<int32_t>(READ32(tex + 8u));
        const uint16_t dbp = static_cast<uint16_t>((tbp >= 0 ? tbp : tbp + 0xFF) >> 8);
        const uint16_t dbw = static_cast<uint16_t>((width + (width < 0 ? 0x3F : 0)) >> 6);

        auto clamp8 = [](int v) -> uint8_t { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); };
        for (int32_t strip = 0; strip < strips; ++strip)
        {
            for (int32_t y = 0; y < height; ++y)
            {
                const uint8_t *yRow = yPlane + static_cast<size_t>(y) * yStride + static_cast<size_t>(strip) * 16u;
                const uint8_t *cbRow = cbPlane + static_cast<size_t>(y >> 1) * cStride + static_cast<size_t>(strip) * 8u;
                const uint8_t *crRow = crPlane + static_cast<size_t>(y >> 1) * cStride + static_cast<size_t>(strip) * 8u;
                uint8_t *dst = out + static_cast<size_t>(y) * 64u;
                for (int x = 0; x < 16; ++x, dst += 4)
                {
                    const int cb = static_cast<int>(cbRow[x >> 1]) - 128;
                    const int cr = static_cast<int>(crRow[x >> 1]) - 128;
                    const int lum = (0x95 * (std::max(static_cast<int>(yRow[x]), 16) - 16)) >> 7;
                    dst[0] = clamp8(lum + ((0xCC * cr) >> 7));
                    dst[1] = clamp8(lum + ((-0x68 * cr - 0x32 * cb) >> 7));
                    dst[2] = clamp8(lum + ((0x102 * cb) >> 7));
                    dst[3] = 0x80;
                }
            }
            // descriptor de nlPutLocalMem: mismos campos que arma UsrLoadMvFrm en su pila
            WRITE32(desc, scratch);
            WRITE16(desc + 4u, dbp);
            WRITE16(desc + 6u, dbw);
            WRITE16(desc + 8u, 0u);
            WRITE16(desc + 10u, static_cast<uint16_t>(strip * 16));
            WRITE16(desc + 12u, 0u);
            WRITE16(desc + 14u, 16u);
            WRITE16(desc + 16u, static_cast<uint16_t>(height));
            SET_GPR_U32(ctx, 4, desc);
            nlPutLocalMem_0x1a7280(rdram, ctx, runtime);
            // espera a que termine la DMA del canal 2 (GIF), igual que el bucle original, antes de reutilizar el hueco
            for (uint32_t spin = 0; (READ32(0x10009000u) & 0x100u) != 0u && spin < 50000000u; ++spin)
                std::this_thread::yield();
        }
    }

    void applyStake(PS2Runtime &runtime)
    {
        runtime.replaceFunction(0x001E9AA0u, ovApAdxUpdateStatus);
        if (std::getenv("STAKE_SFD_IPU") == nullptr) // por defecto video por software; STAKE_SFD_IPU=1 vuelve a la ruta sceMpeg/IPU
        {
            runtime.replaceFunction(0x00199DC0u, ovSfdSetSceMpeg);
            runtime.replaceFunction(0x00198598u, ovSfmpvCreate);
            runtime.replaceFunction(0x002159A0u, ovUsrLoadMvFrm);
        }
        (void)ovAdxmWaitVsync; // desactivado: el cuelgue real era la prioridad 0 rechazada en CreateThread
    }
}

PS2_REGISTER_GAME_OVERRIDE("Project Stake (SCES_504.11)", "SCES_504.11", 0x00140008u, 0u, applyStake)
