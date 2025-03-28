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

#include "engines/metaengine.h"
#include "base/plugins.h"
#include "base/version.h"
#include "common/events.h"
#include "common/system.h"
#include "common/translation.h"
#include "common/util.h"
#include "graphics/conversion.h"
#include "graphics/surface.h"
#include "graphics/fonts/amigafont.h"
#include "gui/about.h"
#include "gui/gui-manager.h"
#include "gui/ThemeEval.h"

namespace GUI {

class EE;

class EEHandler {
public:
	bool handleKeyDown(Common::KeyState &state);
};

enum {
	kScrollStartDelay = 1500,
	kScrollMillisPerPixel = 60
};

// Every Line should start with a letter followed by a digit. Currently those can be
// (all subject to change)
// Letter:
//   C, L, R     -- set center/left/right alignment
//   A           -- ASCII text to replace the next (latin1) line
// Digit:
//   0 - 2       -- set a custom color:
//                  0 normal text
//                  1 highlighted text
//                  2 disabled text
// TODO: Maybe add a tab/indent feature; that is, make it possible to specify
// an amount by which that line shall be indented (the indent of course would have
// to be considered while performing any word wrapping, too).
//
// TODO: Add different font sizes (for bigger headlines)
// TODO: Allow color change in the middle of a line...

static const char *copyright_text[] = {
"",
"C0""Copyright (C) 2001-2022 The ScummVM Team",
"C0""https://www.scummvm.org",
"",
"C0""ScummVM is the legal property of its developers, whose names are too numerous to list here. Please refer to the COPYRIGHT file distributed with this binary.",
"",
};

static const char *gpl_text[] = {
"",
"C0""This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.",
"C0""",
"C0""This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.",
"",
"C0""You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.",
"",
};

#include "gui/credits.h"

AboutDialog::AboutDialog()
	: Dialog(10, 20, 300, 174),
	_scrollPos(0), _scrollTime(0), _willClose(false) {

	reflowLayout();

	int i;

	for (i = 0; i < 1; i++)
		_lines.push_back(Common::U32String());

	Common::String version("C0""ScummVM ");
	version += gScummVMVersion;
	_lines.push_back(version);

	// I18N: built on <build date> with <compiler>
	Common::U32String date = Common::U32String::format(_("(built on %s with %s)"), gScummVMBuildDate, gScummVMCompiler);
	_lines.push_back(Common::U32String("C2") + date);

	for (i = 0; i < ARRAYSIZE(copyright_text); i++)
		addLine(Common::U32String(copyright_text[i]));

	Common::U32String features("C1");
	features += _("Features compiled in:");
	addLine(features);
	Common::String featureList("C0");
	featureList += gScummVMFeatures;
	addLine(featureList);

	_lines.push_back(Common::U32String());

	Common::U32String engines("C1");
	engines += _("Available engines:");
	addLine(engines);

	const PluginList &plugins = EngineMan.getPlugins(PLUGIN_TYPE_ENGINE);
	PluginList::const_iterator iter = plugins.begin();
	for (; iter != plugins.end(); ++iter) {
		Common::String str;
		str = "C0";
		str += (*iter)->getName();
		addLine(str);

		const Plugin *p = EngineMan.findPlugin((*iter)->getName());

		if (!p) {
			warning("Cannot find plugin for %s", (*iter)->getName());
			continue;
		}

		str = "C2";
		str += p->get<MetaEngineDetection>().getOriginalCopyright();
		addLine(str);

		//addLine("");
	}

	for (i = 0; i < ARRAYSIZE(gpl_text); i++)
		addLine(Common::U32String(gpl_text[i]));

	_lines.push_back(Common::U32String());

	for (i = 0; i < ARRAYSIZE(credits); i++)
		addLine(Common::U32String(credits[i], Common::kUtf8));
}

void AboutDialog::addLine(const Common::U32String &str) {
	Common::U32String::const_iterator strBeginItr = str.begin();
	if (*strBeginItr == 0) {
		_lines.push_back(Common::U32String());
	} else {
		Common::U32String format(str.begin(), str.begin() + 2);
		strBeginItr += 2;
		Common::U32String renderStr(strBeginItr, str.end());

		Common::U32StringArray wrappedLines;
		g_gui.getFont().wordWrapText(renderStr, _w - 2 * _xOff, wrappedLines);

		for (Common::U32StringArray::const_iterator i = wrappedLines.begin(); i != wrappedLines.end(); ++i) {
			_lines.push_back(format + *i);
		}
	}
}


void AboutDialog::open() {
	_scrollTime = g_system->getMillis() + kScrollStartDelay;
	_scrollPos = 0;
	_willClose = false;

	Dialog::open();
}

void AboutDialog::close() {
	Dialog::close();
}

void AboutDialog::drawDialog(DrawLayer layerToDraw) {
	Dialog::drawDialog(layerToDraw);

	setTextDrawableArea(Common::Rect(_x, _y, _x + _w, _y + _h));

	// Draw text
	// TODO: Add a "fade" effect for the top/bottom text lines
	// TODO: Maybe prerender all of the text into another surface,
	//       and then simply compose that over the screen surface
	//       in the right way. Should be even faster...
	const int firstLine = _scrollPos / _lineHeight;
	const int lastLine = MIN((_scrollPos + _h) / _lineHeight + 1, (uint32)_lines.size());
	int y = _y + _yOff - (_scrollPos % _lineHeight);

	for (int line = firstLine; line < lastLine; line++) {
		Common::U32String str = _lines[line];
		Common::U32String::const_iterator strLineItrBegin = _lines[line].begin();
		Common::U32String::const_iterator strLineItrEnd = _lines[line].end();

		Graphics::TextAlign align = Graphics::kTextAlignCenter;
		ThemeEngine::WidgetStateInfo state = ThemeEngine::kStateEnabled;
		if (strLineItrBegin != strLineItrEnd) {
			switch (*strLineItrBegin) {
			case 'C':
				align = Graphics::kTextAlignCenter;
				break;
			case 'L':
				align = Graphics::kTextAlignLeft;
				break;
			case 'R':
				align = Graphics::kTextAlignRight;
				break;
			default:
				error("Unknown scroller opcode '%c'", str[0]);
				break;
			}
			switch (*(strLineItrBegin + 1)) {
			case '0':
				state = ThemeEngine::kStateEnabled;
				break;
			case '1':
				state = ThemeEngine::kStateHighlight;
				break;
			case '2':
				state = ThemeEngine::kStateDisabled;
				break;
			case '3':
				warning("Need state for color 3");
				// color = g_gui._shadowcolor;
				break;
			case '4':
				warning("Need state for color 4");
				// color = g_gui._bgcolor;
				break;
			default:
				error("Unknown color type '%c'", str[1]);
			}
			strLineItrBegin += 2;
		}
		// Trim leading whitespaces if center mode is on
		if (align == Graphics::kTextAlignCenter)
			while (strLineItrBegin != strLineItrEnd && *strLineItrBegin == ' ')
				strLineItrBegin++;

		Common::U32String renderStr(strLineItrBegin, strLineItrEnd);
		if (!renderStr.empty())
			g_gui.theme()->drawText(Common::Rect(_x + _xOff, y, _x + _w - _xOff, y + g_gui.theme()->getFontHeight()),
			                        renderStr, state, align, ThemeEngine::kTextInversionNone, 0, false,
			                        ThemeEngine::kFontStyleBold, ThemeEngine::kFontColorNormal, true, _textDrawableArea);
		y += _lineHeight;
	}
}

void AboutDialog::handleTickle() {
	const uint32 t = g_system->getMillis();
	int scrollOffset = ((int)t - (int)_scrollTime) / kScrollMillisPerPixel;
	if (scrollOffset > 0) {
		int modifiers = g_system->getEventManager()->getModifierState();

		// Scroll faster when shift is pressed
		if (modifiers & Common::KBD_SHIFT)
			scrollOffset *= 4;
		// Reverse scrolling when alt is pressed
		if (modifiers & Common::KBD_ALT)
			scrollOffset *= -1;
		_scrollPos += scrollOffset;
		_scrollTime = t;

		if (_scrollPos < 0) {
			_scrollPos = 0;
		} else if ((uint32)_scrollPos > _lines.size() * _lineHeight) {
			_scrollPos = 0;
			_scrollTime += kScrollStartDelay;
		}
		drawDialog(kDrawLayerForeground);
	}
}

void AboutDialog::handleMouseUp(int x, int y, int button, int clickCount) {
	// Close upon any mouse click
	close();
}

void AboutDialog::handleKeyDown(Common::KeyState state) {
	EEHandler eeHandler;

	if (eeHandler.handleKeyDown(state)) {
		reflowLayout();
		return;
	}

	if (state.ascii)
		_willClose = true;
}

void AboutDialog::handleKeyUp(Common::KeyState state) {
	if (state.ascii && _willClose)
		close();
}

void AboutDialog::reflowLayout() {
	Dialog::reflowLayout();
	int i;
	const int screenW = g_system->getOverlayWidth();
	const int screenH = g_system->getOverlayHeight();

	_xOff = g_gui.xmlEval()->getVar("Globals.About.XOffset", 5);
	_yOff = g_gui.xmlEval()->getVar("Globals.About.YOffset", 5);
	int outerBorder = g_gui.xmlEval()->getVar("Globals.About.OuterBorder");

	_w = screenW - 2 * outerBorder;
	_h = screenH - 2 * outerBorder;

	_lineHeight = g_gui.getFontHeight() + 3;

	// Heuristic to compute 'optimal' dialog width
	int maxW = _w - 2*_xOff;
	_w = 0;
	for (i = 0; i < ARRAYSIZE(credits); i++) {
		int tmp = g_gui.getStringWidth(credits[i]) + 5;
		if (_w < tmp && tmp <= maxW) {
			_w = tmp;
		}
	}
	_w += 2*_xOff;

	// Center the dialog
	_x = (screenW - _w) / 2;
	_y = (screenH - _h) / 2;
}


////////////////////////////////
//// Here we go
////////////////////////////////

enum {
	kDirUp, kDirLeft, kDirRight
};

enum {
	kModeMenu, kModePlay, kModePause
};

enum { kPlComp, kPlHuman };

enum { kSndPoint, kSndHit, kSndLoss };


enum {
	kSpL1, kSpL2, kSpL3,
	kSpR1, kSpR2, kSpR3,
	kSpB1, kSpB2, kSpB3, kSpB4,
	kSp0, kSp1, kSp2, kSp3, kSp4,
	kSp5, kSp6, kSp7, kSp8, kSp9,
	kSpCode1,
	kSpCode1h = kSpCode1 + 6,
	kSpNum0 = kSpCode1h + 6,
	kSpStar = kSpNum0 + 10,
	kSpSpace,
	kSpLast
};
class EE  {
public:
	EE();
	~EE();

