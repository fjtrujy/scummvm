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

#include "ags/shared/ac/sprite_cache.h"
#include "ags/shared/gui/gui_main.h"
#include "ags/shared/gui/gui_slider.h"
#include "ags/shared/util/stream.h"

namespace AGS3 {

namespace AGS {
namespace Shared {

GUISlider::GUISlider() {
	MinValue = 0;
	MaxValue = 10;
	Value = 0;
	BgImage = 0;
	HandleImage = 0;
	HandleOffset = 0;
	IsMousePressed = false;

	_scEventCount = 1;
	_scEventNames[0] = "Change";
	_scEventArgs[0] = "GUIControl *control";
	_handleRange = 0;
}

bool GUISlider::IsHorizontal() const {
	return Width > Height;
}

bool GUISlider::IsOverControl(int x, int y, int leeway) const {
	// check the overall boundary
	if (GUIObject::IsOverControl(x, y, leeway))
		return true;
	// now check the handle too
	return _cachedHandle.IsInside(Point(x, y));
}

void GUISlider::Draw(Shared::Bitmap *ds) {
	// Clamp Value
	// TODO: this is necessary here because some Slider fields are still public
	if (MinValue >= MaxValue)
		MaxValue = MinValue + 1;
	Value = Math::Clamp(Value, MinValue, MaxValue);
	// Test if sprite is available
	// TODO: react to sprites initialization/deletion instead!
	if (_GP(spriteset)[HandleImage] == nullptr)
		HandleImage = 0;

	// Depending on slider's orientation, thickness is either Height or Width
	const int thickness = IsHorizontal() ? Height : Width;
	// "thick_f" is the factor for calculating relative element positions
	const int thick_f = thickness / 3; // one third of the control's thickness
	// Bar thickness
	const int bar_thick = thick_f * 2 + 2;

	// Calculate handle size
	Size handle_sz;
	if (HandleImage > 0) // handle is a sprite
	{
		handle_sz = Size(get_adjusted_spritewidth(HandleImage),
			get_adjusted_spriteheight(HandleImage));
	} else // handle is a drawn rectangle
	{
		if (IsHorizontal())
			handle_sz = Size(get_fixed_pixel_size(4) + 1, bar_thick + (thick_f - 1) * 2);
		else
			handle_sz = Size(bar_thick + (thick_f - 1) * 2, get_fixed_pixel_size(4) + 1);
	}

	// Calculate bar and handle positions
	Rect bar;
	Rect handle;
	int handle_range;
	if (IsHorizontal()) // horizontal slider
	{
		// Value pos is a coordinate corresponding to current slider's value
		bar = RectWH(X + 1, Y + Height / 2 - thick_f, Width - 1, bar_thick);
		handle_range = Width - 4;
		int value_pos = (int)(((float)(Value - MinValue) * (float)handle_range) / (float)(MaxValue - MinValue));
		handle = RectWH((bar.Left + get_fixed_pixel_size(2)) - (handle_sz.Width / 2) + 1 + value_pos - 2,
			bar.Top + (bar.GetHeight() - handle_sz.Height) / 2,
			handle_sz.Width, handle_sz.Height);
		handle.MoveToY(handle.Top + data_to_game_coord(HandleOffset));
	}
	// vertical slider
	else {
		bar = RectWH(X + Width / 2 - thick_f, Y + 1, bar_thick, Height - 1);
		handle_range = Height - 4;
		int value_pos = (int)(((float)(MaxValue - Value) * (float)handle_range) / (float)(MaxValue - MinValue));
		handle = RectWH(bar.Left + (bar.GetWidth() - handle_sz.Width) / 2,
			(bar.Top + get_fixed_pixel_size(2)) - (handle_sz.Height / 2) + 1 + value_pos - 2,
			handle_sz.Width, handle_sz.Height);
		handle.MoveToX(handle.Left + data_to_game_coord(HandleOffset));
	}

	color_t draw_color;
	if (BgImage > 0) {
		// tiled image as slider background
		int x_inc = 0;
		int y_inc = 0;
		if (IsHorizontal()) {
			x_inc = get_adjusted_spritewidth(BgImage);
			// centre the image vertically
			bar.Top = Y + (Height / 2) - get_adjusted_spriteheight(BgImage) / 2;
		} else {
			y_inc = get_adjusted_spriteheight(BgImage);
			// centre the image horizontally
			bar.Left = X + (Width / 2) - get_adjusted_spritewidth(BgImage) / 2;
		}
		int cx = bar.Left;
		int cy = bar.Top;
		// draw the tiled background image
		do {
			draw_gui_sprite(ds, BgImage, cx, cy, true);
			cx += x_inc;
			cy += y_inc;
			// done as a do..while so that at least one of the image is drawn
		} while ((cx + x_inc <= bar.Right) && (cy + y_inc <= bar.Bottom));
	} else {
		// normal grey background
		draw_color = ds->GetCompatibleColor(16);
		ds->FillRect(bar, draw_color);
		draw_color = ds->GetCompatibleColor(8);
		ds->DrawLine(Line(bar.Left, bar.Top, bar.Left, bar.Bottom), draw_color);
		ds->DrawLine(Line(bar.Left, bar.Top, bar.Right, bar.Top), draw_color);
		draw_color = ds->GetCompatibleColor(15);
		ds->DrawLine(Line(bar.Right, bar.Top + 1, bar.Right, bar.Bottom), draw_color);
		ds->DrawLine(Line(bar.Left, bar.Bottom, bar.Right, bar.Bottom), draw_color);
	}

	if (HandleImage > 0) // handle is a sprite
	{
		draw_gui_sprite(ds, HandleImage, handle.Left, handle.Top, true);
	} else // handle is a drawn rectangle
	{
		// normal grey tracker handle
		draw_color = ds->GetCompatibleColor(7);
		ds->FillRect(handle, draw_color);
		draw_color = ds->GetCompatibleColor(15);
		ds->DrawLine(Line(handle.Left, handle.Top, handle.Right, handle.Top), draw_color);
		ds->DrawLine(Line(handle.Left, handle.Top, handle.Left, handle.Bottom), draw_color);
		draw_color = ds->GetCompatibleColor(16);
		ds->DrawLine(Line(handle.Right, handle.Top + 1, handle.Right, handle.Bottom), draw_color);
		ds->DrawLine(Line(handle.Left + 1, handle.Bottom, handle.Right, handle.Bottom), draw_color);
	}

	_cachedHandle = handle;
	_handleRange = handle_range;
}

bool GUISlider::OnMouseDown() {
	IsMousePressed = true;
	// lock focus to ourselves
	return true;
}

void GUISlider::OnMouseMove(int x, int y) {
	if (!IsMousePressed)
		return;

	if (IsHorizontal())
		Value = (int)(((float)((x - X) - 2) * (float)(MaxValue - MinValue)) / (float)_handleRange) + MinValue;
	else
		Value = (int)(((float)(((Y + Height) - y) - 2) * (float)(MaxValue - MinValue)) / (float)_handleRange) + MinValue;

	Value = Math::Clamp(Value, MinValue, MaxValue);
	NotifyParentChanged();
	IsActivated = true;
}

void GUISlider::OnMouseUp() {
	IsMousePressed = false;
}

void GUISlider::ReadFromFile(Stream *in, GuiVersion gui_version) {
	GUIObject::ReadFromFile(in, gui_version);
	MinValue = in->ReadInt32();
	MaxValue = in->ReadInt32();
	Value = in->ReadInt32();
	if (gui_version < kGuiVersion_350) { // NOTE: reading into actual variables only for old savegame support
		IsMousePressed = in->ReadInt32() != 0;
	}
	if (gui_version >= kGuiVersion_unkn_104) {
		HandleImage = in->ReadInt32();
		HandleOffset = in->ReadInt32();
		BgImage = in->ReadInt32();
	} else {
		HandleImage = -1;
		HandleOffset = 0;
		BgImage = 0;
	}
}

void GUISlider::WriteToFile(Stream *out) const {
	GUIObject::WriteToFile(out);
	out->WriteInt32(MinValue);
	out->WriteInt32(MaxValue);
	out->WriteInt32(Value);
	out->WriteInt32(HandleImage);
	out->WriteInt32(HandleOffset);
	out->WriteInt32(BgImage);
}

void GUISlider::ReadFromSavegame(Stream *in, GuiSvgVersion svg_ver) {
	GUIObject::ReadFromSavegame(in, svg_ver);
	BgImage = in->ReadInt32();
	HandleImage = in->ReadInt32();
	HandleOffset = in->ReadInt32();
	MinValue = in->ReadInt32();
	MaxValue = in->ReadInt32();
	Value = in->ReadInt32();
}

void GUISlider::WriteToSavegame(Stream *out) const {
	GUIObject::WriteToSavegame(out);
	out->WriteInt32(BgImage);
	out->WriteInt32(HandleImage);
	out->WriteInt32(HandleOffset);
	out->WriteInt32(MinValue);
	out->WriteInt32(MaxValue);
	out->WriteInt32(Value);
}

} // namespace Shared
} // namespace AGS
} // namespace AGS3
