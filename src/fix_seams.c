#include "modding.h"
#include "global.h"
#include "recomputils.h"
#include "recompconfig.h"
#include "segmented_address.h"
#include "segment_symbols.h"

#define DEFINE_SCENE(name, _enumValue, textId, drawConfig, _restrictionFlags, _persistentCycleFlags) \
    DECLARE_ROM_SEGMENT(name)

#define DEFINE_SCENE_UNSET(_enumValue)

#include "tables/scene_table.h"

#undef DEFINE_SCENE

extern Vtx Z2_CLOCKTOWER_room_00Vtx_002A90[];
extern Gfx Z2_CLOCKTOWER_room_00DL_0032D0[];


#define MAX_SCAN 10000

bool replace_dl_commands_jump(Gfx* to_scan, Gfx* to_find, Gfx* target, u32 length) {
    Gfx* cur_cmd = to_scan;
    for (u32 i = 0; i < MAX_SCAN; i++) {
        // If the current command's words match the words of the first target command, check the remaining commands.
        if (cur_cmd->words.w0 == to_find[0].words.w0 && cur_cmd->words.w1 == to_find[0].words.w1) {
            // Check the remaining commands.
            bool didMatch = true;
            for (u32 commandIndex = 1; commandIndex < length; commandIndex++) {
                if (cur_cmd[commandIndex].words.w0 != to_find[commandIndex].words.w0 || cur_cmd[commandIndex].words.w1 != to_find[commandIndex].words.w1) {
                    didMatch = false;
                    break;
                }
            }
            // If all commands matched, perform the replacement and return true.
            if (didMatch) {
                gSPDisplayList(cur_cmd, target);
                // Replace the remaining commands with noops.
                for (u32 commandIndex = 1; commandIndex < length; commandIndex++) {
                    gSPNoOp(&cur_cmd[commandIndex]);
                }
                return true;
            }
        }
        switch (cur_cmd->words.w0) {
            case G_ENDDL:
                // Reached the end of the displaylist without finding the target command.
                return false;
            case G_DL:
                // If this is a branchlist (G_DL_NOPUSH) then treat it as the end of a displaylist.
                {
                    u32 push = (cur_cmd->words.w0 >> 16) & 0xFF;
                    if (push == G_DL_NOPUSH) {
                        return false;
                    }
                }
            default:
                // Just continue forward for any other command.
                break;
        }

        // Go to the next command.
        cur_cmd++;
    }
    recomp_printf("didnt find\n");
    return false;
}

PlayState* gPlay;
RoomContext* gRoomCtx;
s8 gRoomStatus;

RECOMP_HOOK("Room_ProcessRoomRequest") void on_process_room_request(PlayState* play, RoomContext* roomCtx) {
    gPlay = play;
    gRoomCtx = roomCtx;
    gRoomStatus = roomCtx->status;
}

Gfx cringeDL[] = {
    gsSPVertex(&Z2_CLOCKTOWER_room_00Vtx_002A90[14], 32, 0),
    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),
};

Vtx ramp_vertices[5] = {
	{{ {-393, 200, -1253}, 0, {1253, -2385}, {208, 118, 0, 255} }},
	{{ {-393, 200, -1400}, 0, {0, -2385}, {208, 118, 0, 255} }},
	{{ {-513, 151, -1253}, 0, {1253, -727}, {208, 118, 0, 255} }},
	{{ {-640, 100, -1400}, 0, {0, 1024}, {208, 118, 0, 255} }},
	{{ {-640, 100, -1253}, 0, {1253, 1024}, {208, 118, 0, 255} }},
};

Gfx basedRamp[] = {
	gsSPVertex(ramp_vertices + 0, 5, 0),
	gsSP2Triangles(0, 1, 2, 0, 1, 3, 2, 0),
	gsSP1Triangle(3, 4, 2, 0),
    gsSPEndDisplayList(),
};

Gfx basedDL[] = {
    gsSPDisplayList(basedRamp),
    gsSPVertex(&Z2_CLOCKTOWER_room_00Vtx_002A90[14], 32, 0),
    gsSPEndDisplayList(),
};

RECOMP_HOOK_RETURN("Room_ProcessRoomRequest") void after_process_room_request() {
    // Check if the room was just loaded.
    if (gRoomCtx->status == 0 && gRoomStatus == 1) {
        // Check if it's the scene we're editing.
        if (gPlay->sceneId == SCENE_CLOCKTOWER) {
            // Check if it's the room we're editing.
            if (gRoomCtx->curRoom.num == 0) {
                gSegments[0x03] = OS_K0_TO_PHYSICAL(gRoomCtx->curRoom.segment);
                Gfx* to_scan = (Gfx*)Lib_SegmentedToVirtual(Z2_CLOCKTOWER_room_00DL_0032D0);
                replace_dl_commands_jump(to_scan, cringeDL, basedDL, ARRAY_COUNT(cringeDL));
            }
        }
    }
}