	void run();

private:
	Graphics::Surface _back;
	int _hits;
	bool _rmode; // animation after loosing
	int _score[2];

	bool _soundEnabled;
	bool _shouldQuit;

	int _rcnt; //last 20 cycles of round
	int _winner; //who wins the point or serving
	int _tvelx, _tvely;

	int _x[2], _y[2], _px[2];
	int _frame[2], _frameindex[2];
	int _rebound[2];
	int _bx, _by, _pbx, _pby, _tbx, _tby, _bvelx, _bvely;
	int _serve, _server, _servevel, _compserve;
	int _hitter, _starter, _sstage;
	int _bytop;
	int _rnd;

	int _olx, _oly, _orx, _ory, _obx, _oby;

	bool _air;

	bool _oCoords;

	int _mode;

	int _keymove[2][3];
	int _opts;
	int _opt;

	Graphics::AmigaFont _font;

	Graphics::Surface _sp[kSpLast];

	Graphics::PixelFormat _format;
	int _windowX, _windowY, _windowW, _windowH;
	float _scale;
	bool _inited;

	uint32 _colorBlack, _colorBlue, _colorOrange, _colorKey;

	void cls(bool update = true);
	void loadSounds();
	void processKeyUp(Common::Event &e);
	void processKeyDown(Common::Event &e);
	void getPos();
	void setwinner();
	void resetpt();
	void calcscore();
	int destination(int pl, int destx, int tol);
	bool moveball();
	void docollisions();
	void computer0();
	void computer1();
	void init();
	void draw(int sn, int x, int y);
	void doMenu(Common::Event &e);
	void putshapes();
	void game();
	void playSound(int d);

