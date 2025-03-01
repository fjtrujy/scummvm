/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Save and restore scene and game.
 */


#include "tinsel/actors.h"
#include "tinsel/background.h"
#include "tinsel/config.h"
#include "tinsel/drives.h"
#include "tinsel/dw.h"
#include "tinsel/faders.h"		// FadeOutFast()
#include "tinsel/graphics.h"		// ClearScreen()
#include "tinsel/handle.h"
#include "tinsel/dialogs.h"
#include "tinsel/music.h"
#include "tinsel/pid.h"
#include "tinsel/play.h"
#include "tinsel/polygons.h"
#include "tinsel/movers.h"
#include "tinsel/savescn.h"
#include "tinsel/scene.h"
#include "tinsel/sched.h"
#include "tinsel/scroll.h"
#include "tinsel/sound.h"
#include "tinsel/sysvar.h"
#include "tinsel/tinlib.h"
#include "tinsel/token.h"

#include "common/textconsole.h"

namespace Tinsel {

//----------------- EXTERN FUNCTIONS --------------------

// In DOS_DW.C
void RestoreMasterProcess(INT_CONTEXT *pic);

// in EVENTS.C (declared here and not in events.h because of strange goings-on)
void RestoreProcess(INT_CONTEXT *pic);

// in SCENE.C
extern SCNHANDLE GetSceneHandle();


//----------------- LOCAL DEFINES --------------------

enum {
	RS_COUNT = 5,	// Restore scene count

