/*
	BlackjackCheat, like PokerCheat, needs UIDEBUG::_BG_DISPLAY_TEXT/
	_BG_SET_TEXT_COLOR for any HUD text -- the stock SDK's UI::DRAW_TEXT /
	UI::SET_TEXT_COLOR_RGBA are nullsub on this game build (1491.50), long
	past whatever build (<1436) they still worked on. This is a game-build
	fact, not something specific to poker_sp -- see PokerCheat's
	ExtraNatives.h/PokerCheat.cpp DrawFontTest() header comment for the full
	derivation trail (github.com/Halen84/RDR2-Native-Menu-Base confirmed
	this pair as the working replacement, build 1355+). Reopening the same
	namespace here rather than editing the vendored SDK header.

	Include this after natives.h/types.h (script.h does that ordering).
*/

#pragma once

namespace UIDEBUG
{
	static void _BG_DISPLAY_TEXT(char* text, float x, float y) { invoke<Void>(0x16794E044C9EFB58, text, x, y); }
	static void _BG_SET_TEXT_SCALE(float scaleX, float scaleY) { invoke<Void>(0xA1253A3C870B6843, scaleX, scaleY); }
	static void _BG_SET_TEXT_COLOR(int red, int green, int blue, int alpha) { invoke<Void>(0x16FA5CE47F184F1E, red, green, blue, alpha); }
}