	void setupGraphics();
	void genField();
};

bool EEHandler::handleKeyDown(Common::KeyState &state) {
	if (state.ascii == 'v') {
		EE *ee = new EE();
		ee->run();
		delete ee;

		g_gui.redrawFull();

		return true;
	}

	return false;
}

EE::EE() {
	init();

	_scale = 1.0;
	_windowW = 320;
	_windowH = 200;
	_windowX = _windowY = 0;

	_format = g_system->getOverlayFormat();

	_colorBlack = _colorBlue = _colorOrange = _colorKey = 0;
}

EE::~EE() {
	for (int i = 0; i < kSpLast; i++)
		_sp[i].free();
}

void EE::cls(bool update) {
	_back.fillRect(Common::Rect(0, 0, _windowW, _windowH), _colorBlack);

	if (update)
		g_system->copyRectToOverlay(_back.getPixels(), _back.pitch, _windowX, _windowY, _windowW, _windowH);
}

void EE::run() {
	if (!_inited)
		return;

	_shouldQuit = false;

	setupGraphics();

	init();

	while (!_shouldQuit) {
		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			switch (event.type) {
			case Common::EVENT_QUIT:
			case Common::EVENT_RETURN_TO_LAUNCHER:
				_shouldQuit = true;
				break;
			case Common::EVENT_SCREEN_CHANGED:
				if (g_gui.checkScreenChange())
					setupGraphics();
				break;
			case Common::EVENT_LBUTTONDOWN:
				break;
			case Common::EVENT_KEYDOWN:
				processKeyDown(event);
				break;
			case Common::EVENT_KEYUP:
				processKeyUp(event);
				break;
			default:
				break;
			}
		}

		game();
		putshapes();

		g_system->updateScreen();
		g_system->delayMillis(10);
	}
}

const int polecol[] = {0, 1, 2, 3, 3, 4, 6, 7, 9, 14};
const int jump[] = {-4, -4, -3, -3, -3, -3, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1, -1, 0, 0,
					0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4};

void EE::loadSounds() {
#if 0
	if ($.isEmptyObject(sounds)) {
		for (i = 0; i < snNames.length; i++) {
			s = snNames[i];
			sounds[s] = new Audio();
			sounds[s].src = 'sounds/' + s + '.wav';
		}
	}
#endif
}

void EE::processKeyUp(Common::Event &e) {
	switch (e.kbd.keycode) {
	case Common::KEYCODE_LEFT: // left
		_keymove[1][kDirLeft] = 0;
		break;
	case Common::KEYCODE_RIGHT: // right
		_keymove[1][kDirRight] = 0;
		break;
	case Common::KEYCODE_UP: // top
		_keymove[1][kDirUp] = 0;
		break;
	case Common::KEYCODE_a: //a
		_keymove[0][kDirLeft] = 0;
		break;
	case Common::KEYCODE_d: // d
		_keymove[0][kDirRight] = 0;
		break;
	case Common::KEYCODE_w: // w
		_keymove[0][kDirUp] = 0;
		break;
	default:
		break;
	}
}

void EE::processKeyDown(Common::Event &e) {
	if (_mode == kModeMenu) {
		doMenu(e);
		return;
	}

	switch (e.kbd.keycode) {
	case Common::KEYCODE_LEFT: // left
		_keymove[1][kDirLeft] = -2;
		break;
	case Common::KEYCODE_RIGHT: // right
		_keymove[1][kDirRight] = 2;
		break;
	case Common::KEYCODE_UP: // top
		_keymove[1][kDirUp]= 1;
		break;
	case Common::KEYCODE_a: //a
		_keymove[0][kDirLeft] = -2;
		break;
	case Common::KEYCODE_d: // d
		_keymove[0][kDirRight] = 2;
		break;
	case Common::KEYCODE_w: // w
		_keymove[0][kDirUp] = 1;
		break;
	case Common::KEYCODE_ESCAPE:
		_mode = kModeMenu;
		break;
	case Common::KEYCODE_p: // p
		_keymove[0][kDirRight] = _keymove[0][kDirLeft] = _keymove[0][kDirUp] = 0;
		_mode = (_mode == kModePlay) ? kModePause : kModePlay;
		break;
	case Common::KEYCODE_r: // r
		if (_mode == kModePlay)
			init();
		break;
	case Common::KEYCODE_s: // s
		if (!_soundEnabled)
			loadSounds();
		_soundEnabled = !_soundEnabled;
		break;
	default:
		break;
	}
}

