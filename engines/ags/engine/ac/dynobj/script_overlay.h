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
 */

#ifndef AGS_ENGINE_AC_DYNOBJ_SCRIPTOVERLAY_H
#define AGS_ENGINE_AC_DYNOBJ_SCRIPTOVERLAY_H

#include "ags/engine/ac/dynobj/cc_ags_dynamic_object.h"

namespace AGS3 {

struct ScriptOverlay final : AGSCCDynamicObject {
	int overlayId;
	int borderWidth;
	int borderHeight;
	int isBackgroundSpeech;

	int Dispose(const char *address, bool force) override;
	const char *GetType() override;
	void Unserialize(int index, AGS::Shared::Stream *in, size_t data_sz) override;
	void Remove();
	ScriptOverlay();

protected:
	// Calculate and return required space for serialization, in bytes
	size_t CalcSerializeSize() override;
	// Write object data into the provided stream
	void Serialize(const char *address, AGS::Shared::Stream *out) override;
};

} // namespace AGS3

#endif