	MAX_NEST = 4
};

//----------------- EXTERNAL GLOBAL DATA --------------------

extern int	g_thingHeld;
extern int	g_restoreCD;
extern SRSTATE g_SRstate;

//----------------- LOCAL GLOBAL DATA --------------------

// These vars are reset upon engine destruction

bool g_ASceneIsSaved = false;

static int g_savedSceneCount = 0;

static bool g_bNotDoneYet = false;

static SAVED_DATA *g_ssData = nullptr;
static SAVED_DATA g_sgData;
static SAVED_DATA *g_rsd = nullptr;

static int g_RestoreSceneCount = 0;

static bool g_bNoFade = false;

void ResetVarsSaveScn() {
	g_ASceneIsSaved = false;

	g_savedSceneCount = 0;

	g_bNotDoneYet = false;

	free(g_ssData);
	g_ssData = nullptr;

	memset(&g_sgData, 0, sizeof(g_sgData));
	g_rsd = nullptr;

	g_RestoreSceneCount = 0;
	g_bNoFade = false;
}

/**
 * Save current scene.
 * @param sd			Pointer to the scene data
 */
void DoSaveScene(SAVED_DATA *sd) {
	sd->SavedSceneHandle = GetSceneHandle();
	sd->SavedBgroundHandle = _vm->_bg->GetBgroundHandle();
	SaveMovers(sd->SavedMoverInfo);
	sd->NumSavedActors = _vm->_actor->SaveActors(sd->SavedActorInfo);
	_vm->_bg->PlayfieldGetPos(FIELD_WORLD, &sd->SavedLoffset, &sd->SavedToffset);
	SaveInterpretContexts(sd->SavedICInfo);
	sd->SavedControl = ControlIsOn();
	sd->SavedNoBlocking = GetNoBlocking();
	_vm->_scroll->GetNoScrollData(&sd->SavedNoScrollData);

	if (TinselVersion >= 2) {
		// Tinsel 2 specific data save
		_vm->_actor->SaveActorZ(sd->savedActorZ);
		_vm->_actor->SaveZpositions(sd->zPositions);
		SavePolygonStuff(sd->SavedPolygonStuff);
		_vm->_pcmMusic->getTunePlaying(sd->SavedTune, sizeof(sd->SavedTune));
		sd->bTinselDim = _vm->_pcmMusic->getMusicTinselDimmed();
		sd->SavedScrollFocus = _vm->_scroll->GetScrollFocus();
		SaveSysVars(sd->SavedSystemVars);
		SaveSoundReels(sd->SavedSoundReels);

	} else {
		// Tinsel 1 specific data save
		SaveDeadPolys(sd->SavedDeadPolys);
		_vm->_music->CurrentMidiFacts(&sd->SavedMidi, &sd->SavedLoop);
	}

	g_ASceneIsSaved = true;
}

/**
 * Initiate restoration of the saved scene.
 * @param sd			Pointer to the scene data
 * @param bFadeOut		Flag to perform a fade out
 */
void DoRestoreScene(SAVED_DATA *sd, bool bFadeOut) {
	g_rsd = sd;

	if (bFadeOut)
		g_RestoreSceneCount = RS_COUNT + COUNTOUT_COUNT;	// Set restore scene count
	else
		g_RestoreSceneCount = RS_COUNT;	// Set restore scene count
}

void InitializeSaveScenes() {
	if (g_ssData == NULL) {
		g_ssData = (SAVED_DATA *)calloc(MAX_NEST, sizeof(SAVED_DATA));
		if (g_ssData == NULL) {
			error("Cannot allocate memory for scene changes");
		}
	} else {
		// Re-initialize - no scenes saved
		g_savedSceneCount = 0;
	}
}

void FreeSaveScenes() {
	free(g_ssData);
	g_ssData = nullptr;
}

/**
 * Checks that all non-moving actors are playing the same reel as when
 * the scene was saved.
 * Also 'stand' all the moving actors at their saved positions.
 */
void sortActors(SAVED_DATA *sd) {
	assert(TinselVersion <= 1);
	for (int i = 0; i < sd->NumSavedActors; i++) {
		_vm->_actor->ActorsLife(sd->SavedActorInfo[i].actorID, sd->SavedActorInfo[i].bAlive);

		// Should be playing the same reel.
		if (sd->SavedActorInfo[i].presFilm != 0) {
			if (!_vm->_actor->actorAlive(sd->SavedActorInfo[i].actorID))
				continue;

			RestoreActorReels(sd->SavedActorInfo[i].presFilm, sd->SavedActorInfo[i].presRnum, sd->SavedActorInfo[i].zFactor,
					sd->SavedActorInfo[i].presPlayX, sd->SavedActorInfo[i].presPlayY);
		}
	}

	RestoreAuxScales(sd->SavedMoverInfo);
	for (int i = 0; i < MAX_MOVERS; i++) {
		if (sd->SavedMoverInfo[i].bActive)
			Stand(Common::nullContext, sd->SavedMoverInfo[i].actorID, sd->SavedMoverInfo[i].objX,
				sd->SavedMoverInfo[i].objY, sd->SavedMoverInfo[i].hLastfilm);
	}
}

/**
 * Stand all the moving actors at their saved positions.
 * Not called from the foreground.
 */
static void SortMAProcess(CORO_PARAM, const void *) {
	CORO_BEGIN_CONTEXT;
		int i;
		int viaActor;
	CORO_END_CONTEXT(_ctx);


	CORO_BEGIN_CODE(_ctx);

	// Disable via actor for the stands
	_ctx->viaActor = SysVar(ISV_DIVERT_ACTOR);
	SetSysVar(ISV_DIVERT_ACTOR, 0);

	RestoreAuxScales(g_rsd->SavedMoverInfo);

	for (_ctx->i = 0; _ctx->i < MAX_MOVERS; _ctx->i++) {
		if (g_rsd->SavedMoverInfo[_ctx->i].bActive) {
			CORO_INVOKE_ARGS(Stand, (CORO_SUBCTX, g_rsd->SavedMoverInfo[_ctx->i].actorID,
				g_rsd->SavedMoverInfo[_ctx->i].objX, g_rsd->SavedMoverInfo[_ctx->i].objY,
				g_rsd->SavedMoverInfo[_ctx->i].hLastfilm));

			if (g_rsd->SavedMoverInfo[_ctx->i].bHidden)
				HideMover(GetMover(g_rsd->SavedMoverInfo[_ctx->i].actorID));
		}

		ActorPalette(g_rsd->SavedMoverInfo[_ctx->i].actorID,
			g_rsd->SavedMoverInfo[_ctx->i].startColor, g_rsd->SavedMoverInfo[_ctx->i].paletteLength);

		if (g_rsd->SavedMoverInfo[_ctx->i].brightness != BOGUS_BRIGHTNESS)
			ActorBrightness(g_rsd->SavedMoverInfo[_ctx->i].actorID, g_rsd->SavedMoverInfo[_ctx->i].brightness);
	}

	// Restore via actor
	SetSysVar(ISV_DIVERT_ACTOR, _ctx->viaActor);

	g_bNotDoneYet = false;

	CORO_END_CODE;
}


//---------------------------------------------------------------------------

void ResumeInterprets() {
	// Master script only affected on restore game, not restore scene
	if (TinselVersion <= 1) {
		if (g_rsd == &g_sgData) {
			CoroScheduler.killMatchingProcess(PID_MASTER_SCR, -1);
			FreeMasterInterpretContext();
		}
	}

	for (int i = 0; i < NUM_INTERPRET; i++) {
		switch (g_rsd->SavedICInfo[i].GSort) {
		case GS_NONE:
			break;

		case GS_INVENTORY:
			if (g_rsd->SavedICInfo[i].event != POINTED) {
				RestoreProcess(&g_rsd->SavedICInfo[i]);
			}
			break;

		case GS_MASTER:
			// Master script only affected on restore game, not restore scene
			if (g_rsd == &g_sgData)
				RestoreMasterProcess(&g_rsd->SavedICInfo[i]);
			break;

		case GS_PROCESS:
			// Tinsel 2 process
			RestoreSceneProcess(&g_rsd->SavedICInfo[i]);
			break;

		case GS_GPROCESS:
			// Tinsel 2 Global processes only affected on restore game, not restore scene
			if (g_rsd == &g_sgData)
				RestoreGlobalProcess(&g_rsd->SavedICInfo[i]);
			break;

		case GS_ACTOR:
			if (TinselVersion >= 2)
				RestoreProcess(&g_rsd->SavedICInfo[i]);
			else
				RestoreActorProcess(g_rsd->SavedICInfo[i].idActor, &g_rsd->SavedICInfo[i], g_rsd == &g_sgData);
			break;

		case GS_POLYGON:
		case GS_SCENE:
			RestoreProcess(&g_rsd->SavedICInfo[i]);
			break;

		default:
			warning("Unhandled GSort in ResumeInterprets");
		}
	}
}

/**
 * Do restore scene
 * @param n			Id
 */
static int DoRestoreSceneFrame(SAVED_DATA *sd, int n) {
	switch (n) {
	case RS_COUNT + COUNTOUT_COUNT:
		// Trigger pre-load and fade and start countdown
		FadeOutFast();
		break;

	case RS_COUNT:
		_vm->_sound->stopAllSamples();
		ClearScreen();

		if (TinselVersion >= 2) {

			// Master script only affected on restore game, not restore scene
			if (sd == &g_sgData) {
				CoroScheduler.killMatchingProcess(PID_MASTER_SCR);
				KillGlobalProcesses();
				FreeMasterInterpretContext();
			}

			RestorePolygonStuff(sd->SavedPolygonStuff);

			// Abandon temporarily if different CD
			if (sd == &g_sgData && g_restoreCD != GetCurrentCD()) {
				g_SRstate = SR_IDLE;

				EndScene();
				SetNextCD(g_restoreCD);
				CDChangeForRestore(g_restoreCD);

				return 0;
			}
		} else {
			RestoreDeadPolys(sd->SavedDeadPolys);
		}

		// Start up the scene
		StartNewScene(sd->SavedSceneHandle, NO_ENTRY_NUM);

		_vm->_bg->SetDoFadeIn(!g_bNoFade);
		g_bNoFade = false;
		_vm->_bg->StartupBackground(Common::nullContext, sd->SavedBgroundHandle);

		if (TinselVersion >= 2) {
			Offset(EX_USEXY, sd->SavedLoffset, sd->SavedToffset);
		} else {
			_vm->_scroll->KillScroll();
			_vm->_bg->PlayfieldSetPos(FIELD_WORLD, sd->SavedLoffset, sd->SavedToffset);
			SetNoBlocking(sd->SavedNoBlocking);
		}

		_vm->_scroll->RestoreNoScrollData(&sd->SavedNoScrollData);

		if (TinselVersion >= 2) {
			// create process to sort out the moving actors
			CoroScheduler.createProcess(PID_MOVER, SortMAProcess, NULL, 0);
			g_bNotDoneYet = true;

			_vm->_actor->RestoreActorZ(sd->savedActorZ);
			_vm->_actor->RestoreZpositions(sd->zPositions);
			RestoreSysVars(sd->SavedSystemVars);
			_vm->_actor->RestoreActors(sd->NumSavedActors, sd->SavedActorInfo);
			RestoreSoundReels(sd->SavedSoundReels);
			return 1;
		}

		sortActors(sd);
		break;

	case 2:
		break;

	case 1:
		if (TinselVersion >= 2) {
			if (g_bNotDoneYet)
				return n;

			if (sd == &g_sgData)
				_vm->_dialogs->HoldItem(g_thingHeld, true);
			if (sd->bTinselDim)
				_vm->_pcmMusic->dim(true);
			_vm->_pcmMusic->restoreThatTune(sd->SavedTune);
			_vm->_scroll->ScrollFocus(sd->SavedScrollFocus);
		} else {
			_vm->_music->RestoreMidiFacts(sd->SavedMidi, sd->SavedLoop);
		}

		if (sd->SavedControl)
			ControlOn();	// Control was on
		ResumeInterprets();
		break;

	default:
		break;
	}

	return n - 1;
}

/**
 * Restore game
 * @param num			num
 */
void RestoreGame(int num) {
	_vm->_dialogs->KillInventory();

	RequestRestoreGame(num, &g_sgData, &g_savedSceneCount, g_ssData);

	// Actual restoring is performed by ProcessSRQueue
}

/**
 * Save game
 * @param name			Name of savegame
 * @param desc			Description of savegame
 */
void SaveGame(char *name, char *desc) {
	// Get current scene data
	DoSaveScene(&g_sgData);

	RequestSaveGame(name, desc, &g_sgData, &g_savedSceneCount, g_ssData);

	// Actual saving is performed by ProcessSRQueue
}


//---------------------------------------------------------------------------------

bool IsRestoringScene() {
	if (g_RestoreSceneCount) {
		g_RestoreSceneCount = DoRestoreSceneFrame(g_rsd, g_RestoreSceneCount);
	}

	return g_RestoreSceneCount ? true : false;
}

/**
 * Restores Scene
 */
void TinselRestoreScene(bool bFade) {
	// only called by restore_scene PCODE
	if (g_RestoreSceneCount == 0) {
		assert(g_savedSceneCount >= 1); // No saved scene to restore

		if (g_ASceneIsSaved)
			DoRestoreScene(&g_ssData[--g_savedSceneCount], bFade);
		if (!bFade)
			g_bNoFade = true;
	}
}

/**
 * Please Save Scene
 */
void TinselSaveScene(CORO_PARAM) {
	// only called by save_scene PCODE
	CORO_BEGIN_CONTEXT;
	CORO_END_CONTEXT(_ctx);

	CORO_BEGIN_CODE(_ctx);

	assert(g_savedSceneCount < MAX_NEST); // nesting limit reached

	// Don't save the same thing multiple times!
	// FIXME/TODO: Maybe this can be changed to an assert?
	if (g_savedSceneCount && g_ssData[g_savedSceneCount-1].SavedSceneHandle == GetSceneHandle())
		CORO_KILL_SELF();

	DoSaveScene(&g_ssData[g_savedSceneCount++]);

	CORO_END_CODE;
}

} // End of namespace Tinsel