void EE::getPos() {
	for (int i = 0; i < 2; i++) {
		if (_keymove[i][kDirUp] && _frameindex[i] == -1)
			_frameindex[i] = 0;
		int velx = _keymove[i][kDirLeft] + _keymove[i][kDirRight];
		int tx = _x[i] + velx;
		int bnd = 3 + i * 155;
		if (velx > 0) {
			if (tx < bnd + 119)
				_x[i] = tx;
			else
				_x[i] = bnd + 119;
		} else if (tx > bnd) {
			_x[i] = tx;
		} else {
			_x[i] = bnd;
		}

		if (_frameindex[i] == -2) {
			_y[i] = 173;
			_frame[i] = 0;
			_frameindex[i] = -1;
		}
		if (_frameindex[i] == -1) {
			if (velx != 0) {
				if (abs(_px[i] - _x[i]) > 4) {
					_frame[i] ^= 1;
					_px[i] = _x[i];
				}
			} else {
				_frame[i] = 0;
			}
		} else {
			_frame[i] = 2 + (_frameindex[i] > 18);

			if (_frameindex[i] == 19)
				_y[i] -= 4;

			_y[i] += jump[_frameindex[i]++];

			if (_frameindex[i] > 37)
				_frameindex[i] = -2;
		}
	}
}

void EE::setwinner() {
	if (_hits > 2) {
		_winner = 1 - _hitter;
	} else {
		_winner = _tbx < 150 ? 1 : 0;
	}

	_tvely = abs(_bvely) >> 3;
	_tvelx = abs(_bvelx) >> 3;
}

void EE::resetpt() {
	_keymove[0][kDirUp] = 0;
	_keymove[1][kDirUp] = 0;
	getPos();

	if (abs(_bvelx) > _tvelx) {
		if (_bvelx < 0)
			_bvelx = -_tvelx;
		else
			_bvelx = _tvelx;
	}

	if (abs(_bvely) > _tvely) {
		if (_bvely < 0)
			_bvely = -_tvely;
		else
			_bvely = _tvely;
	}

	docollisions();
	moveball();
	putshapes();
}

void EE::calcscore() {
	if (_winner == _server) {
		if (_soundEnabled && _mode != kModeMenu) {
			playSound(kSndPoint);
		}
		_score[_winner]++;
		if (_score[_winner] > 14 && _score[_winner] - _score[1 - _winner] > 1) {
			init();
			return;
		}
	} else {
		if (_soundEnabled && _mode != kModeMenu) {
			playSound(kSndLoss);
		}
		_server = _winner;
	}

	_bx = (_tbx = _pbx = (64 + _winner * 165)) << 6;
	_by = (_tby = _pby = 135) << 6;
	_bvelx = _bvely = _hits = _rebound[0] = _rebound[1] = 0;
	_serve = _servevel = 1;
	_hitter = 2;
	_compserve = abs(_rnd) % 5;
	if (_score[_server] == 14)
		_compserve = 5;
	_sstage = 0;
}

int EE::destination(int pl, int destx, int tol) {
	int xp = _x[pl];
	if (abs(xp - destx) < tol) {
		_keymove[pl][kDirLeft] = 0;
		_keymove[pl][kDirRight] = 0;
		return 1;
	}
	if (xp < destx) {
		_keymove[pl][kDirLeft] = 0;
		_keymove[pl][kDirRight] = 2;
	} else {
		_keymove[pl][kDirLeft] = -2;
		_keymove[pl][kDirRight] = 0;
	}
	return 0;
}

int reset = false;

bool EE::moveball() {
	if (!reset) {
		_bx = 4096; _by = 8640; _bvelx = 125; _bvely = -259;
		reset = true;
	}

	int rbvelx = _bvelx;
	int rbvely = _bvely;
	if (rbvelx > 319) rbvelx = 319;
	if (rbvelx < -319) rbvelx = -319;
	if (rbvely > 319) rbvely = 319;
	if (rbvely < -319) rbvely = -319;
	_pbx = _tbx;
	_pby = _tby;
	_bx += rbvelx;
	_by += rbvely;

	if (_bx < 320) {
		_bx = 320;
		rbvelx = -rbvelx;
		rbvelx -= rbvelx >> 4;
		rbvely -= rbvely >> 4;
		if (_hitter == 1) {
			_hitter = 2;
			_hits = 0;
		}
	}
	if (_bx > 18112) {
		_bx = 18112;
		rbvelx = -rbvelx;
		rbvelx -= rbvelx >> 4;
		rbvely -= rbvely >> 4;
		if (_hitter == 0) {
			_hitter = 2;
			_hits = 0;
		}
	}
	if (_by < 832) {
		_by = 832;
		rbvely = -rbvely;
		rbvelx -= rbvelx >> 4;
		rbvely -= rbvely >> 4;
	}
	bool hitfloor;
	if (_by > 11392) {
		_by = 11392;
		rbvely = -rbvely;
		hitfloor = false;
	} else {
		hitfloor = true;
	}

	//if (rbvely > 0) // Checked with original, this is how it is
	//	rbvely += 1;
	//else
		rbvely += 1;

	_tbx = _bx >> 6;
	_tby = _by >> 6;
	_bvelx = rbvelx;
	_bvely = rbvely;

	return hitfloor;
}

