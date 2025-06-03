#include "modding.h"
#include "global.h"
#include "recomputils.h"
#include "recompconfig.h"
#include "segmented_address.h"
#include "segment_symbols.h"
#include "volcano_fix_pos.h"
#include "volcano_cleared_fix_pos.h"
#include "modding.h"
#include "global.h"
#include "recomputils.h"
#include "recompconfig.h"

#include "overlays/actors/ovl_Bg_Breakwall/z_bg_breakwall.h"

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

DECLARE_ROM_SEGMENT(object_keikoku_obj);
extern Gfx object_keikoku_obj_DL_0027D8[]; // uncleared volcano
extern Gfx object_keikoku_obj_DL_0044A8[]; // cleared volcano

void* gRam;
uintptr_t gVrom;
size_t gSize;
RECOMP_HOOK("DmaMgr_ProcessRequest") void on_DmaMgr_RequestSync(DmaRequest* req) {
    gRam = req->dramAddr;
    gVrom = req->vromAddr;
    gSize = req->size;
}

RECOMP_HOOK_RETURN("DmaMgr_ProcessRequest") void after_dma() {
    if (gVrom == SEGMENT_ROM_START(object_keikoku_obj)) {
        uintptr_t old_segment_6 = gSegments[0x06];
        gSegments[0x06] = OS_K0_TO_PHYSICAL(gRam);
        Gfx* to_patch = Lib_SegmentedToVirtual(object_keikoku_obj_DL_0027D8);
        Gfx* to_patch2 = Lib_SegmentedToVirtual(object_keikoku_obj_DL_0044A8);
        gSPBranchList(to_patch , volcano_fix_pos);
        gSPBranchList(to_patch2 , volcano_cleared_fix_pos);
        gSegments[0x06] = old_segment_6;
    }
    gVrom = 0;
    gRam = NULL;
    }

typedef struct {
    /* 0x00 */ s16 objectId;
    /* 0x04 */ Gfx* unk_04;
    /* 0x08 */ Gfx* unk_08;
    /* 0x0C */ AnimatedMaterial* unk_0C;
    /* 0x10 */ CollisionHeader* unk_10;
    /* 0x14 */ BgBreakwallUnkFunc unk_14;
    /* 0x18 */ BgBreakwallUnkFunc2 unk_18;
    /* 0x1C */ ActorFunc unk_1C;
} BgBreakwallStruct; // size = 0x20

extern BgBreakwallStruct D_808B8140[];
u32 D_808B8140_entries = 13;
extern InitChainEntry Bg_Breakwall_sInitChain[];

extern void BgBreakwall_SetupAction(BgBreakwall* this, BgBreakwallActionFunc actionFunc);
extern void func_808B76CC(BgBreakwall* this, PlayState* play);

// Uncleared;
extern bool func_808B73FC(BgBreakwall* this, PlayState* play);
extern void func_808B77D0(BgBreakwall* this, PlayState* play);

extern bool func_808B736C(BgBreakwall* this, PlayState* play);
extern void func_808B77D0(BgBreakwall* this, PlayState* play);

extern AnimatedMaterial object_keikoku_obj_Matanimheader_004290[];

RECOMP_PATCH void BgBreakwall_Init(Actor* thisx, PlayState* play) {
    s32 pad;
    BgBreakwall* this = (BgBreakwall*)thisx;
    BgBreakwallStruct* sp24 = &D_808B8140[BGBREAKWALL_GET_F(&this->dyna.actor)];

    if (sp24->unk_04 == object_keikoku_obj_DL_0027D8 && CHECK_WEEKEVENTREG(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE)) {
        D_808B8140[2].unk_04 = object_keikoku_obj_DL_0044A8;
        D_808B8140[2].unk_0C = NULL;
        D_808B8140[2].unk_14 = func_808B736C;
        D_808B8140[2].unk_18 = func_808B77D0;
    } else if (sp24->unk_04 == object_keikoku_obj_DL_0044A8 && !(CHECK_WEEKEVENTREG(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE))) {
        D_808B8140[2].unk_04 = object_keikoku_obj_DL_0027D8;
        D_808B8140[2].unk_0C = object_keikoku_obj_Matanimheader_004290;
        D_808B8140[2].unk_14 = func_808B73FC;
        D_808B8140[2].unk_18 = func_808B77D0;
    }

    Actor_ProcessInitChain(&this->dyna.actor, Bg_Breakwall_sInitChain);
    this->objectSlot = Object_GetSlot(&play->objectCtx, sp24->objectId);

    if ((this->objectSlot <= OBJECT_SLOT_NONE) || !sp24->unk_14(this, play)) {
        Actor_Kill(&this->dyna.actor);
        return;
    }

    BgBreakwall_SetupAction(this, func_808B76CC);
    this->switchFlag = BGBREAKWALL_SWITCH_FLAG(&this->dyna.actor);
}
