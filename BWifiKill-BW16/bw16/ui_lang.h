#pragma once

// UI language selection for BWifiKill-BW16.
//
//   UI_LANG_EN = 0  Spanish (default, matches original firmware)
//   UI_LANG_EN = 1  English
//
// Override at build time (no file edits needed), e.g. with arduino-cli:
//   arduino-cli compile --build-property 'compiler.c.extra_flags=-DUI_LANG_EN=1'
// or change the default below.

#ifndef UI_LANG_EN
#define UI_LANG_EN 0
#endif

#if UI_LANG_EN
#include "ui_strings_en.h"
#else
#include "ui_strings_es.h"
#endif