void EE::docollisions() {
	for (int i = 0; i < 2; i++) {
		int dx = _tbx - _x[i] - i * 7;
		int dy = (_tby - _y[i]) >> 1;
		int dist = (dx >> 2) * dx + dy * dy;
		if (dist < 110) {
			int rndoff = 8 - (_rnd & 15);
			if (_frameindex[i] > -1)
				_bvely = -abs(_bvely) + (jump[_frameindex[i]] << (3 << _servevel));
			else
				_bvely = -abs(_bvely);
			_bvely += rndoff;
			_bvelx += dx * abs(dx) + ((_keymove[i][kDirRight] + _keymove[i][kDirLeft]) << (4 + _servevel)) + rndoff;
			if (!_rebound[i]) {
				if (_mode != kModeMenu) {
					if (_soundEnabled && _mode != kModeMenu) {
						playSound(kSndHit);
					}
				}
				_bytop = 200;
				_serve = 0;
				_rebound[i] = 1;
				if (_hitter != i) {
					_hitter = i;
					_hits = 0;
				} else {
					_hits++;
				}
			}
		} else if (_rebound[i]) {
			_rebound[i] = _servevel = 0;
		}
	}

	int i = 1;
	if (_tby > 91) {
		if (_pbx < 128 && _tbx > 127) {
			_bvelx = -abs(_bvelx) >> 1;
			_bx = 127 * 64;
			i = 0;
		} else if (_pbx > 159 && _tbx < 160) {
			_bvelx = abs(_bvelx) >> 1;
			_bx = 160 * 64;
			i = 0;
		}
	}
	if (i && _tby > 81 && _tbx > 127 && _tbx < 160) {
		if (_tby > 91) {
			if (_tbx < 148)
				_bvelx = -abs(_bvelx);
			else
				_bvelx = abs(_bvelx);
		} else if ((_tbx > 147 && 161 - _tbx >= polecol[91 - _tby]) ||
				   (_tbx < 148 && _tbx - 133 >= polecol[91 - _tby])) {
			if (_bvely > 0) {
				int dx = _tbx - 145;
				if (dx < -5) _bvelx = -abs(_bvelx);
				if (dx > 5) _bvelx = abs(_bvelx);
				_bvely = -abs(_bvely);
			}
			if (abs(_bvelx) > 32) _bvelx = _bvelx >> 1;
			if (abs(_bvely) > 32) _bvely = _bvely >> 1;
		}
	}
}


void EE::computer0() {
	_keymove[0][kDirUp] = 0;
	if (_tby < _bytop) _bytop = _tby;
	int rndoff = 5 - _rnd % 10;
	int dest = 0;
	if (_serve && ((_server & 1) == 0)) {
		switch (_compserve) {
		case 0:
			dest = destination(0, 55, 2);
			break;
		case 1:
			dest = destination(0, 84, 2);
			break;
		case 2:
			dest = destination(0, 80, 2);
			break;
		case 3:
			if (_sstage == 0) {
				if ((dest = destination(0, 44, 2)))
					_sstage = 1;
			} else {
				destination(0, 58, 2);
				dest = 1;
			}
			break;
		case 4:
			if (_sstage == 0) {
				if ((dest = destination(0, 90, 2)))
					_sstage = 1;
			} else {
				destination(0, 58, 2);
				dest = 1;
			}
			break;
		case 5:
			if (_sstage == 0) {
				if (destination(0, 3, 2))
					_sstage = 1;
				dest = 0;
			} else {
				destination(0, 8 + _sstage++, 1);
				dest = 1;
			}
			break;
		default:
			break;
		}
		_keymove[0][kDirUp] = dest;
	} else if (_bvely > 0 && _tbx < 140) {
		int ystep, destx;
		if (_bvely >> 6 == 0)
			ystep = 0;
		else
			ystep = (140 - _tby) / (_bvely >> 6);

		if (ystep < 1 || (_bvelx >> 6) == 0)
			destx = _tbx;
		else
			destx = _tbx + (_bvelx >> 6) * ystep - 4;

		int dx = _x[0] - _tbx;

		if (abs(_bvelx) < 128 && _bytop < 75) {
			if ((_tby < 158) ^ (_bvelx < 0))
				destination(0, _tbx - 15, 3);
			else
				destination(0, _tbx + 15, 3);
		} else {
			if (_tby > 130) {
				if (abs(dx) > 6 && abs(_bvelx) < 1024) {
					destination(0, _tbx - ((_x[0] - 60) >> 3), 3);
				} else {
					destination(0, _tbx + rndoff + ((_x[0] - 60) >> 3), 10);
					_keymove[0][kDirUp] = _x[0] < 105 && (_hitter != 0 || _hits < 2);
				}
			} else {
				if (destx < 3)
					destx = 6 - destx;
				if (destx > 123)
					destx = 246 - destx;
				destination(0, destx + rndoff, 3);
			}
		}
	} else
		destination(0, 56, 8);
}

void EE::computer1() {
	_keymove[1][kDirUp] = 0;
	if (_tby < _bytop) _bytop = _tby;
	int rndoff = 5 - _rnd % 10;
	if (_serve && ((_server & 1) == 1)) {
		int dest = 0;
		switch (_compserve) {
		case 0:
			dest = destination(1, 232, 2);
			break;
		case 1:
			dest = destination(1, 202, 2);
			break;
		case 2:
			dest = destination(1, 208, 2);
			break;
		case 3:
			if (_sstage == 0) {
				if ((dest = destination(1, 250, 2)))
					_sstage = 1;
			} else {
				destination(1, 220, 2);
				dest = 1;
			}
			break;
		case 4:
			if (_sstage == 0) {
				if ((dest = destination(1, 190, 2)))
					_sstage = 1;
			} else {
				destination(1, 230, 2);
				dest = 1;
			}
			break;
		case 5:
			if (_sstage == 0) {
				if (destination(1, 277, 2))
					_sstage = 1;
				dest = 0;
			} else {
				destination(1, 272 - _sstage++, 1);
				dest = 1;
			}
			break;
		default:
			break;
		}
		_keymove[1][kDirUp] = dest;
	} else if (_bvely > 0 && _tbx > 125) {
		int ystep, destx;
		if (_bvely >> 6 == 0)
			ystep = 0;
		else
			ystep = (140 - _tby) / (_bvely >> 6);

		if (ystep < 1 || (_bvelx >> 6) == 0)
			destx = _tbx;
		else
			destx = _tbx + (_bvelx >> 6) * ystep - 4;

		int dx = _x[1] - _tbx;

		if (abs(_bvelx) < 128 && _bytop < 75) {
			if ((_tby < 158) ^ (_bvelx < 0))
				destination(1, _tbx + 15, 3);
			else
				destination(1, _tbx - 15, 3);
		} else {
			if (_tby > 130) {
				if (abs(dx) > 6 && abs(_bvelx) < 1024)
					destination(1, _tbx + ((_x[1] - 218) >> 3), 3);
				else {
					destination(1, _tbx - rndoff - ((_x[1] - 218) >> 3), 10);
					_keymove[1][kDirUp] = _x[1] > 175 && (_hitter != 1 || _hits < 2);
				}
			} else {
				if (destx < 158)
					destx = 316 - destx;
				if (destx > 277)
					destx = 554 - destx;
				destination(1, destx - rndoff, 3);
			}
		}
	} else
		destination(1, 211, 8);
}

void EE::init() {
	_rnd = 0;
	_starter = _winner = _hits = 0;
	_bvelx = _bvely = 0;
	_tbx = 200; _tby = 20;
	_bytop = 200;
	_x[0] = 64; _x[1] = 226;

	_air = false;

	_rcnt = 0;
	_tvelx = _tvely = 0;

	for (int i = 0; i < 2; i++) {
		_px[i] = _x[i];
		_y[i] = 173;
		_frameindex[i] = -1;
		_rebound[i] = 0;
		_score[i] = 0;
		_keymove[i][kDirRight] = _keymove[i][kDirLeft] = _keymove[i][kDirUp] = 0;
		_frame[i] = 0;
	}

	_oCoords = false;
	_opts = 3;
	_opt = 0;

	_mode = kModeMenu;
	_soundEnabled = false;

	_olx = _oly = _orx = _ory = _obx = _oby = 0;
	_compserve = 0;
	_sstage = 0;

	_tbx = 64 + _starter * 165;
	_pbx = _tbx;
	_bx = _tbx << 6;
	_tby = 135;
	_pby = 135;
	_by = _tby << 6;

	_server = 2 + _starter;
	_hitter = 2;
	_serve = _servevel = 1;

	_rmode = false;

	_shouldQuit = false;

	_inited = true;
}

void EE::draw(int sn, int x1, int y1) {
	int x = x1 * _scale;
	int y = y1 * _scale;

	Graphics::keyBlit((byte *)_back.getBasePtr(x, y), (const byte *)_sp[sn].getPixels(), _back.pitch, _sp[sn].pitch, _sp[sn].w, _sp[sn].h, _back.format.bytesPerPixel, _colorKey);
	g_system->copyRectToOverlay(_back.getBasePtr(x, y), _back.pitch, _windowX + x, _windowY + y, _sp[sn].w, _sp[sn].h);
}

void EE::putshapes() {
	int sprite;

	cls(false);

	if (_oCoords) {
		int obx = _obx * _scale, oby = _oby * _scale;
		int olx = _olx * _scale, oly = _oly * _scale;
		int orx = _orx * _scale, ory = _ory * _scale;
		g_system->copyRectToOverlay(_back.getBasePtr(obx, oby), _back.pitch, _windowX + obx, _windowY + oby, _sp[kSpB1].w, _sp[kSpB1].h);
		g_system->copyRectToOverlay(_back.getBasePtr(olx, oly), _back.pitch, _windowX + olx, _windowY + oly, _sp[kSpL1].w, _sp[kSpL1].h);
		g_system->copyRectToOverlay(_back.getBasePtr(orx, ory), _back.pitch, _windowX + orx, _windowY + ory, _sp[kSpR1].w, _sp[kSpR1].h);
	}

	sprite = kSpB1 + (_tbx / 16) % 4;
	draw(sprite, _tbx, _tby);
	_obx = _tbx;
	_oby = _tby;

	if (_y[0] < 173) {
		sprite = kSpL3;
	} else {
		if ((_x[0] / 8) % 2)
			sprite = kSpL1;
		else
			sprite = kSpL2;
	}
	draw(sprite, _x[0], _y[0]);
	_olx = _x[0];
	_oly = _y[0];

	if (_y[1] < 173) {
		sprite = kSpR3;
	} else {
		if ((_x[1] / 8) % 2)
			sprite = kSpR1;
		else
			sprite = kSpR2;
	}
	draw(sprite, _x[1], _y[1]);
	_orx = _x[1];
	_ory = _y[1];

	_oCoords = true;

	const int frames[] = {
		153, 106, 160, 200,
		4, 13, 6, 200,
		310, 13, 312, 200,
		4, 13, 312, 14,
		4, 199, 312, 200,
		154, 107, 159, 199
	};

	uint32 color1 = _back.format.RGBToColor(5 * 16, 7 * 16, 8 * 16);
	uint32 color2 = _back.format.RGBToColor(6 * 16, 8 * 16, 12 * 16);

	const int *ptr = frames;
	for (int i = 0; i < 6; i++, ptr += 4) {
		Common::Rect r(ptr[0] * _scale, ptr[1] * _scale, ptr[2] * _scale, ptr[3] * _scale);
		_back.fillRect(r, (i < 5 ? color1 : color2));
		g_system->copyRectToOverlay(_back.getBasePtr(r.left, r.top), _back.pitch, _windowX + r.left, _windowY + r.top, r.width(), r.height());
	}

	for (int i = 0; i < 2; i++) {
		int startx = i ? 264 : 32;

		draw(kSpNum0 + _score[i] / 10, startx, 1);
		startx += 8;
		draw(kSpNum0 + _score[i] % 10, startx, 1);
		startx += 8;
		draw(_server == i ? kSpStar : kSpSpace, startx, 1);
	}

	for (int i = 0; i < 6; i++) {
		int sx = i == 0 ? 92 : 80;
		int sy = i == 0 ? 2 : 20;

		int code = i == 0 ? kSpCode1 : i - 1 == _opt ? kSpCode1h + i : kSpCode1 + i;
		draw(code, sx, sy + i * 10);

		if (_mode != kModeMenu)
			break;
	}
}

void EE::doMenu(Common::Event &e) {
	switch (e.kbd.keycode) {
	case Common::KEYCODE_UP:
		_opt--;
		if (_opt < 0)
			_opt = 4;
		break;

	case Common::KEYCODE_DOWN:
		_opt = (_opt + 1) % 5;
		break;

	case Common::KEYCODE_RETURN:
		if (_opt < 4) {
			_opts = _opt;
			_mode = kModePlay;

			cls();
		} else {
			_shouldQuit = true;
		}
		break;

	case Common::KEYCODE_ESCAPE:
		_shouldQuit = true;
		break;
	default:
		break;
	}
}

void EE::game() {
	if (_mode == kModePlay || _mode == kModeMenu) {
		_rnd = (_rnd * 5 + 1) % 32767;

		if (_opts & 1)
			computer0();
		if (_opts & 2)
			computer1();
		if (_rmode) {
			if (_rcnt-- > 0 || _frameindex[0] != -1 || _frameindex[1] != -1) {
				resetpt();
			} else {
				_rmode = false;
				calcscore();
			}
			return;
		}

		getPos();

		if (_serve) {
			docollisions();
			_air = true;
		} else {
			if (_air) {
				docollisions();
				_air = moveball();
			}
		}
		if (!_air || _hits > 2) {
			setwinner();
			_rmode = true;
			_rcnt = 20;
		}
	}
}

void EE::playSound(int sound) {
	// FIXME
}

static const uint32 ball[21] = {
	0x0000fd00, 0x00074ac0, 0x000afde0, 0x001703e8, 0x0038f7d8, 0x0056f73c,
	0x00aef6fc, 0x00cf79fb, 0x01afbbf7, 0x01b7bdcd, 0x0077dd3a, 0x013beef3,
	0x01bbef6a, 0x015df78b, 0x00defbea, 0x00ef7dd8, 0x00779eb4, 0x0029ef68,
	0x001ef4a0, 0x000673c0, 0x0000ae00
};

static const uint32 head[38] = {
	0xbb400000, 0x0000000f, 0xea650000, 0x000003af, 0xfaaa6000, 0x00000e7f,
	0xfea5a400, 0x03ff2af0, 0x3eaa6900, 0x3ea969c0, 0x3ea99940, 0xeaaa69c8,
	0x3a669540, 0xea995540, 0x5a555550, 0xa9aa999a, 0x65a65550, 0x2aa69a5a,
	0x59555650, 0x02aaaa55, 0x6aa66550, 0x000aa950, 0x6a569950, 0x000a9500,
	0xa9595580, 0x00015001, 0xa9a55540, 0x00000015, 0xa9a9a500, 0x0000015a,
	0x59695800, 0x00001659, 0x65659000, 0x00000a96, 0xa65a8000, 0x000002aa,
	0xa6500000, 0x0000002a
};

static const uint32 legs[42] = {
	0xa0000000, 0x00000000, 0x80000000, 0x00000002, 0x80000000, 0x00000002, 0xa0000000,
	0x00000000, 0x50000000, 0x00000000, 0xf0000000, 0x00000003, 0xfc000000, 0x000003ff,

	0xa0000000, 0x00000002, 0x0a000000, 0x0000000a, 0x02400000, 0x00000028, 0x00700000,
	0x00000028, 0x00fc0000, 0x00000014, 0x03c00000, 0x000000fc, 0x0f000000, 0x0000ffff,

	0xa0000000, 0x00000000, 0x80000000, 0x00000002, 0xa8000000, 0x00000002, 0x29c00000,
	0x00000000, 0x01f00000, 0x00000000, 0x0fc00000, 0x00000000, 0xfc000000, 0x00000000
};

static const int spcolors[10 * 3] = {
	0, 0, 0,  8, 15,  8,  6, 12, 6, 12, 13, 12,
	0, 0, 0, 15,  8,  8, 12,  6, 6, 13, 12, 12,
	0, 0, 0, 12, 12, 12
};

const char *codes =
"Dvhgkm#Ztrsm|ffrs(#$%&'#$%&O}pes&}{1$M{tiq$%&'#$M{tiq${y5(Fsrv||hv%&'#$"
"Hutxxxjx'~v2%N|udr%&'#Gtsw}wiw&}{1$Hutxxxjx'#$%&'(#$%W|qw$%&'(#$%&'";

void EE::setupGraphics() {
	// Determine scale factor
	float scaleX = g_system->getOverlayWidth() * 0.9 / 320;
	float scaleY = g_system->getOverlayHeight() * 0.9 / 200;
	_scale = MIN(scaleX, scaleY);

	_windowW = 320 * _scale;
	_windowH = 200 * _scale;
	_windowX = (g_system->getOverlayWidth() > 320) ? (g_system->getOverlayWidth() - _windowW) / 2 : 0;
	_windowY = (g_system->getOverlayHeight() > 200) ? (g_system->getOverlayHeight() - _windowH) / 2 : 0;

	_format = g_system->getOverlayFormat();
	_back.create(_windowW, _windowH, _format);

	_colorBlack  = _format.RGBToColor( 0 * 16,  0 * 16,  0 * 16);
	_colorBlue   = _format.RGBToColor( 5 * 16,  7 * 16,  8 * 16);
	_colorOrange = _format.RGBToColor(15 * 16,  7 * 16,  8 * 16);
	_colorKey    = _colorBlack;

	cls();

	uint32 palette[12];
	for (int i = 0; i < 10 * 3; i += 3)
		palette[i / 3] = _back.format.RGBToColor(spcolors[i] * 16, spcolors[i + 1] * 16, spcolors[i + 2] * 16);

	Graphics::Surface tmp;
	int w = 25, h = 21;
	tmp.create(w, h, g_system->getOverlayFormat());

	for (int s = 0; s < 4; s++) {

		int posy = 0, dy = 1;
		if (s & 2) { posy = 20; dy = -1; }

		for (int y = 0; y < h; y++, posy += dy) {
			uint32 pixels = ball[y];

			int posx = 0, dx = 1;
			if (s & 1) { posx = 24; dx = -1; }

			for (int x = 0; x < w; x++, posx += dx) {
				int color = pixels & 1;

				pixels >>= 1;

				tmp.setPixel(posx, posy, palette[color + 8]);
			}
		}

		Graphics::Surface *tmp2 = tmp.scale(w * _scale, h * _scale);
		_sp[kSpB1 + s].copyFrom(*tmp2);
		tmp2->free();
		delete tmp2;
	}
	tmp.free();

	w = 32;
	h = 26;
	tmp.create(w, h, g_system->getOverlayFormat());

	for (int s = 0; s < 6; s++) {
		for (int y = 0; y < h; y++) {
			const uint32 *ptr = (y < 19) ? &head[y * 2] : &legs[(y - 19 + (s % 3) * 7) * 2];
			uint32 pixels = *ptr++;

			int posx = 0, dx = 1;
			if (s > 2) { posx = 31; dx = -1; }

			for (int x = 0; x < w; x++, posx += dx) {
				int color = pixels & 3;

				pixels >>= 2;

				if (x == 15)
					pixels = *ptr;

				tmp.setPixel(posx, y, palette[color + 4 * (s / 3)]);
			}
		}

		Graphics::Surface *tmp2 = tmp.scale(w * _scale, h * _scale);
		_sp[kSpL1 + s].copyFrom(*tmp2);
		tmp2->free();
		delete tmp2;
	}
	tmp.free();

	w = 23 * 8;
	h = 10;
	tmp.create(w, h, g_system->getOverlayFormat());
	for (int hl = 0; hl < 2; hl++) {
		for (int i = 0; i < 6; i++) {
			tmp.fillRect(Common::Rect(0, 0, w, h), hl == 1 ? _colorBlue : _colorKey);

			char buf[100];
			int c;
			for (c = 0; c < 23; c++)
				buf[c] = codes[c + 23 * i] - 3 - c % 6;
			buf[c] = 0;

			int c1 = i == 0 ? _colorOrange : hl == 1 ? _colorKey : _colorBlue;

			_font.drawString(&tmp, buf, 0, 1, w, c1, Graphics::kTextAlignLeft, 0, false);

			Graphics::Surface *tmp2 = tmp.scale(w * _scale, h * _scale, true);
			_sp[kSpCode1 + hl * 6 + i].copyFrom(*tmp2);
			tmp2->free();
			delete tmp2;
		}
	}
	tmp.free();

	w = 8;
	h = 10;
	tmp.create(w, h, g_system->getOverlayFormat());
	for (int i = 0; i < 12; i++) {
		tmp.fillRect(Common::Rect(0, 0, w, h), _colorKey);

		char buf[2];
		buf[0] = i == 10 ? '*' : i == 11 ? ' ' : '0' + i;
		buf[1] = 0;

		int c = i > 9 ? _colorBlue : _colorOrange;

		_font.drawString(&tmp, buf, 0, 1, w, c, Graphics::kTextAlignLeft, 0, false);

		Graphics::Surface *tmp2 = tmp.scale(w * _scale, h * _scale, true);
		_sp[kSpNum0 + i].copyFrom(*tmp2);
		tmp2->free();
		delete tmp2;
	}
	tmp.free();
}

} // End of namespace GUI
